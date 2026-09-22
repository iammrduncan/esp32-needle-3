# Run #299 - skip `logf(1.0f)` in Sinkhorn (banked after #291): MEASURED, CLOSED, no board time

Screen (`.auto/logfskip/screen.patch`, apply + `cmake -S host -B /tmp/hostsink
-DCMAKE_C_FLAGS=-DND_SINK_STATS` + build, then run the frozen primary prompts through that
`nd_dump`; the `SINKSTAT` stderr line is its own proof the define compiled in - #242's rule):

    1,448,960 logf calls over the 6 primary prompts of the real model:
      sum == 1.0f            : 13.58 %   <- the banked skip's real hit rate
      8-entry exact-bit memo : 17.58 %   (distinct sum patterns >= 8,192/prompt, i.e. capped)
      nd_expf args == +-0    : 26.07 %   <- independently confirms run #291's "a quarter" premise

Verdict, by break-even rather than by feel: 1,280 logf/token, 13.58 % skipped = 174 calls/token.
A 0.2 % keep bar at the campaign's measured 70 % realisation needs 0.57 ms/token = 137k cycles,
so **logf must cost >= 787 cycles for this to clear the bar**. That is far above any plausible
float log on this core, so the idea is closed without spending a device microbench; reopen only
on a measured logf cost >= 787 cycles. The stronger variant I tested alongside it - a memo keyed
on the exact bits of `sum` (bit-exact because logf is a pure function of its argument) - is dead
on the data: the argument is essentially never bit-repeated (>= 8,192 distinct values against
~230k calls per prompt, 3.5 % reuse at best), and even the 4-value special-case table for
sum in {1,2,3,4} cannot reach break-even.

Kept because it is rare and useful: `exp_d_zero = 26.07 %` is a *live* confirmation of an accepted
change's premise (#291 claimed "a quarter of the kernel's exponentials"), which most premises in
this ledger do not get.
