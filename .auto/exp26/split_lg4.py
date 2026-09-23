#!/usr/bin/env python3
"""Split the sampler's ND_P_LOGITS phase into prepare and gather, per call.

Why this is the next measurement: the request-path harvest (batch 20260923T0200L,
board 3) measured `logits4` = 3.8 ms per tools-request token and 7.1 ms per route
token, with the candidate count measured on the SAME image at mean_n = 6.7 (max 68).
At the cycles/row this campaign measured for that exact tensor and kernel in kbench
(2,885,800 cycles for 512 rows two-core = 5,636 wall cycles/row), 6.7 rows is
37.8k cycles = 0.16 ms, and one nd_cq_prepare costs ~0.22 ms (run #342's own
`prep+lut` = 3.6 ms over ~16 calls). So ~3.4 ms of a 3.8 ms bracket - 1.7 % of a
request token - is inside nd_model_logits_subset and is neither the rows nor the
prepare by those numbers. This patch localises it.

Diagnostic only: everything is behind ND_PROFILE, so a shipping configure cannot see
it, and `git checkout engine/src/nd_model.c` reverts. Apply, then run
`.auto/lane_prof.sh` inside `needle-board run N` (it builds -DND_PROFILE=1 on a fresh
dir, flashes, and reads the per-request tables off the console).
"""

p = "engine/src/nd_model.c"
s = open(p).read()

old = """    ND_T0(tl);
    nd_cq_prepare(&m->embedding, hidden, m->xh);
    nd_cq_gemv_gather(&m->c, &m->embedding, nd_cact_data(&m->c, &m->embedding),
                      m->xh, ids, n, out);
    ND_T1(tl, ND_P_LOGITS);
}"""

new = """    ND_T0(tl);
#ifdef ND_PROFILE
    /* Per-call split of the phase that run #342 measured at 3.8 ms per request
     * token while the candidate list averages 6.7 rows (max 68) on the same image:
     * the rows are ~0.16 ms and one prepare ~0.22 ms, so most of this bracket is
     * currently unattributed. One line per call is cheap - a request is ~26 calls. */
    {
        extern int printf(const char *, ...);
        int64_t p0 = esp_timer_get_time(), p1, p2;
        nd_cq_prepare(&m->embedding, hidden, m->xh);
        p1 = esp_timer_get_time();
        nd_cq_gemv_gather(&m->c, &m->embedding, nd_cact_data(&m->c, &m->embedding),
                          m->xh, ids, n, out);
        p2 = esp_timer_get_time();
        printf("EVT lg4 prep=%.3f gather=%.3f n=%u\\n",
               (double)(p1 - p0) / 1000.0, (double)(p2 - p1) / 1000.0, (unsigned)n);
    }
#else
    nd_cq_prepare(&m->embedding, hidden, m->xh);
    nd_cq_gemv_gather(&m->c, &m->embedding, nd_cact_data(&m->c, &m->embedding),
                      m->xh, ids, n, out);
#endif
    ND_T1(tl, ND_P_LOGITS);
}"""

assert s.count(old) == 1, "nd_model_logits_subset body moved; re-read it"
open(p, "w").write(s.replace(old, new, 1))
print("lg4 split inserted (ND_PROFILE only); revert with git checkout engine/src/nd_model.c")
