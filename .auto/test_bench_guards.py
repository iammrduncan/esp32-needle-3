"""Guards for the two golden-integrity holes closed in run #296.

The byte-exact gate is the campaign's quality authority, so the ways it could
silently stop checking things matter more than a fraction of a percent of speed.
Run: .venv/bin/python .auto/test_bench_guards.py
"""
import importlib.util
import pathlib
import sys

spec = importlib.util.spec_from_file_location(
    'bench', pathlib.Path(__file__).resolve().parent / 'bench.py')
bench = importlib.util.module_from_spec(spec)
sys.argv = ['bench.py']
spec.loader.exec_module(bench)

# 1. CASES must be the case groups only. It used to include `probe_ids`, which
#    made measured_groups_full() never true and every --save-golden refused.
assert set(bench.CASES) == {'primary', 'extended', 'think'}, sorted(bench.CASES)
assert bench.measured_groups_full('primary,extended,think') is True
assert bench.measured_groups_full('primary') is False
assert bench.measured_groups_full('primary,extended') is False
# Order must not matter; a full run in any order is still a full run.
assert bench.measured_groups_full('think,extended,primary') is True

# 2. A case with no golden entry counts as exact (that keeps adding a held-out
#    case non-breaking) but must never be invisible: missing_golden reports it.
assert bench.missing_golden({'a': 1, 'b': 2}, {'a': {'raw': '', 'tokens': 0}}) == 1
assert bench.missing_golden({'a': 1}, {'a': {'raw': '', 'tokens': 0}}) == 0
assert bench.missing_golden({}, {}) == 0

# 3. compare() really does pass a missing case - the behaviour the metric above
#    exists to expose - and really does fail a changed one.
exact, delta = bench.compare({'ghost': {'raw': 'x', 'tokens': 3}}, {'other': {}}, 't')
assert exact == 1 and delta == 0, 'a case with no golden currently counts exact'
exact, delta = bench.compare(
    {'c': {'raw': 'new', 'tokens': 5}},
    {'c': {'raw': 'old', 'tokens': 4}}, 't')
assert exact == 0 and delta == 1, 'a changed case must not count exact'

# 4. The frozen primary set is 6 cases and primary is what the metric means; a
#    change here is a benchmark change and should be a deliberate commit.
assert len(bench.CASES['primary']) == 6
assert {c['id'] for c in bench.CASES['primary']} == {
    'sampling5', 'timer60', 'status_heap', 'batch', 'heldout_timer45',
    'route_translate'}, 'primary set changed'

print('bench guard checks OK')
