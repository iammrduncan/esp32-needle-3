#!/usr/bin/env python3
"""Screen `f16bc` (board 1, kbench over JTAG): what does the four-byte bitcast inside the
fp16->fp32 conversion actually cost on this target?

The mechanism is proven in the ELF, not assumed: this target compiles with -fno-builtin-memcpy,
so the plain 4-byte `memcpy` the shipped conversion uses becomes a ROM library call with
argument setup and FP spills around it. An explicit builtin call is exempt from that flag.

Both sides here are the SHIPPED arithmetic (the candidate side calls the engine's own nd_f16 for
the value check); only the bit-transfer differs. Two operands:
  enc  - all 65,536 half encodings, register-resident operand, so the number is the machinery
         (call + spills) rather than delivery, and the range covers every operand the model can
         hand the converter, including every subnormal and both signed zeros.
  dq   - a PSRAM operand walked with one conversion feeding a dependent FMA per element, which is
         the call structure the group dequant actually uses (delivery identical on both sides).

Prints a mismatch count over the full encoding space BEFORE any cycle number, so a candidate that
changes a bit cannot be read as a speed result.
"""
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "esp32/main/kbench.c"

BODY = """
/* ------------------------------------------------------------------ *
 * Experiment 52 screen: the cost of the four-byte bitcast in the fp16 -> fp32
 * conversion. See .auto/exp52/apply_kb_f16bc.py for what is being priced and
 * why the mechanism is not a guess (-fno-builtin-memcpy makes the shipped
 * 4-byte copy a ROM call; an explicit builtin is exempt).
 * ------------------------------------------------------------------ */
#define KB_F16_N   65536u
#define KB_F16_DQ  8192u
#define KB_F16_ROUNDS 9u

static ND_HOT float kb_f16_bc(uint16_t h)
{
    uint32_t e = (h >> 10) & 0x1Fu;
    uint32_t bits;
    float    f;

    if (e == 0u || e == 0x1Fu)
        return nd_f16_slow(h);
    bits = ((uint32_t)(h & 0x8000u) << 16) |
           ((e + (127u - 15u)) << 23) |
           ((uint32_t)(h & 0x3FFu) << 13);
    __builtin_memcpy(&f, &bits, 4);
    return f;
}

#define KB_F16_SWEEP_ENC(NAME, CONV)                                            \\
static ND_HOT float NAME(void)                                                  \\
{                                                                               \\
    float acc = 0.0f;                                                           \\
    for (uint32_t i = 0; i < KB_F16_N; i++)                                     \\
        acc += CONV((uint16_t)i) * (1.0f + (float)(i & 7u) * 0.001f);           \\
    asm volatile("" : "+f"(acc));                                               \\
    return acc;                                                                 \\
}

#define KB_F16_SWEEP_TAB(NAME, CONV)                                            \\
static ND_HOT float NAME(const uint16_t *tab, uint32_t n)                       \\
{                                                                               \\
    float acc = 0.0f;                                                           \\
    for (uint32_t i = 0; i < n; i++)                                            \\
        acc += CONV(tab[i]) * (1.0f + (float)(i & 7u) * 0.001f);                \\
    asm volatile("" : "+f"(acc));                                               \\
    return acc;                                                                 \\
}

KB_F16_SWEEP_ENC(kb_f16_enc_shipped, nd_f16)
KB_F16_SWEEP_ENC(kb_f16_enc_bc, kb_f16_bc)
KB_F16_SWEEP_TAB(kb_f16_tab_shipped, nd_f16)
KB_F16_SWEEP_TAB(kb_f16_tab_bc, kb_f16_bc)

static void bench_f16bc(const uint8_t *blob, uint32_t blob_bytes)
{
    uint16_t  *tab;
    uint32_t   i, r, bad = 0, slow = 0;
    uint32_t   best[4] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
    float      sink = 0.0f;

    /* Value first: the full encoding space, bit-compared against the engine's own
     * converter. Signed zero, subnormals and the NaN payloads all land in here. */
    for (i = 0; i < KB_F16_N; i++) {
        float a = nd_f16((uint16_t)i), b = kb_f16_bc((uint16_t)i);
        uint32_t eu = ((uint32_t)i >> 10) & 0x1Fu;
        if (eu == 0u || eu == 0x1Fu) slow++;
        if (memcmp(&a, &b, 4) != 0) bad++;
    }
    printf("KB F16BC NUM enc=%u mismatch=%u slow_path=%u\\n",
           (unsigned)KB_F16_N, (unsigned)bad, (unsigned)slow);

    tab = (uint16_t *)heap_caps_malloc(KB_F16_DQ * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!tab) {
        printf("KB F16BC SKIP dq reason=alloc\\n");
        fflush(stdout);
        return;
    }
    /* A real operand: bytes straight out of the mapped blob, so the exponent mix is
     * model-shaped rather than synthetic, in the memory the field reads from. */
    memcpy(tab, blob + (blob_bytes / 3u), KB_F16_DQ * sizeof(uint16_t));

    for (r = 0; r < KB_F16_ROUNDS; r++) {
        for (i = 0; i < 2; i++) {
            uint32_t c0, cy;
            c0 = esp_cpu_get_cycle_count();
            sink += i ? kb_f16_tab_bc(tab, KB_F16_DQ) : kb_f16_tab_shipped(tab, KB_F16_DQ);
            cy = esp_cpu_get_cycle_count() - c0;
            if (cy < best[i]) best[i] = cy;
            c0 = esp_cpu_get_cycle_count();
            sink += i ? kb_f16_enc_bc() : kb_f16_enc_shipped();
            cy = esp_cpu_get_cycle_count() - c0;
            if (cy < best[2u + i]) best[2u + i] = cy;
        }
    }
    asm volatile("" : "+f"(sink));
    printf("KB F16BC mode=dq        n=%u min_shipped=%u min_builtin=%u "
           "cyc_shipped=%u.%02u cyc_builtin=%u.%02u pct=%+.2f (positive = builtin cheaper)\\n",
           (unsigned)KB_F16_DQ, (unsigned)best[0], (unsigned)best[1],
           (unsigned)(best[0] / KB_F16_DQ), (unsigned)(best[0] * 100u / KB_F16_DQ % 100u),
           (unsigned)(best[1] / KB_F16_DQ), (unsigned)(best[1] * 100u / KB_F16_DQ % 100u),
           100.0 * (double)(best[0] - best[1]) / (double)best[0]);
    printf("KB F16BC mode=enc_all   n=%u min_shipped=%u min_builtin=%u "
           "cyc_shipped=%u.%02u cyc_builtin=%u.%02u pct=%+.2f (positive = builtin cheaper)\\n",
           (unsigned)KB_F16_N, (unsigned)best[2], (unsigned)best[3],
           (unsigned)(best[2] / KB_F16_N), (unsigned)(best[2] * 100u / KB_F16_N % 100u),
           (unsigned)(best[3] / KB_F16_N), (unsigned)(best[3] * 100u / KB_F16_N % 100u),
           100.0 * (double)(best[2] - best[3]) / (double)best[2]);
    fflush(stdout);
    heap_caps_free(tab);
}

"""

CALL = """    bench_div();
#if ND_KB_EXP38"""

CALL_NEW = """    bench_f16bc(s_base, bytes);
    /* The capture harness stops on this marker, so the screen-only image
     * never reaches the shape benches - no unreachable code, no dead statements. */
    printf("EVT KBENCH_DONE\\n");
    fflush(stdout);
    bench_div();
#if ND_KB_EXP38"""


def main() -> int:
    text = open(PATH).read()
    if "kb_f16_bc" in text:
        print("ALREADY_APPLIED")
        return 2
    for anchor in ("static void bench_div(void)", CALL):
        if text.count(anchor) != 1:
            print(f"ANCHOR missing/duplicated: {anchor[:40]!r} count={text.count(anchor)}")
            return 2
    at = text.index("static void bench_div(void)")
    new = text[:at] + BODY + text[at:]
    new = new.replace(CALL, CALL_NEW, 1)
    if new.count("bench_f16bc(") != 2:
        print("MARKER_ASSERT_FAILED")
        return 3
    open(PATH, "w").write(new)
    print(f"APPLIED target={PATH}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
