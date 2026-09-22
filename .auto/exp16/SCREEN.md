# Experiment 16 off-device screen (host, all six primary prompts, 93 real decode steps)

Build: fresh host dir with `-DND_SAMPLE_STATS=1`, screen lives inside
`nd_sample_hidden` behind that define (`screen.patch`), verified live by its own
output (not by `nm`) per the run #242 lesson. It builds the bucket index,
enumerates exactly what a bucket walk would enumerate, runs the same `token_ok`
byte walk, sorts, and compares to the shipped candidate list element by element.

    sampling5              tools  steps= 17   SAMPLEIDX ids=8176 buckets_nonempty=94
    timer60                tools  steps= 13   SAMPLEIDX ids=8176 buckets_nonempty=94
    status_heap            tools  steps=  9   SAMPLEIDX ids=8176 buckets_nonempty=94
    batch                  tools  steps= 28   SAMPLEIDX ids=8176 buckets_nonempty=94
    heldout_timer45        tools  steps= 13   SAMPLEIDX ids=8176 buckets_nonempty=94
    route_translate        route  steps= 13   SAMPLEIDX ids=8176 buckets_nonempty=94

    total steps=93  set_mismatch_steps=0
    allowed_bytes      min/med/max = 1 / 1 / 11
    touched_by_buckets min/med/max = 3 / 51 / 589
    candidates         min/med/max = 1 / 3 / 68
    shipped walk per step          = 8176 piece lookups
    sum(touched) / sum(walk)       = 1.62 %
    steps with touched > 50% walk  = 0 of 93
    touched p50 / p90              = 51 / 600

Read: the grammar engages on very few first bytes (median ONE legal byte per
decode step, only 94 of 256 byte values ever start a piece), so 98.4 % of the
shipped filter's per-id work - `nd_tok_piece` + table test + branch for 8,176 ids
per token - is spent rejecting ids that could never be legal. The bucket walk
visits a median of 51 ids and reproduced the shipped candidate list, element for
element and in ascending order, on every one of the 93 steps.

That is the condition the experiment set for spending board time, cleared by a
wide margin, and it is why this candidate is not the same bet as the rejected
run #149 (which kept all 8,176 iterations and only shortened each one).
