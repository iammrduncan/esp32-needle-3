#!/usr/bin/env python3
"""Candidate `eghoist`: the engram tap convolution accumulates in a register and stores each output
once, instead of a read-modify-write pass over the whole output per tap.

`egtap_rows` is tap-OUTER: it zeroes `out[lo..hi)`, then for each tap walks the range again doing
`out[d] += tp[j][d] * src_j[d]`. With the archive's 4 taps and 3 dilation that is four PSRAM
load-and-store passes per column per site where one store is enough, plus the same number of
dependent round-trips through memory between the FMAs. It is also the exact shape the campaign
warned about for the tap family ("avoid a naive tap-outer implementation that adds a PSRAM output
load/store for every tap").

The candidate resolves the valid tap rows once per call (the modulo and the site/hist arithmetic
move out of the column loop, as in #exp57) and then walks columns with a register accumulator and
a single store.

Bit-exact by construction, and it is the order that makes it admissible: for every column the
products are summed in ascending tap order, `back > pos` taps are skipped exactly as the shipped
`continue` skips them, and the accumulator is seeded at +0.0f exactly as the shipped zero-store
seeds it, so each stored value is the same sequence of roundings. `.auto/exp64/test_eg_equiv.c`
proves it over dims 96/576/768/33, taps 1-8, dilations 1-4, every position and 1-3 column blocks.
"""
import hashlib, sys
PATH = sys.argv[1] if len(sys.argv) > 1 else "engine/src/nd_model.c"
BASE_MD5 = "53553a3507c5"           # accepted+expbc+head4+taphoist engine/src/nd_model.c

OLD = """    for (d = lo; d < hi; d++)
        c->out[d] = 0.0f;
    for (j = 0; j < c->taps; j++) {
        uint32_t      back = j * c->dil;
        const float   *src;
        if (back > c->pos)
            continue;                          /* tap_ok */
        src = c->hist + ((size_t)c->site * ND_EG_HIST +
                         ((c->eg_pos - back) % ND_EG_HIST)) * dm;
        for (d = lo; d < hi; d++)
            c->out[d] += c->tp[j * dm + d] * src[d];
    }
}"""

NEW = """    /* Resolve the participating tap rows once: the shipped form derived the history slot
     * inside the tap loop, which is fine, but then swept the output once PER TAP with a
     * dependent load/store. Column order and ascending-tap accumulation are preserved, so
     * the register sum is the same sequence of roundings as the memory-resident one. */
    const float *hp[ND_EG_HIST], *wp[ND_EG_HIST];
    uint32_t     nt = 0;
    if (c->taps > ND_EG_HIST) {
        /* A geometry this pool was never built for (the archive satisfies
         * (taps-1)*dilation + 1 = 10, so taps <= 10 < ND_EG_HIST): keep the shipped
         * sweep rather than silently dropping the taps that do not fit. */
        for (d = lo; d < hi; d++)
            c->out[d] = 0.0f;
        for (j = 0; j < c->taps; j++) {
            uint32_t back = j * c->dil;
            if (back > c->pos)
                continue;
            {
                const float *src = c->hist + ((size_t)c->site * ND_EG_HIST +
                                    ((c->eg_pos - back) % ND_EG_HIST)) * dm;
                for (d = lo; d < hi; d++)
                    c->out[d] += c->tp[j * dm + d] * src[d];
            }
        }
        return;
    }
    for (j = 0; j < c->taps; j++) {
        uint32_t back = j * c->dil;
        if (back > c->pos)
            continue;                          /* tap_ok, exactly as shipped */
        wp[nt] = c->tp + (size_t)j * dm;
        hp[nt] = c->hist + ((size_t)c->site * ND_EG_HIST +
                            ((c->eg_pos - back) % ND_EG_HIST)) * dm;
        nt++;
    }
    if (nt == 0u) {
        for (d = lo; d < hi; d++)
            c->out[d] = 0.0f;
        return;
    }
    for (d = lo; d < hi; d++) {
        float value = 0.0f;
        for (j = 0; j < nt; j++)
            value += wp[j][d] * hp[j][d];
        c->out[d] = value;
    }
}"""


def main() -> int:
    data = open(PATH, "rb").read()
    got = hashlib.md5(data).hexdigest()[:12]
    if got != BASE_MD5:
        print(f"BASE_MISMATCH got={got} want={BASE_MD5} -> refusing"); return 2
    text = data.decode()
    if text.count(OLD) != 1:
        print(f"ANCHOR count={text.count(OLD)} -> refusing"); return 2
    new = text.replace(OLD, NEW, 1)
    if new == text or new.count("const float *hp[ND_EG_HIST]") != 1 or new.count("out[d] +=") != 1:
        print("MARKER_ASSERT_FAILED"); return 3
    open(PATH, "wb").write(new.encode())
    print("APPLIED target=" + PATH + " md5=" + hashlib.md5(new.encode()).hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main())
