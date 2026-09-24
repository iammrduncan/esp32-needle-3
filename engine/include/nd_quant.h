/* nd_quant.h - Cactus-Quants kernels.
 *
 * A CQ tensor stores, per group of `group` weights, a codebook index per
 * weight plus one FP16 group norm. The logical weights are
 *
 *     w_group = (codebook[idx] * norm) @ H,     H = Walsh(group)/sqrt(group)
 *
 * H is symmetric and orthonormal, so for a matvec we never have to
 * materialise w:
 *
 *     <w_group, x_group> = <(cb[idx] * norm) @ H, x_group>
 *                        = <cb[idx] * norm, H @ x_group>
 *
 * i.e. transform the *activation* once per group (O(in log group)) and every
 * output row then costs one codebook lookup and one multiply-add per weight,
 * with no dequantised weights in RAM. That is what makes a 45M-parameter model
 * tractable on an LX7 reading weights straight out of flash.
 */
#ifndef ND_QUANT_H
#define ND_QUANT_H

#include <stdint.h>
#include <string.h>

#include "nd_cact.h"

/* The GEMV inner loop runs ~45M times per token. On the ESP32 the definitions
 * are placed in IRAM so they are not fetched through the instruction cache
 * from flash. Applied at the definition only: repeating it on the prototype
 * makes GCC emit conflicting section names. */
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define ND_HOT IRAM_ATTR
#else
#define ND_HOT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* IEEE half -> float.
 *
 * Inline and branch-free on the normal-number path: this is called once per
 * norm scale, per MLP diagonal and per confidence probe element, which adds up
 * to >100K calls per token. As an out-of-line call it cost more than the
 * arithmetic it feeds. Subnormals and inf/nan fall back to the slow path. */
float nd_f16_slow(uint16_t h);

static inline float nd_f16(uint16_t h)
{
    uint32_t e = (h >> 10) & 0x1Fu;
    uint32_t bits;
    float    f;

    if (e == 0u || e == 0x1Fu)
        return nd_f16_slow(h);

    bits = ((uint32_t)(h & 0x8000u) << 16) |
           ((e + (127u - 15u)) << 23) |
           ((uint32_t)(h & 0x3FFu) << 13);
    memcpy(&f, &bits, 4);
    return f;
}

/* exp(x), ~5x faster than libm's and accurate to ~1e-7 relative.
 *
 * silu, the attention gate, the engram alpha, Sinkhorn and the attention
 * softmax together make ~30K exponential calls per token; on an LX7 that is
 * software-emulated and measurable. Range-reduce to 2^k * 2^f with f in
 * [-0.5, 0.5], evaluate 2^f with a degree-5 polynomial, and apply 2^k by
 * assembling the exponent field directly. */
/* Put an assembled 32-bit pattern into a float register.
 *
 * On Xtensa this is a single register transfer. The plain 4-byte copy - even as an explicit
 * builtin, which is what run #424 needs for the -fno-builtin-memcpy flag - is lowered to a stack
 * store plus an FP load (`s32i.n` + `lsi`), and that round trip, not the arithmetic, is what the
 * board-1 kbench screen measured at ~50 cycles per conversion. Same bits in, same float out; the
 * host build (the correctness oracle) keeps the portable copy. */
static inline float nd_f32_from_bits(uint32_t bits)
{
#if defined(__XTENSA__)
    float f;
    __asm__ __volatile__("wfr %0, %1" : "=f"(f) : "r"(bits));
    return f;
#else
    float f;
    __builtin_memcpy(&f, &bits, 4);
    return f;
#endif
}

static inline float nd_expf(float x)
{
    float    z, f, p;
    int      k;
    uint32_t bits;
    float    scale;

    if (x > 88.0f)  return 3.4028235e38f;
    if (x < -88.0f) return 0.0f;

    z = x * 1.44269504f;               /* x / ln 2 */
    k = (int)(z + (z >= 0.0f ? 0.5f : -0.5f));
    f = z - (float)k;

    p = 0.0013333f;
    p = p * f + 0.0096181f;
    p = p * f + 0.0555041f;
    p = p * f + 0.2402265f;
    p = p * f + 0.6931472f;
    p = p * f + 1.0f;

    bits = (uint32_t)(k + 127) << 23;  /* 2^k */
    scale = nd_f32_from_bits(bits);
    return p * scale;
}

/* exp() for two independent arguments at once.
 *
 * The attention online softmax evaluates `w0 = exp(s0 - m)` and
 * `w1 = exp(s1 - m)` back to back for the two KV positions of a pair: 12
 * heads x ~half the context x 8 layers, so ~10K pairs and two thirds of all
 * exponential calls in a decode token. Each scalar expansion is a degree-5
 * Horner chain with a strict serial dependency (~5 x FP-add latency of pure
 * latency, nothing to issue in between), and disassembly of attn_heads showed
 * GCC scheduling the two expansions one after the other and spilling live
 * floats to the stack to do it. Interleaving the two chains gives the
 * scheduler two independent operands per slot and lets both stay in
 * registers.
 *
 * Each chain performs exactly the operations nd_expf() performs, in the same
 * order with the same constants, so the pair is bit-identical to two scalar
 * calls - this is not an approximation swap. Out-of-range arguments take the
 * scalar path, which is the only place the two clamps live. */
static inline void nd_expf_pair(float x0, float x1, float *r0, float *r1)
{
    float    z0, z1, f0, f1, p0, p1, sc0, sc1;
    int      k0, k1;
    uint32_t b0, b1;

    if (x0 > 88.0f || x0 < -88.0f || x1 > 88.0f || x1 < -88.0f) {
        *r0 = nd_expf(x0);
        *r1 = nd_expf(x1);
        return;
    }

    z0 = x0 * 1.44269504f;
    z1 = x1 * 1.44269504f;
    k0 = (int)(z0 + (z0 >= 0.0f ? 0.5f : -0.5f));
    k1 = (int)(z1 + (z1 >= 0.0f ? 0.5f : -0.5f));
    f0 = z0 - (float)k0;
    f1 = z1 - (float)k1;

    p0 = 0.0013333f;  p1 = 0.0013333f;
    p0 = p0 * f0 + 0.0096181f;  p1 = p1 * f1 + 0.0096181f;
    p0 = p0 * f0 + 0.0555041f;  p1 = p1 * f1 + 0.0555041f;
    p0 = p0 * f0 + 0.2402265f;  p1 = p1 * f1 + 0.2402265f;
    p0 = p0 * f0 + 0.6931472f;  p1 = p1 * f1 + 0.6931472f;
    p0 = p0 * f0 + 1.0f;        p1 = p1 * f1 + 1.0f;

    b0 = (uint32_t)(k0 + 127) << 23;
    b1 = (uint32_t)(k1 + 127) << 23;
    sc0 = nd_f32_from_bits(b0);
    sc1 = nd_f32_from_bits(b1);
    *r0 = p0 * sc0;
    *r1 = p1 * sc1;
}

/* In-place unnormalised fast Walsh-Hadamard transform. `n` must be a power
 * of two. Apply 1/sqrt(n) yourself if you want the orthonormal H. */
void nd_fwht(float *x, uint32_t n);

/* Scratch an activation needs before nd_cq_gemv: `in_pad` floats. */
uint32_t nd_cq_scratch(const nd_tensor *t);

/* Prepare an activation for one or more matvecs that share a reduction axis:
 * zero-pads x to in_pad and applies the orthonormal H per group.
 * `xh` must hold nd_cq_scratch(t) floats. Reusable across every tensor with
 * the same (shape[1], group). */
void nd_cq_prepare(const nd_tensor *t, const float *x, float *xh);

/* y[0..out) = W @ x, where `xh` came from nd_cq_prepare on the same tensor. */
void nd_cq_gemv_prepared(const nd_cact *c, const nd_tensor *t, const void *blob,
                         const float *xh, float *y);

/* Same, but only rows [r0, r0+nrows). The mHC phi tensors stack every layer
 * into one tensor, so a layer's slice is a row range. */
void nd_cq_gemv_rows(const nd_cact *c, const nd_tensor *t, const void *blob,
                     const float *xh, uint32_t r0, uint32_t nrows, float *y);

/* Convenience: prepare + matvec. `scratch` holds nd_cq_scratch(t) floats. */
void nd_cq_gemv(const nd_cact *c, const nd_tensor *t, const void *blob,
                const float *x, float *scratch, float *y);

/* ---- 2-bit lookup path -------------------------------------------------
 *
 * At 2 bits the per-weight work (extract index, index the codebook, load the
 * activation, multiply-add) is ~6 instructions, and it repeats for every
 * output row. Since the activation is fixed across rows, precompute instead:
 * for each adjacent PAIR of reduction positions, tabulate the 16 possible
 * partial sums. The inner loop then becomes one indexed load and one add per
 * pair - no multiplies at all.
 *
 * The table is 16 floats per pair (in_pad/2 pairs), built once per prepared
 * activation and shared by every tensor that reduces over it.
 */
static inline uint32_t nd_cq_lut_floats(uint32_t in_pad)
{
    return in_pad / 2 * 16;
}

/* ---- optional row-level parallelism ------------------------------------
 *
 * Every GEMV here is embarrassingly parallel across output rows: each row
 * reads its own slice of weights and writes one output. The engine stays
 * single-threaded and portable by default; a platform can install a splitter
 * (the ESP32 build hands half the rows to the second core) and every matvec
 * picks it up.
 *
 * CONTRACT for an installed fn: it runs on two cores at once, over disjoint row
 * ranges, with NO barrier between the halves - the caller only waits for both to
 * finish. So fn may write its output rows and nothing else shared. Scratch
 * belongs on the stack or in the ctx (see attn_heads' staging arrays); a static
 * or global buffer is a data race, and one that mixes values silently will not
 * reliably show up in byte-exact goldens, because the halves are equal-sized and
 * usually stay in phase. The host splitter is serial, so the host build cannot
 * catch this at all. */
typedef void (*nd_row_fn)(void *ctx, uint32_t r0, uint32_t r1);
extern void (*nd_parallel_rows)(nd_row_fn fn, void *ctx, uint32_t nrows);

/* Build the pair table from an already-prepared activation. */
void nd_cq_lut_build(const nd_cact *c, const float *xh, uint32_t in_pad,
                            float *lut);

/* y[0..out) = W @ x for a 2-bit tensor, via the pair table. */
void nd_cq_gemv_lut2(const nd_tensor *t, const void *blob,
                            const float *lut, float *y);

/* ---- the 2-bit row walker, shared with the handwritten microkernels -------
 *
 * The context the row walker reads. It is also the ABI of the handwritten
 * kernels in engine/src/lut2_tie728.S, which hard-code these offsets, so the
 * field order is load-bearing (kbench.c carries _Static_asserts for it).
 * Exposed so the Experiment 2 microbenchmark can drive the very function the
 * shipping path calls, instead of a copy that could drift from it. */
typedef struct {
    const uint8_t  *packed;
    const uint16_t *norms;
    const float    *lut;
    float          *y;
    uint32_t        ngroup, gbytes, gpairs, g, rowbytes;
} nd_lut2_ctx;

/* Fill a context exactly the way nd_cq_gemv_lut2() does. */
void nd_lut2_fill(nd_lut2_ctx *c, const nd_tensor *t, const void *blob,
                  const float *lut, float *y);

/* The shipping row walker plus the two handwritten prototypes. All three have
 * the nd_row_fn signature, so any of them can go through nd_parallel_rows. */
void nd_lut2_rows_c(void *vc, uint32_t r0, uint32_t r1);
void nd_lut2_rows_tie1(void *vc, uint32_t r0, uint32_t r1);
void nd_lut2_rows_tie2(void *vc, uint32_t r0, uint32_t r1);
void nd_lut2_rows_tie1p(void *vc, uint32_t r0, uint32_t r1);
void nd_lut2_rows_tie1n(void *vc, uint32_t r0, uint32_t r1);
void nd_lut2_rows_tie1m(void *vc, uint32_t r0, uint32_t r1);

/* Can the handwritten kernels take this tensor? They are specialised to the
 * geometry every 2-bit tensor in needle3.cact uses (group 128, so 32 packed
 * bytes and 64 pairs per group), need the 4-byte alignment every CQ row has,
 * and do the inline FP16->FP32 conversion only, so no norm in the range may
 * touch nd_f16_slow's subnormal/inf path. */
int nd_lut2_asm_ok(const nd_lut2_ctx *c, uint32_t r0, uint32_t r1);

/* Context for the handwritten 4-bit row walker in engine/src/gemv4_tie728.S,
 * which hard-codes these offsets. C resolves every cursor - packed0/norms0/y0 are
 * already stepped to the first row of the split and rowbytes/normstep step them
 * on - so the kernel body needs no integer multiply, only adds. */
typedef struct {
    const uint8_t  *packed0;
    const uint16_t *norms0;
    const float    *xh, *cb;
    float          *y0;
    uint32_t        ngroup, g, rowbytes, normstep, nrows;
} nd_gemv4_ctx;

void nd_gemv4_rows_tie1(void *vc);

/* The 4-bit kernel is specialised to group 128 (16 index words and a 512-byte xh
 * stride per group) and does the inline FP16->FP32 conversion only, so no norm in
 * range may touch nd_f16_slow's path - the same restriction nd_lut2_asm_ok()
 * applies. bits must be 4: this body reads two nibbles per 32-bit word. */
int nd_gemv4_asm_ok(uint32_t bits, uint32_t g, uint32_t ngroup, uint32_t rows,
                    const uint16_t *norms);

/* A quad table (one byte -> one lookup -> four weights) was tried and removed:
 * it issues fewer instructions but forces group-outer iteration, which uses
 * only 32 bytes of every 64-byte cache line and measured 38% slower on the
 * ESP32-S3. The pair table above is the faster of the two.
 */

/* y[i] = W[ids[i]] . x, for a prepared activation. Used to score only the
 * tokens a grammar currently permits instead of the whole 8192-row vocabulary. */
void nd_cq_gemv_gather(const nd_cact *c, const nd_tensor *t, const void *blob,
                              const float *xh, const uint32_t *ids, uint32_t n,
                              float *y);

/* Reconstruct one dequantised output row into `w` (shape[1] floats).
 * Used for validation and for the engram tables, which are gathered by row
 * rather than streamed as a matvec. `scratch` holds in_pad floats. */
void nd_cq_dequant_row(const nd_cact *c, const nd_tensor *t, const void *blob,
                       uint32_t row, float *scratch, float *w);

#ifdef __cplusplus
}
#endif
#endif /* ND_QUANT_H */
