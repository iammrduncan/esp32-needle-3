#include "nd_quant.h"

#include <math.h>
#include <string.h>

float nd_f16_slow(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp  = (h >> 10) & 0x1Fu;
    uint32_t man  = h & 0x3FFu;
    uint32_t bits;

    if (exp == 0) {
        if (man == 0) {
            bits = sign; /* +-0 */
        } else {
            /* Subnormal: renormalise into a float32 exponent. */
            exp = 127 - 15 + 1;
            while ((man & 0x400u) == 0) {
                man <<= 1;
                exp--;
            }
            man &= 0x3FFu;
            bits = sign | (exp << 23) | (man << 13);
        }
    } else if (exp == 0x1F) {
        bits = sign | 0x7F800000u | (man << 13); /* inf / nan */
    } else {
        bits = sign | ((exp + 127 - 15) << 23) | (man << 13);
    }

    {
        float f;
        memcpy(&f, &bits, 4);
        return f;
    }
}

/* Two butterflies per memory instruction, with the same scalar add.s/sub.s the
 * generic loop uses. Measured (run #359 lane 1, on the real 6-group x 128
 * geometry, diffed cell-by-cell against the generic body below): +36.75 %.
 *
 * This is NOT float SIMD - the ESP32-S3 assembler rejects every vector float
 * arithmetic opcode this core might have wanted (ee.add.64 / ee.adds.64 /
 * ee.sub*.64 / ee.mul*.64 do not exist to it, which is what
 * engine/src/lut2_tie728.S:24 already records) - it is only fewer LSU
 * instructions around identical arithmetic, so the bits cannot move: each cell
 * still computes a+b and a-b over its own two operands, in the same order.
 *
 * Why ee.ldf is admissible here when run #335 found it returning the wrong
 * values: that defect was in the 2-bit *decoder* pattern, where the nibble
 * identity was lost upstream of the loads. On plain aligned fp32 arrays the wide
 * load passes both a bit-differential against the shipping kernel and a
 * known-answer probe (#356). The differential is the gate, not the instruction.
 *
 * `.ip` post-updates its base register: the access uses the old value and then
 * the register advances. The load and store cursors are therefore deliberately
 * DIFFERENT variables - one variable reused for both would write 16 bytes past
 * the pair it just read (that bug is in run #359's history). */
#if defined(__XTENSA__)
ND_HOT static void fwht_stages_pairs(float *x, uint32_t n)
{
    uint32_t len, i, j;

    for (len = 2; len < n; len <<= 1) {
        for (i = 0; i < n; i += len << 1) {
            for (j = i; j < i + len; j += 2u) {
                float *pa = &x[j],       *pb = &x[j + len];
                float *qa = &x[j],       *qb = &x[j + len];
                __asm__ __volatile__(
                    "ee.ldf.64.ip  f4, f5,  %[_a], 8\n\t"
                    "ee.ldf.64.ip  f6, f7,  %[_b], 8\n\t"
                    "add.s  f8,  f4, f6\n\t"
                    "add.s  f10, f5, f7\n\t"
                    "sub.s  f12, f4, f6\n\t"
                    "sub.s  f14, f5, f7\n\t"
                    "ee.stf.64.ip  f8,  f10, %[_c], 8\n\t"
                    "ee.stf.64.ip  f12, f14, %[_d], 8\n\t"
                    : [_a] "+a"(pa), [_b] "+a"(pb), [_c] "+a"(qa), [_d] "+a"(qb)
                    :
                    : "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11",
                      "f12", "f13", "f14", "f15", "memory");
            }
        }
    }
}
#endif

ND_HOT void nd_fwht(float *x, uint32_t n)
{
    uint32_t len;

#if defined(__XTENSA__)
    /* The pair form needs 8-byte alignment of both pair bases. For an even len
     * that is exactly `x` being 8-aligned; the len == 1 stage is left scalar
     * because its "b" operand lives inside the same pair as its "a" operand. */
    if (n >= 2u && (n & (n - 1u)) == 0u && (((uintptr_t)x) & 7u) == 0u) {
        /* The WHOLE len == 1 stage: n/2 butterflies, one per block. Shipping the
         * first one alone was measured on device as DEVICE_OUTPUT_DIVERGED 4/17
         * with token_delta 257 (#362) - and the kbench differential could not see
         * it, because the bench copy looped correctly while this copy did not.
         * Differential the code that ships, never a transcription of it. */
        uint32_t i;
        for (i = 0; i < n; i += 2u) {
            float a = x[i], b = x[i + 1u];
            x[i]           = a + b;
            x[i + 1u]      = a - b;
        }
        fwht_stages_pairs(x, n);
        return;
    }
#endif

    for (len = 1; len < n; len <<= 1) {
        uint32_t i;
        for (i = 0; i < n; i += len << 1) {
            uint32_t j;
            for (j = i; j < i + len; j++) {
                float a = x[j];
                float b = x[j + len];
                x[j]       = a + b;
                x[j + len] = a - b;
            }
        }
    }
}

uint32_t nd_cq_scratch(const nd_tensor *t)
{
    return nd_cq_in_pad(t);
}

typedef struct { const float *cb, *xh; float *lut; } lutb_ctx;

static ND_HOT void lutb_rows(void *vc, uint32_t p0, uint32_t p1)
{
    const lutb_ctx *c = (const lutb_ctx *)vc;
    uint32_t        p;
    /* Each pair's 16 entries depend only on that pair's two activation values,
     * so pairs split across cores and every entry is the same expression,
     * computed the same way, as in the sequential build. */
    for (p = p0; p < p1; p++) {
        float  x0 = c->xh[2 * p], x1 = c->xh[2 * p + 1];
        float  a0 = c->cb[0] * x0, a1 = c->cb[1] * x0;
        float  a2 = c->cb[2] * x0, a3 = c->cb[3] * x0;
        float  b0 = c->cb[0] * x1, b1 = c->cb[1] * x1;
        float  b2 = c->cb[2] * x1, b3 = c->cb[3] * x1;
        float *T  = c->lut + (size_t)p * 16;
        T[0]  = a0 + b0; T[1]  = a1 + b0; T[2]  = a2 + b0; T[3]  = a3 + b0;
        T[4]  = a0 + b1; T[5]  = a1 + b1; T[6]  = a2 + b1; T[7]  = a3 + b1;
        T[8]  = a0 + b2; T[9]  = a1 + b2; T[10] = a2 + b2; T[11] = a3 + b2;
        T[12] = a0 + b3; T[13] = a1 + b3; T[14] = a2 + b3; T[15] = a3 + b3;
    }
}

typedef struct { float *xh; uint32_t g; float scale; } fwht_ctx;

static ND_HOT void fwht_rows(void *vc, uint32_t g0, uint32_t g1)
{
    const fwht_ctx *c = (const fwht_ctx *)vc;
    uint32_t        gi;
    for (gi = g0; gi < g1; gi++) {
        float   *blk = c->xh + (size_t)gi * c->g;
        nd_fwht(blk, c->g);
        /* Unrolled by 4: measured +263 % on the shipped geometry (#359 lane 2,
         * 10,789 -> 2,968 cycles for 6 groups of 128, bit-exact because every
         * element is its own multiply and there is no accumulation order to
         * move). `restrict` alone measured exactly zero (#356 lane 3), which is
         * why the annotation is not the change - the unroll is. */
        {
            uint32_t j = 0u;
            float   *restrict b4 = blk;
            for (; j + 3u < c->g; j += 4u) {
                float v0 = b4[j], v1 = b4[j + 1u], v2 = b4[j + 2u], v3 = b4[j + 3u];
                b4[j] = v0 * c->scale; b4[j + 1u] = v1 * c->scale;
                b4[j + 2u] = v2 * c->scale; b4[j + 3u] = v3 * c->scale;
            }
            for (; j < c->g; j++) b4[j] *= c->scale;
        }
    }
}

void nd_cq_prepare(const nd_tensor *t, const float *x, float *xh)
{
    uint32_t in_pad = nd_cq_in_pad(t);
    uint32_t g      = t->group;
    uint32_t ngroup = in_pad / g;
    float    scale  = 1.0f / sqrtf((float)g);

    memcpy(xh, x, t->shape[1] * sizeof(float));
    if (in_pad > t->shape[1])
        memset(xh + t->shape[1], 0, (in_pad - t->shape[1]) * sizeof(float));

    {
        /* Groups are independent: the transform + rescale splits by group.
         * The copy/pad above stays on the calling core - it is one sequential
         * read of the activation. */
        fwht_ctx fc = { xh, g, scale };
        nd_parallel_rows(fwht_rows, &fc, ngroup);
    }
}

typedef struct {
    const uint8_t  *packed;
    const uint16_t *norms;
    const float    *xh, *cb;
    float          *y;
    uint32_t        ngroup, g, bits, rowbytes, base;
} gemv_ctx;

/* Sum over one group of cb[idx] * xh[j], for the common packings.
 * Indices are an LSB-first bitstream per row: index k occupies bits
 * [k*bits, (k+1)*bits). */
static ND_HOT float dot_group(const uint8_t *p, uint32_t bit_off, uint32_t bits,
                       uint32_t g, const float *cb, const float *xh)
{
    float    s = 0.0f;
    uint32_t j;

    /* Four independent accumulators: a single running sum serialises on the
     * FPU's add latency, which dominates this loop on an LX7. */
    if (bits == 2 && (bit_off & 7u) == 0) {
        const uint8_t *q = p + (bit_off >> 3);
        float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
        /* Four packed bytes (16 weights) per 32-bit load. Group starts are
         * byte-aligned and g is a multiple of 8, so q is 4-aligned. Same
         * accumulator, same index order per accumulator as the byte-wise loop. */
        for (j = 0; j < g; j += 16) {
            uint32_t w = ((const uint32_t *)(const void *)q)[0];
            q += 4;
            s0 += cb[ w        & 3u] * xh[j];
            s1 += cb[(w >>  2) & 3u] * xh[j + 1];
            s2 += cb[(w >>  4) & 3u] * xh[j + 2];
            s3 += cb[(w >>  6) & 3u] * xh[j + 3];
            s0 += cb[(w >>  8) & 3u] * xh[j + 4];
            s1 += cb[(w >> 10) & 3u] * xh[j + 5];
            s2 += cb[(w >> 12) & 3u] * xh[j + 6];
            s3 += cb[(w >> 14) & 3u] * xh[j + 7];
            s0 += cb[(w >> 16) & 3u] * xh[j + 8];
            s1 += cb[(w >> 18) & 3u] * xh[j + 9];
            s2 += cb[(w >> 20) & 3u] * xh[j + 10];
            s3 += cb[(w >> 22) & 3u] * xh[j + 11];
            s0 += cb[(w >> 24) & 3u] * xh[j + 12];
            s1 += cb[(w >> 26) & 3u] * xh[j + 13];
            s2 += cb[(w >> 28) & 3u] * xh[j + 14];
            s3 += cb[ w >> 30]       * xh[j + 15];
        }
        return (s0 + s1) + (s2 + s3);
    }
    if (bits == 4 && (bit_off & 7u) == 0) {
        const uint8_t *q = p + (bit_off >> 3);
        float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
        /* Eight weights per 32-bit load. Group starts are byte-aligned and g
         * is a multiple of 8, so q is 4-aligned. Each accumulator sees the same
         * indices in the same order as the byte-wise loop. */
        for (j = 0; j < g; j += 8) {
            uint32_t w = ((const uint32_t *)(const void *)q)[0];
            q += 4;
            s0 += cb[ w        & 15u] * xh[j];
            s1 += cb[(w >>  4) & 15u] * xh[j + 1];
            s2 += cb[(w >>  8) & 15u] * xh[j + 2];
            s3 += cb[(w >> 12) & 15u] * xh[j + 3];
            s0 += cb[(w >> 16) & 15u] * xh[j + 4];
            s1 += cb[(w >> 20) & 15u] * xh[j + 5];
            s2 += cb[(w >> 24) & 15u] * xh[j + 6];
            s3 += cb[ w >> 28]        * xh[j + 7];
        }
        return (s0 + s1) + (s2 + s3);
    }

    /* Generic bit reader (covers bits == 3, and any unaligned start). Only
     * fetches the second byte when the field actually straddles, so the last
     * index of the last row never reads past the blob. */
    for (j = 0; j < g; j++) {
        uint32_t b   = bit_off + j * bits;
        uint32_t byt = b >> 3;
        uint32_t sh  = b & 7u;
        uint32_t w   = p[byt];
        if (sh + bits > 8)
            w |= (uint32_t)p[byt + 1] << 8;
        s += cb[(w >> sh) & ((1u << bits) - 1u)] * xh[j];
    }
    return s;
}

/* Row-range variant. `base` shifts the tensor row that output i maps to, so
 * the mHC phi tensors (all 27 layers stacked into one tensor) can hand out a
 * per-layer slice and still be split across cores. */
static ND_HOT void gemv_rows_offset(void *vc, uint32_t i0, uint32_t i1)
{
    const gemv_ctx *c = (const gemv_ctx *)vc;
    uint32_t        i;

    for (i = i0; i < i1; i++) {
        uint32_t        r   = c->base + i;
        const uint8_t  *row = c->packed + (size_t)r * c->rowbytes;
        const uint16_t *nrm = c->norms + (size_t)r * c->ngroup;
        float           acc = 0.0f;
        uint32_t        gi;

        for (gi = 0; gi < c->ngroup; gi++)
            acc += nd_f16(nrm[gi]) *
                   dot_group(row, gi * c->g * c->bits, c->bits, c->g, c->cb,
                             c->xh + (size_t)gi * c->g);
        c->y[i] = acc;
    }
}

#if ND_GEMV4_ASM
/* The 4-bit kernel is specialised to group 128 (16 index words, 512-byte xh
 * stride per group) and does the inline FP16->FP32 conversion only, so no norm in
 * range may touch nd_f16_slow's subnormal/inf path - the same restriction
 * nd_lut2_asm_ok() applies. bits must be 4: the body reads two nibbles per word. */
int nd_gemv4_asm_ok(uint32_t bits, uint32_t g, uint32_t ngroup, uint32_t rows,
                    const uint16_t *norms)
{
    uint32_t i, n = ngroup * rows;

    if (bits != 4u || g != 128u || ngroup == 0u || rows == 0u)
        return 0;
    for (i = 0; i < n; i++) {
        uint32_t e = ((const uint16_t *)norms)[i] >> 10 & 0x1fu;
        if (e == 0u || e == 31u)
            return 0;
    }
    return 1;
}

/* One split of a 4-bit projection through the handwritten row walker. All the
 * cursor arithmetic the C walker does per row is resolved here once, which is why
 * the kernel body itself only adds. */
static void gemv_rows_offset_asm(void *vc, uint32_t i0, uint32_t i1)
{
    const gemv_ctx *c = (const gemv_ctx *)vc;
    nd_gemv4_ctx    a;
    uint32_t        r0 = c->base + i0;

    a.packed0  = c->packed + (size_t)r0 * c->rowbytes;
    a.norms0   = c->norms + (size_t)r0 * c->ngroup;
    a.xh       = c->xh;
    a.cb       = c->cb;
    a.y0       = c->y + i0;
    a.ngroup   = c->ngroup;
    a.g        = c->g;
    a.rowbytes = c->rowbytes;
    a.normstep = c->ngroup * 2u;
    a.nrows    = i1 - i0;
    nd_gemv4_rows_tie1(&a);
}

/* nd_gemv4_asm_ok() walks the tensor's norms and a projection runs once per token,
 * so the answer is remembered per (blob, rows) like the 2-bit kernel's. Four
 * entries, because the mHC phi stage sweeps three tensors back to back and a
 * single-entry cache would re-walk all three every token. A 2-bit tensor's answer
 * is not cached - it is rejected on the spot and costs three compares. */
static struct { const void *blob; uint32_t rows; int ok; } s_g4[4];
static uint32_t s_g4_next;

static int gemv4_asm_usable(const gemv_ctx *c, const void *blob, uint32_t rows)
{
    uint32_t i;
    int      ok;

    for (i = 0; i < 4; i++)
        if (s_g4[i].blob == blob && s_g4[i].rows == rows)
            return s_g4[i].ok;
    if (c->bits != 4u)
        return 0;
    ok = nd_gemv4_asm_ok(c->bits, c->g, c->ngroup, rows, c->norms);
    s_g4[s_g4_next].blob = blob;
    s_g4[s_g4_next].rows = rows;
    s_g4[s_g4_next].ok   = ok;
    s_g4_next            = (s_g4_next + 1u) & 3u;
    return ok;
}

static nd_row_fn gemv4_pick(const gemv_ctx *c, const void *blob, uint32_t rows)
{
    return gemv4_asm_usable(c, blob, rows) ? gemv_rows_offset_asm
                                          : gemv_rows_offset;
}
#else
static nd_row_fn gemv4_pick(const gemv_ctx *c, const void *blob, uint32_t rows)
{
    (void)c; (void)blob; (void)rows;
    return gemv_rows_offset;
}
#endif

ND_HOT void nd_cq_gemv_rows(const nd_cact *c, const nd_tensor *t, const void *blob,
                     const float *xh, uint32_t r0, uint32_t nrows, float *y)
{
    const uint8_t *packed   = (const uint8_t *)blob;
    uint32_t       out      = t->shape[0];
    uint32_t       rowbytes = nd_cq_row_bytes(t);
    gemv_ctx       ctx;

    ctx.packed   = packed;
    ctx.norms    = (const uint16_t *)(const void *)(packed +
                       (size_t)out * rowbytes);
    ctx.xh       = xh;
    ctx.cb       = nd_cact_codebook(c, t->bits);
    ctx.y        = y;
    ctx.ngroup   = nd_cq_groups(t);
    ctx.g        = t->group;
    ctx.bits     = t->bits;
    ctx.rowbytes = rowbytes;
    ctx.base     = r0;

    nd_parallel_rows(gemv4_pick(&ctx, blob, nrows), &ctx, nrows);
}




static ND_HOT void gemv_rows_generic(void *vc, uint32_t r0, uint32_t r1)
{
    const gemv_ctx *c = (const gemv_ctx *)vc;
    uint32_t        r;

    for (r = r0; r < r1; r++) {
        const uint8_t  *row = c->packed + (size_t)r * c->rowbytes;
        const uint16_t *nrm = c->norms + (size_t)r * c->ngroup;
        float           acc = 0.0f;
        uint32_t        gi;

        for (gi = 0; gi < c->ngroup; gi++)
            acc += nd_f16(nrm[gi]) *
                   dot_group(row, gi * c->g * c->bits, c->bits, c->g, c->cb,
                             c->xh + (size_t)gi * c->g);
        c->y[r] = acc;
    }
}

ND_HOT void nd_cq_gemv_prepared(const nd_cact *c, const nd_tensor *t, const void *blob,
                         const float *xh, float *y)
{
    const uint8_t *packed   = (const uint8_t *)blob;
    uint32_t       out      = t->shape[0];
    uint32_t       rowbytes = nd_cq_row_bytes(t);
    gemv_ctx       ctx;

    ctx.packed   = packed;
    ctx.norms    = (const uint16_t *)(const void *)(packed +
                       (size_t)out * rowbytes);
    ctx.xh       = xh;
    ctx.cb       = nd_cact_codebook(c, t->bits);
    ctx.y        = y;
    ctx.ngroup   = nd_cq_groups(t);
    ctx.g        = t->group;
    ctx.bits     = t->bits;
    ctx.rowbytes = rowbytes;

    nd_parallel_rows(gemv_rows_generic, &ctx, out);
}

ND_HOT void nd_cq_lut_build(const nd_cact *c, const float *xh, uint32_t in_pad,
                            float *lut)
{
    lutb_ctx bc = { nd_cact_codebook(c, 2), xh, lut };

    /* T[i0 | (i1 << 2)] = cb[i0]*xh[2p] + cb[i1]*xh[2p+1], matching the
     * LSB-first packing (the low 2 bits of a nibble are the earlier weight).
     * Split by pair range: 512 independent 16-float entries at in_pad=1024, so
     * the table the GEMV is about to read in full is built by both cores. */
    nd_parallel_rows(lutb_rows, &bc, in_pad / 2);
}

/* One group's contribution, eight pairs (16 weights) per 32-bit load.
 *
 * The packed 2-bit stream pairs weight i with weight i + g/2, so pair p lives
 * at byte p/4 nibble (p%4) and the four nibbles of a word are pairs
 * 4t, 4t+1, 4t+2, 4t+3. Each nibble indexes its own 16-entry block of the
 * table, so reading a whole word and shifting it is the same four lookups the
 * byte walk did, in the same order - and it quarters the loads on the row,
 * which is the only stream this kernel cannot keep in a register.
 * g is a multiple of 16 and the row is group-aligned, so the word read is
 * in-bounds and 4-aligned. */
static ND_HOT float dot_group_lut2(const uint8_t *q, const float *T, uint32_t g)
{
    float    s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
    uint32_t j;

    for (j = 0; j < g; j += 16) {
        uint32_t w = ((const uint32_t *)(const void *)q)[0];
        q += 4;
        s0 += T[          w  & 15u];
        s1 += T[16 + ((w >>  4) & 15u)];
        s2 += T[32 + ((w >>  8) & 15u)];
        s3 += T[48 + ((w >> 12) & 15u)];
        T += 64;                      /* 4 pairs consumed */
        s0 += T[          (w >> 16) & 15u];
        s1 += T[16 + ((w >> 20) & 15u)];
        s2 += T[32 + ((w >> 24) & 15u)];
        s3 += T[48 +  (w >> 28)];
        T += 64;                      /* next 4 pairs */
    }
    return (s0 + s1) + (s2 + s3);
}

/* Default splitter: run everything on the calling thread. */
static void rows_serial(nd_row_fn fn, void *ctx, uint32_t nrows)
{
    fn(ctx, 0, nrows);
}

void (*nd_parallel_rows)(nd_row_fn fn, void *ctx, uint32_t nrows) = rows_serial;

/* Row walker for the 2-bit pair-table path; see nd_lut2_rows_c in the header.
 * The context is shared with engine/src/lut2_tie728.S, so the field order in
 * nd_lut2_ctx is part of the assembly ABI. */
ND_HOT void nd_lut2_rows_c(void *vc, uint32_t r0, uint32_t r1)
{
    const nd_lut2_ctx *c = (const nd_lut2_ctx *)vc;
    uint32_t           r;

    for (r = r0; r < r1; r++) {
        const uint8_t  *row = c->packed + (size_t)r * c->rowbytes;
        const uint16_t *nrm = c->norms + (size_t)r * c->ngroup;
        float           acc = 0.0f;
        uint32_t        gi;

        /* Same value as nd_f16(nrm[gi]), hoisted: nrm is read once per row and
         * the conversion does not depend on the group's data. */
        for (gi = 0; gi < c->ngroup; gi++) {
            float   nf = nd_f16(nrm[gi]);
            float   s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
            const uint8_t *qq = row + (size_t)gi * c->gbytes;
            const float   *T  = c->lut + (size_t)gi * c->gpairs * 16;
            uint32_t       j;
            /* Two packed bytes (four pairs, 8 weights) per 32-bit load. Rows
             * are group-aligned and g is a multiple of 8, so qq stays
             * 4-aligned. Each pair k indexes the table slot block its own 4
             * bits belong to, so the four accumulators see exactly the same
             * entries in the same order as the byte-wise loop. */
            for (j = 0; j < c->g; j += 16) {
                uint32_t w = ((const uint32_t *)(const void *)qq)[0];
                qq += 4;
                s0 += T[         w         & 15u];
                s1 += T[16 + ((w >>  4)  & 15u)];
                s2 += T[32 + ((w >>  8)  & 15u)];
                s3 += T[48 + ((w >> 12)  & 15u)];
                T += 64;
                s0 += T[         (w >> 16) & 15u];
                s1 += T[16 + ((w >> 20)  & 15u)];
                s2 += T[32 + ((w >> 24)  & 15u)];
                s3 += T[48 +  (w >> 28)];
                T += 64;
            }
            acc += nf * ((s0 + s1) + (s2 + s3));
        }
        c->y[r] = acc;
    }
}

#if ND_LUT2_ASM
/* nd_lut2_asm_ok() walks every norm in the row range, and a projection runs once
 * per token, so asking it on every call would cost more than the kernel saves.
 * Its answer depends only on the geometry and on the packed/norm blob - both set
 * in stone once the model is mapped - so it is remembered per (blob, rows). */
static const void *s_asm_blob;
static uint32_t    s_asm_rows;
static int         s_asm_ok;

static int lut2_asm_usable(const nd_lut2_ctx *c, const void *blob, uint32_t rows)
{
    if (blob != s_asm_blob || rows != s_asm_rows) {
        s_asm_ok   = nd_lut2_asm_ok(c, 0, rows);
        s_asm_blob = blob;
        s_asm_rows = rows;
    }
    return s_asm_ok;
}
#endif

ND_HOT void nd_cq_gemv_lut2(const nd_tensor *t, const void *blob,
                            const float *lut, float *y)
{
    nd_lut2_ctx ctx;
    nd_row_fn   fn = nd_lut2_rows_c;

    nd_lut2_fill(&ctx, t, blob, lut, y);
#if ND_LUT2_ASM
    fn = lut2_asm_usable(&ctx, blob, t->shape[0]) ? nd_lut2_rows_tie1n
                                                  : nd_lut2_rows_c;
#endif
    nd_parallel_rows(fn, &ctx, t->shape[0]);
}

ND_HOT void nd_lut2_fill(nd_lut2_ctx *c, const nd_tensor *t, const void *blob,
                         const float *lut, float *y)
{
    const uint8_t *packed   = (const uint8_t *)blob;
    uint32_t       out      = t->shape[0];

    c->packed   = packed;
    c->norms    = (const uint16_t *)(const void *)(packed +
                      (size_t)out * nd_cq_row_bytes(t));
    c->lut      = lut;
    c->y        = y;
    c->ngroup   = nd_cq_groups(t);
    c->g        = t->group;
    c->gbytes   = t->group / 4;        /* 2 bits per weight */
    c->gpairs   = t->group / 2;
    c->rowbytes = nd_cq_row_bytes(t);
}

int nd_lut2_asm_ok(const nd_lut2_ctx *c, uint32_t r0, uint32_t r1)
{
    uint32_t r, gi;
    /* The specialised geometry of every 2-bit tensor in this model. */
    if (c->g != 128u || c->gbytes != 32u || c->gpairs != 64u)
        return 0;
    if ((((uintptr_t)c->packed) & 3u) != 0u)
        return 0;
    /* nd_f16()'s inline path cannot see subnormals or inf/nan; the C kernel
     * calls nd_f16_slow() for those, so the blob has to be free of them. */
    for (r = r0; r < r1; r++)
        for (gi = 0; gi < c->ngroup; gi++) {
            uint32_t e = (c->norms[(size_t)r * c->ngroup + gi] >> 10) & 0x1Fu;
            if (e == 0u || e == 0x1Fu)
                return 0;
        }
    return 1;
}

typedef struct { const uint8_t *packed; const uint16_t *norms;
                 const float *xh, *cb; const uint32_t *ids; float *y;
                 uint32_t ngroup, g, bits, rowbytes; } gather_ctx;

/* Gathered rows over an id range: each id writes its own y slot and its group
 * walk keeps the ascending gi order, so both cores produce exactly the values
 * the single-core loop did. Used by the constrained-logits path, where `n` is
 * the grammar's candidate set (dozens of 4-bit embedding rows). */
static ND_HOT void gather_rows(void *vc, uint32_t i0, uint32_t i1)
{
    const gather_ctx *c = (const gather_ctx *)vc;
    uint32_t          i;

    for (i = i0; i < i1; i++) {
        const uint8_t  *row = c->packed + (size_t)c->ids[i] * c->rowbytes;
        const uint16_t *nrm = c->norms + (size_t)c->ids[i] * c->ngroup;
        float           acc = 0.0f;
        uint32_t        gi;

        for (gi = 0; gi < c->ngroup; gi++) {
            float nf = nd_f16(nrm[gi]);   /* same value, converted once */
            acc += nf * dot_group(row, gi * c->g * c->bits, c->bits, c->g,
                                  c->cb, c->xh + (size_t)gi * c->g);
        }
        c->y[i] = acc;
    }
}

ND_HOT void nd_cq_gemv_gather(const nd_cact *c, const nd_tensor *t, const void *blob,
                              const float *xh, const uint32_t *ids, uint32_t n,
                              float *y)
{
    const uint8_t  *packed   = (const uint8_t *)blob;
    uint32_t        out      = t->shape[0];
    uint32_t        rowbytes = nd_cq_row_bytes(t);
    gather_ctx      gc;

    gc.packed   = packed;
    gc.norms    = (const uint16_t *)(const void *)(packed +
                       (size_t)out * rowbytes);
    gc.xh       = xh;
    gc.cb       = nd_cact_codebook(c, t->bits);
    gc.ids      = ids;
    gc.y        = y;
    gc.ngroup   = nd_cq_groups(t);
    gc.g        = t->group;
    gc.bits     = t->bits;
    gc.rowbytes = rowbytes;

    nd_parallel_rows(gather_rows, &gc, n);
}

void nd_cq_gemv(const nd_cact *c, const nd_tensor *t, const void *blob,
                const float *x, float *scratch, float *y)
{
    nd_cq_prepare(t, x, scratch);
    nd_cq_gemv_prepared(c, t, blob, scratch, y);
}

void nd_cq_dequant_row(const nd_cact *c, const nd_tensor *t, const void *blob,
                       uint32_t row, float *scratch, float *w)
{
    const uint8_t  *packed   = (const uint8_t *)blob;
    uint32_t        out      = t->shape[0];
    uint32_t        g        = t->group;
    uint32_t        ngroup   = nd_cq_groups(t);
    uint32_t        rowbytes = nd_cq_row_bytes(t);
    const uint16_t *norms    = (const uint16_t *)(const void *)(packed +
                                (size_t)out * rowbytes);
    const float    *cb       = nd_cact_codebook(c, t->bits);
    const uint8_t  *p        = packed + (size_t)row * rowbytes;
    const uint16_t *nrm      = norms + (size_t)row * ngroup;
    float           scale    = 1.0f / sqrtf((float)g);
    uint32_t        gi;

    for (gi = 0; gi < ngroup; gi++) {
        float   *blk    = scratch + (size_t)gi * g;
        float    norm   = nd_f16(nrm[gi]);
        uint32_t bit_off = gi * g * t->bits;
        uint32_t j;

        for (j = 0; j < g; j++) {
            uint32_t b   = bit_off + j * t->bits;
            uint32_t byt = b >> 3;
            uint32_t sh  = b & 7u;
            uint32_t v   = p[byt];
            if (sh + t->bits > 8)
                v |= (uint32_t)p[byt + 1] << 8;
            blk[j] = cb[(v >> sh) & ((1u << t->bits) - 1u)] * norm;
        }
        nd_fwht(blk, g);
        for (j = 0; j < g; j++)
            blk[j] *= scale;
    }
    memcpy(w, scratch, t->shape[1] * sizeof(float));
}
