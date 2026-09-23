#!/usr/bin/env python3
"""Insert request-path emit timing into main.c, behind ND_PROFILE only.

Why: run #341 measured the sampler's projection at mean_n = 6.8 rows (~0.08 ms), so
the bench-invisible part of a request token is no longer attributable to the
projection. The one per-token block the boot bench structurally never runs, and that
no ND_P_* phase brackets, is the emit: nd_tok_decode_ex (SentencePiece piece -> text)
plus a per-character printf loop plus an fflush per token - and a USB CDC flush can
block until the host drains. dec_ms includes all of it, so it is inside the metric.

Diagnostic only: the ND_PROFILE guard means a shipping configure cannot see any of
it, and it is reverted with `git checkout esp32/main/main.c`.
"""

p = "esp32/main/main.c"
s = open(p).read()

# Counters at function scope, so the print after prof_dump() can see them.
old0 = """    t0 = esp_timer_get_time();
    for (i = 0; i < MAX_NEW; i++) {"""
new0 = """#ifdef ND_PROFILE
    static int64_t s_emit_tok, s_emit_wr;
#endif
    t0 = esp_timer_get_time();
    for (i = 0; i < MAX_NEW; i++) {"""

old1 = """        nd_tok_decode_ex(&s_model.tok, &id, 1, piece, sizeof(piece), 0);
        printf("TOK ");"""
new1 = """#ifdef ND_PROFILE
        /* Split the emit into the tokenizer's piece->text call and the console
         * write, which are different levers. */
        int64_t e_emit = esp_timer_get_time();
#endif
        nd_tok_decode_ex(&s_model.tok, &id, 1, piece, sizeof(piece), 0);
#ifdef ND_PROFILE
        { int64_t e_tok = esp_timer_get_time(); s_emit_tok += e_tok - e_emit; e_emit = e_tok; }
#endif
        printf("TOK ");"""

old2 = """        printf("\\n");
        fflush(stdout);
"""
new2 = """        printf("\\n");
        fflush(stdout);
#ifdef ND_PROFILE
        s_emit_wr += esp_timer_get_time() - e_emit;
#endif
"""

old3 = """#ifdef ND_PROFILE
        prof_dump(dec_ms, (int)produced);
#endif"""
new3 = """#ifdef ND_PROFILE
        prof_dump(dec_ms, (int)produced);
        if (produced && dec_ms > 0.0) {
            printf("EVT prof %-10s %8.1f ms  %5.1f%%\\n", "tok-piece",
                   s_emit_tok / 1000.0 / produced, 100.0 * s_emit_tok / 1000.0 / dec_ms);
            printf("EVT prof %-10s %8.1f ms  %5.1f%%\\n", "tok-emit",
                   s_emit_wr / 1000.0 / produced, 100.0 * s_emit_wr / 1000.0 / dec_ms);
        }
        s_emit_tok = s_emit_wr = 0;
#endif"""

for old, new in ((old0, new0), (old1, new1), (old2, new2), (old3, new3)):
    assert s.count(old) == 1, "site not found: " + old.strip().splitlines()[0]
    s = s.replace(old, new, 1)

open(p, "w").write(s)
print("emit timing inserted (ND_PROFILE only); revert with git checkout esp32/main/main.c")
