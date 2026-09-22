"""Guards for the two golden-integrity holes closed in run #296.

The byte-exact gate is the campaign's quality authority, so the ways it could
silently stop checking things matter more than a fraction of a percent of speed.
Run: .venv/bin/python .auto/test_bench_guards.py
"""
import hashlib
import importlib.util
import json
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

# 5. `primary` defines the metric and `probe_ids` defines the fidelity probe, but
#    checks.sh's frozen-input guard is a git diff over model/, tools/ and
#    partitions.csv - it does NOT cover .auto/prompts.json, so editing the prompt
#    set was invisible to every gate. Pinned here instead (sha256 of the canonical
#    JSON, first 16 hex). Adding a case to `extended` is sanctioned (run #296) and
#    must not trip this; editing, reordering or trimming `primary` must.
def _sha(obj):
    return hashlib.sha256(
        json.dumps(obj, sort_keys=True, ensure_ascii=False).encode()).hexdigest()[:16]


assert _sha([c['input'] for c in bench.CASES['primary']]) == '52d80c388c6caf26', \
    'primary prompt TEXT changed - this changes what the metric measures'
assert _sha([(c['id'], c['phase']) for c in bench.CASES['primary']]) == '84cc8dea3e0bd9ae', \
    'primary ids/phases changed - route vs tools phase changes the code path measured'
_prompts = json.loads((pathlib.Path(__file__).resolve().parent / 'prompts.json').read_text())
assert _sha(_prompts['probe_ids']) == 'b67f231b98f76efd', 'fidelity probe changed'

print('bench guard checks OK')
