#include "nd_sample.h"
#include "nd_quant.h"

#include <string.h>

#define ND_LEX_BITS 32u
#define ND_LEX_MAX_WORDS 256u     /* sized for this archive's 8192-id vocabulary */


/* ---- first-byte bucket index over the vocabulary --------------------------
 *
 * The constrained sampler has to know which pieces the current grammar state can
 * spell. The direct answer - ask every one of the 8,176 vocabulary pieces - costs
 * a piece lookup plus a table test per id, per decode token. Measured against the
 * real thing (all six primary prompts, 93 decode steps, `.auto/exp16/SCREEN.md`),
 * the grammar allows a *median of one first byte* per step, and only 94 of the 256
 * byte values ever start a piece, so 98.4 % of that walk is spent rejecting ids
 * that could never be legal: 51 ids touched per step on average, against 8,176
 * examined.
 *
 * So index the vocabulary by first byte once - ascending ids grouped per byte, as
 * a counting sort, which needs no comparison sort and leaves each bucket already
 * in id order - and per token walk only the buckets whose byte the state accepts.
 * Legality itself is unchanged: every id visited still takes the same `token_ok`
 * byte walk, and results are still written into the same one-bit-per-id bitmap that
 * the caller then reads in ascending order, so the candidate list, the argmax and
 * its tie-breaking are the shipped ones. The index reproduces the shipped
 * enumeration element for element on every captured step.
 *
 * This is deliberately *not* the run #149 attempt, which kept all 8,176 iterations
 * and only made each one cheaper (it measured -0.17 %). Removing 98 % of the
 * iterations is a different change, and it also drops the two-core split this walk
 * needed when it was 8,176 ids deep (run #176): 51 ids do not pay for a handshake.
 *
 * Lives in PSRAM next to the weights it indexes (~17 KB of the 2 MB free); the
 * 15 KB of internal RAM left is not a place to put it. If the allocation fails the
 * sampler uses the original full walk, so the index is an optimisation, not a
 * dependency. */
#define ND_LEX_MAX_VOCAB 65536u    /* ids are stored uint16_t */

typedef struct {
    uint16_t *id;        /* vocabulary ids, grouped by first byte, ascending inside a group */
    uint32_t *off;       /* off[257]: group b is id[off[b] .. off[b+1])                     */
    uint8_t  *present;   /* the byte values with a non-empty group                          */
    uint32_t  npresent;
    uint32_t  n;         /* indexed ids                                                     */
    const nd_tokenizer *tok;
    uint32_t  vocab;
    void     *block;
} lex_index;

static lex_index s_lx;

static int lex_index_build(const nd_tokenizer *tok, uint32_t vocab)
{
    uint32_t cnt[256];
    uint32_t b, j, n = 0;
    size_t   need;
    uint8_t *blk;

    if (vocab < 16u || vocab > ND_LEX_MAX_VOCAB)
        return -1;
    need = (size_t)vocab * sizeof(uint16_t) + 257u * sizeof(uint32_t) + 256u;
    blk  = (uint8_t *)ND_ALLOC(need);
    if (!blk)
        return -1;

    memset(cnt, 0, sizeof(cnt));
    for (j = 16u; j < vocab; j++) {
        uint16_t     plen;
        const char  *pc = nd_tok_piece(tok, j, &plen);
        if (!pc || plen == 0u)
            continue;                       /* the walk rejects these too */
        cnt[(unsigned char)pc[0]]++;
        n++;
    }
    s_lx.id      = (uint16_t *)blk;
    s_lx.off     = (uint32_t *)(blk + (size_t)vocab * sizeof(uint16_t));
    s_lx.present = (uint8_t *)(s_lx.off + 257u);
    s_lx.block   = blk;
    s_lx.n       = n;

    s_lx.off[0] = 0u;
    for (b = 1; b < 257u; b++)
        s_lx.off[b] = s_lx.off[b - 1] + cnt[b - 1u];
    for (j = 16u; j < vocab; j++) {         /* ids ascend, so each group ends ascending */
        uint16_t    plen;
        const char *pc = nd_tok_piece(tok, j, &plen);
        if (!pc || plen == 0u)
            continue;
        s_lx.id[s_lx.off[(unsigned char)pc[0]]++] = (uint16_t)j;
    }
    for (b = 256u; b > 0u; b--)             /* off[b] advanced to its end; restore ends  */
        s_lx.off[b] = s_lx.off[b - 1u];
    s_lx.off[0] = 0u;
    s_lx.npresent = 0u;
    for (b = 0; b < 256u; b++)
        if (s_lx.off[b + 1u] > s_lx.off[b])
            s_lx.present[s_lx.npresent++] = (uint8_t)b;
    s_lx.tok   = tok;
    s_lx.vocab = vocab;
    return 0;
}

/* Visit only the groups the state can start a piece with. Same predicate, same
 * bitmap, one core: the whole pass is ~100 grammar steps plus ~50 byte walks. */
static int token_ok(const nd_sampler *s, uint32_t id, nd_gstate *out);

static void lex_buckets(const nd_sampler *s, uint32_t *bits, uint32_t words)
{
    uint32_t i, k;

    memset(bits, 0, (size_t)words * sizeof(uint32_t));
    for (i = 0; i < s_lx.npresent; i++) {
        uint32_t    b = s_lx.present[i];
        nd_gstate   trial = s->st;
        uint32_t    e;

        if (!nd_gstate_byte(&trial, (char)b))
            continue;
        for (k = s_lx.off[b], e = s_lx.off[b + 1u]; k < e; k++) {
            uint32_t j = s_lx.id[k];
            if (token_ok(s, j, NULL))
                bits[j / ND_LEX_BITS] |= 1u << (j % ND_LEX_BITS);
        }
    }
}

void nd_sampler_init(nd_sampler *s, const nd_tokenizer *tok,
                     const nd_grammar *g)
{
    memset(s, 0, sizeof(*s));
    s->tok = tok;
    nd_gstate_init(&s->st, g);
}

/* Can the grammar accept every byte of this token's surface? */
static int token_ok(const nd_sampler *s, uint32_t id, nd_gstate *out)
{
    nd_gstate    trial = s->st;
    uint16_t     len;
    const char  *surf;
    uint16_t     i;

    surf = nd_tok_piece(s->tok, id, &len);
    if (!surf)
        return 0;

    for (i = 0; i < len; i++) {
        /* SentencePiece's space marker is U+2581; inside a JSON call the
         * model emits real bytes, so a marker byte here is not legal. */
        if (!nd_gstate_byte(&trial, surf[i]))
            return 0;
    }
    if (out)
        *out = trial;
    return 1;
}

uint32_t nd_sample(nd_sampler *s, const float *logits, uint32_t vocab)
{
    uint32_t best = (uint32_t)-1;
    float    bv = -1e30f;
    uint32_t j;

    if (!s->engaged) {
        /* Unconstrained: plain argmax over the whole vocabulary. */
        for (j = 0; j < vocab; j++)
            if (logits[j] > bv) { bv = logits[j]; best = j; }
        return best;
    }

    /* Once the call is complete only </tool_call> may follow. */
    if (nd_gstate_complete(&s->st))
        return ND_TOOL_CALL_END_ID;

    for (j = 0; j < vocab; j++) {
        if (logits[j] <= bv)
            continue;
        /* Control pieces carry no bytes and would slip through the byte
         * check, so exclude them explicitly while constrained. */
        if (j < 16)
            continue;
        if (!token_ok(s, j, NULL))
            continue;
        bv = logits[j];
        best = j;
    }
    return best;
}

/* Candidate legality, computed one word (32 vocabulary ids) at a time.
 *
 * This is the walk over the whole vocabulary that decides which pieces the
 * grammar can spell, and it costs ~1.6 ms per decode token, measured. It runs
 * while the second core has nothing to do - the next token's projections depend
 * on the token being sampled here - so splitting it is free capacity.
 *
 * Rows are whole words, so each core writes only words inside its own range and
 * builds its own 256-entry first-byte table on its own stack: no shared scratch,
 * no barrier needed (see the nd_parallel_rows contract in nd_quant.h). The
 * predicate is the one the inline walk used; token_ok() takes the sampler const
 * and copies the grammar state, so it is pure. Ids below 16 are the tokenizer's
 * control range and stay illegal here, and a zero-length piece stays illegal
 * even though token_ok() alone would accept it.
 */
typedef struct {
    const nd_sampler *s;
    uint32_t         *bits;       /* one bit per vocabulary id */
    uint32_t          w0;         /* first word of this chunk */
    uint32_t          vocab;
} lex_ctx;

static ND_HOT void lex_words(void *vc, uint32_t wrel0, uint32_t wrel1)
{
    const lex_ctx *c = (const lex_ctx *)vc;
    uint8_t        first_ok[256];
    uint32_t       w, b;

    for (b = 0; b < 256u; b++) {
        nd_gstate trial = c->s->st;
        first_ok[b] = nd_gstate_byte(&trial, (char)b) ? 1u : 0u;
    }

    for (w = wrel0; w < wrel1; w++) {
        uint32_t word = 0u, i;
        for (i = 0; i < ND_LEX_BITS; i++) {
            uint32_t     j = (c->w0 + w) * ND_LEX_BITS + i;
            uint16_t     plen;
            const char  *piece;

            if (j < 16u || j >= c->vocab)
                continue;
            piece = nd_tok_piece(c->s->tok, j, &plen);
            if (!piece || plen == 0u || !first_ok[(unsigned char)piece[0]])
                continue;
            if (!token_ok(c->s, j, NULL))
                continue;
            word |= 1u << i;
        }
        c->bits[c->w0 + w] = word;
    }
}

uint32_t nd_sample_hidden(nd_model *m, nd_sampler *s, const float *hidden)
{
    static uint32_t cand[ND_SAMPLE_MAX_CAND];
    static float    score[ND_SAMPLE_MAX_CAND];
    uint32_t        n = 0, j, best = (uint32_t)-1;
    float           bv = -1e30f;

    /* Unconstrained (the <think> block): plain argmax over everything. */
    if (!s->engaged)
        return nd_sample(s, nd_model_logits_all(m, hidden), m->vocab);

    if (nd_gstate_complete(&s->st))
        return ND_TOOL_CALL_END_ID;

    /* Enumerate what the grammar allows.
     *
     * The straightforward walk asks the grammar about every byte of every
     * piece: ~25K byte steps per token, and on device that measured 14.6 ms of
     * a 207 ms decode token (7%) - far more than its share of the arithmetic,
     * because nd_gstate_byte is not a cheap predicate. Memoizing the whole list
     * on the grammar state does nothing (the state changes on essentially every
     * accepted token, so it never repeats).
     *
     * What does work: almost every piece is rejected by its FIRST byte, so
     * resolve byte 0 once per byte value for the current state - 256 grammar
     * steps, ~0.1 ms - and let the table veto candidates before any grammar
     * call. A piece whose first byte cannot be consumed from this state can
     * never be legal, so the survivors (which still take the full byte walk)
     * are exactly the set the old loop produced: no numerical change, and the
     * argmax and its tie-breaking are untouched. */
    {
        static uint32_t s_legal[ND_LEX_MAX_WORDS];
        lex_ctx    lc;
        uint32_t   base = 0u, w;
        uint32_t   all_words = (m->vocab + ND_LEX_BITS - 1u) / ND_LEX_BITS;

        /* The index is worth building only if the bitmap can hold the whole
         * vocabulary in one pass; beyond that the chunked walk is used. */
        if (all_words <= ND_LEX_MAX_WORDS) {
            if (s_lx.vocab != m->vocab || s_lx.tok != &m->tok) {
                if (lex_index_build(&m->tok, m->vocab) != 0) {
                    s_lx.vocab = 0u;         /* fall back, and do not retry per token */
                    s_lx.tok   = &m->tok;
                }
            }
            if (s_lx.vocab == m->vocab && s_lx.tok == &m->tok) {
                lex_buckets(s, s_legal, all_words);
                goto collected;
            }
        }

        lc.s = s; lc.bits = s_legal; lc.vocab = m->vocab;
        /* Chunked so a vocabulary larger than the static bitmap still works: one
         * split for this archive's 8192 ids, more for a hypothetical bigger one. */
        while (base < m->vocab) {
            uint32_t words = (m->vocab - base + ND_LEX_BITS - 1u) / ND_LEX_BITS;
            if (words > ND_LEX_MAX_WORDS)
                words = ND_LEX_MAX_WORDS;
            lc.w0 = base / ND_LEX_BITS;
            nd_parallel_rows(lex_words, &lc, words);
            base += words * ND_LEX_BITS;
        }

collected:
        /* Ascending id order, so the argmax below sees exactly the candidate list
         * - and therefore the tie-break - the serial walk used to produce. The
         * count is complete rather than cut off at the cap, which is equivalent:
         * over the cap both forms take the full-vocabulary path. */
        for (w = 0; w * ND_LEX_BITS < m->vocab; w++) {
            uint32_t word = s_legal[w];
            while (word) {
                uint32_t i = (uint32_t)__builtin_ctz(word);
                word &= word - 1u;
                if (n < ND_SAMPLE_MAX_CAND)
                    cand[n] = w * ND_LEX_BITS + i;
                n++;
            }
        }
    }

    if (n == 0)
        return (uint32_t)-1;
    if (n > ND_SAMPLE_MAX_CAND)                 /* too many: full projection */
        return nd_sample(s, nd_model_logits_all(m, hidden), m->vocab);

    nd_model_logits_subset(m, hidden, cand, n, score);
    for (j = 0; j < n; j++)
        if (score[j] > bv) { bv = score[j]; best = cand[j]; }
    return best;
}

void nd_sample_accept(nd_sampler *s, uint32_t id)
{
    if (id == ND_TOOL_CALL_START_ID) {
        s->engaged = 1;
        nd_gstate_open(&s->st);
        return;
    }
    if (id == ND_TOOL_CALL_END_ID) {
        s->engaged = 0;
        s->finished = 1;
        return;
    }
    if (s->engaged) {
        nd_gstate trial;
        if (token_ok(s, id, &trial))
            s->st = trial;
    }
}
