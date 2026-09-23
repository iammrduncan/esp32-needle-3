#!/usr/bin/env python3
"""Decode benchmark and quality gate for the Needle 3 ESP32 firmware.

  bench.py device   [--port P] [--groups primary,extended,think] [--save-golden]
  bench.py host     [--save-golden]
  bench.py fidelity [--save-golden]

`device` drives the real request path over the console UART and reports the
timings the firmware measures itself (EVT done tps=...), so nothing in the host
bridge can inflate the number. `host` runs the same prompts through the host
build of the same engine (nd_dump genp) and compares the generated text against
a frozen golden file: that is the quality gate that keeps a speedup honest.

Golden file: {"cases": {id: {"raw": str, "tokens": int, "calls": [...]}}}
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Only the *case* groups. `isinstance(v, list)` alone also swallows `probe_ids`
# (a list of ints), which made measured_groups_full() compare
# {primary,extended,think} against a set that includes probe_ids - i.e. never true -
# so every --save-golden was refused and the device golden could not be updated.
CASES = {k: v for k, v in json.loads((ROOT / '.auto/prompts.json').read_text()).items()
         if isinstance(v, list) and v and isinstance(v[0], dict) and 'id' in v[0]}
TOOLS = str(ROOT / 'tools/demo-tools.json')
ROUTES = str(ROOT / 'tools/model-routes.json')
ND_DUMP = os.environ.get('ND_DUMP', str(ROOT / 'host/build/nd_dump'))
MODEL = str(ROOT / 'model/needle3.cact')
GOLDEN = {'device': ROOT / '.auto/golden/device.json',
          'host': ROOT / '.auto/golden/host.json'}


def metric(name, value):
    print(f'METRIC {name}={value}')


def mean(values):
    return sum(values) / len(values) if values else 0.0


def calls_match(actual, expect):
    """Name and arguments must match exactly, in order. No partial credit."""
    def norm(calls):
        return [(c.get('name'), c.get('arguments') or {}) for c in calls or []]
    return norm(actual) == norm(expect)


def compare(results, golden, tag):
    """Exactness against the frozen baseline: (exact_count, token_delta).

    A case with no golden entry is counted exact - that is what makes adding a
    held-out case non-breaking - but it is also a blind spot: a renamed or
    typo'd case id would compare against nothing and report a pass forever. The
    caller reports `golden_missing` so the number cannot hide.
    """
    exact, delta = 0, 0
    for cid, res in results.items():
        ref = golden.get(cid)
        if ref is None:
            exact += 1
            continue
        if res['raw'] == ref['raw'] and res['tokens'] == ref['tokens']:
            exact += 1
            continue
        delta += abs(res['tokens'] - ref['tokens'])
        print(f'DIVERGE[{tag}] {cid}: golden {ref["tokens"]}tok {ref["raw"][:70]!r}')
        print(f'DIVERGE[{tag}] {cid}: now    {res["tokens"]}tok {res["raw"][:70]!r}')
    return exact, delta


def missing_golden(results, golden):
    """How many measured cases were compared against nothing."""
    return sum(1 for cid in results if golden.get(cid) is None)



def save_golden(path, results):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(
        {'saved_at': time.strftime('%Y-%m-%dT%H:%M:%S'),
         'cases': {k: {m: v[m] for m in ('raw', 'tokens', 'calls')}
                   for k, v in results.items()}}, indent=1) + '\n')
    print(f'golden saved: {path} ({len(results)} cases)')


def switch_think(dev, want):
    """Set the firmware reasoning mode and wait for the ack that proves it took.

    Two measured reasons this cannot be a single fire-and-forget write:
    * the ack VALUE is checked, not just its presence. Run #345 recorded
      `think_tps=5.02` - a constrained-path number - because the toggle went
      unacknowledged and the suite carried on and measured the normal path. A
      monitor that silently reports the wrong path is worse than a failed run.
    * after ~15-20 requests in one attached session the board stops answering
      `!think` at all (run #155, reproduced #346: draining 27 stale lines cleared
      the backlog and it still would not ack), so a missing ack triggers one
      reconnect+handshake - which clears the console backlog without resetting the
      chip (DTR/RTS are pinned through the open) and re-establishes readiness -
      and only then fails loudly instead of reporting a bogus think_tps.
    """
    def ask():
        dev.think = want
        dev._set_think()
        until = time.monotonic() + 20
        while time.monotonic() < until:
            line = dev._line()
            if line.startswith('EVT think='):
                return line.split('=', 1)[1].strip()[:1] == str(int(want))
        return False

    if ask():
        return
    print('WARN think ack missing; reconnecting once (console fatigue, run #155)')
    dev._reconnect()
    dev._handshake(dev.request_timeout)
    if ask():
        return
    raise SystemExit(f'THINK_ACK_MISSING want={int(want)} after reconnect')


# ------------------------------------------------------------------- device

def measured_groups_full(groups):
    return set(groups.split(',')) == set(CASES)


def device_mode(args):
    sys.path.insert(0, str(ROOT / 'tools'))
    import serial_api

    boot = StringIO()
    with redirect_stdout(boot):
        dev = serial_api.Device(args.port, 115200, args.boot_timeout,
                                args.request_timeout, False)
    text = boot.getvalue()
    print(text.rstrip())

    for name, value in sorted(dict(re.findall(r'EVT prof (\S+)\s+([\d.]+) ms', text))
                              .items(), key=lambda kv: -float(kv[1])):
        print(f'PROF {name}={value}')

    results = {}
    # Console fatigue (runs #155 -> #345 -> #346 -> #368): after roughly 15-20
    # requests in one attached session the board stops answering, and the run dies
    # with a 600 s TimeoutError entering the next group. That killed three
    # canonical measurements in a row, so bound the session instead of rescuing
    # one symptom: every group after the first starts on a fresh attach. #346's
    # rescue only covers the !think ack, and a wedged console mid-group takes the
    # whole suite with it. Worst group is 10 requests, well under the observed
    # failure count, and _reconnect keeps the chip's state (DTR/RTS stay pinned),
    # so the primed prefixes and the warm model survive the re-attach.
    for gi, group in enumerate(args.groups.split(',')):
        if gi:
            print(f'### reconnect before group={group}')
            dev._reconnect()
            dev._handshake(dev.request_timeout)
        switch_think(dev, group == 'think')
        tps, ptps, ok, group_tps = [], [], 0, []
        print(f'### group={group} think={int(group == "think")}')
        sel = CASES[group]
        if args.cases:
            # Tail probe (run #392): the suite wedges in the last few cases, so a
            # diagnostic that has to replay 17 cases first costs 20 minutes per
            # hypothesis. Host-side filter only - it cannot change the flashed image.
            want = {c.strip() for c in args.cases.split(',') if c.strip()}
            sel = [c for c in CASES[group] if c['id'] in want]
            if sel:
                print(f'NOTE partial run: selected {",".join(c["id"] for c in sel)} '
                      f'of {len(CASES[group])} in group={group}; byte-exactness here '
                      f'covers these cases only, not the suite')
        for case in sel:
            if os.environ.get("AUTO_REATTACH_EACH") or os.path.exists(".auto/diag_reattach_each"):
                # DIAGNOSTIC for the mid-suite console wedge (#155, #345, #389-#395):
                # the board stops answering at roughly the same POSITION in the suite
                # regardless of candidate, image, board or request budget. Re-attaching
                # per CASE separates per-connection state (USB CDC / our reader) from
                # accumulated firmware state: if a fresh connection per case finishes
                # all 20 cases, the wedge belongs to the connection and canonical runs
                # get a workaround. Prompts, goldens, order and the firmware are
                # untouched, so the metric itself is unaffected - this changes only
                # session management. Note the group-boundary reconnect above already
                # exists; this is the per-case version of it.
                dev._reconnect()
                dev._handshake(dev.request_timeout)
                switch_think(dev, group == 'think')
                print(f'NOTE reattach before case={case["id"]}')
                # MEASURED WARNING (run #396): this path is NOT metric-neutral. With
                # it on, the same accepted image read primary 5.1033 against 5.1167
                # under canonical session management, i.e. -0.23 % = the size of the
                # keep bar. Each extra attach makes the firmware emit a think-ack and
                # a STATE line, and console emission is real CPU time on this part.
                # Diagnostic only: NEVER use it for a speed verdict.
            gap = float(os.environ.get("AUTO_CASE_GAP_S") or 0)
            if gap:
                # DIAGNOSTIC for the mid-suite wedge (#155,#345,#389-#396). Run #396
                # excluded the connection: 17 fresh attaches, 16 cases, and it still
                # wedged at the same position. Remaining split: console TX backlog
                # (a throughput effect, so an idle gap drains it and the suite
                # finishes) vs accumulated firmware state (a gap changes nothing).
                # The firmware times its own decode, so an idle gap between requests
                # cannot change decode_tps - this is metric-safe by construction.
                time.sleep(gap)
            r = dev.complete(case['input'], phase=case['phase'])
            # A blank expectation means "any grammar-legal, successfully executed
            # call is fine"; an exact list must match name and arguments.
            expect = case.get('expect')
            good = r['success'] and (calls_match(r['function_calls'], expect)
                                     if expect else True)
            ok += bool(good)
            results[case['id']] = {'raw': r['raw'], 'tokens': r['decode_tokens'] or 0,
                                   'calls': r['function_calls']}
            tps.append(r['decode_tps'] or 0.0)
            ptps.append(r['prefill_tps'] or 0.0)
            group_tps.append({'id': case['id'], 'tps': r['decode_tps'] or 0.0,
                              'tokens': r['decode_tokens'] or 0})
            # A case that never saw `EVT done` has None metrics; the aggregate lines
            # already use `or 0.0` and this one did not, so the FIRST such case used
            # to kill the whole suite mid-run with a TypeError (measured: the case
            # after the 128-token truncation, which cost the golden save and every
            # case after it). A failed case must be reported, not crash the bench.
            dt, dms, pt = (r['decode_tps'] or 0.0, r['decode_ms'] or 0.0,
                           r['prefill_tps'] or 0.0)
            print(f'CASE {case["id"]} phase={case["phase"]} tps={dt:.3f} '
                  f'tokens={r["decode_tokens"]} decode_ms={dms:.0f} '
                  f'prefill_tps={pt:.2f} calls_ok={int(good)} '
                  f'calls={json.dumps(r["function_calls"], separators=(",", ":"))[:90]}')
            if not good:
                print(f'  BAD {r["error"]} raw={r["raw"][:140]!r}')
        print(f'GROUP {group} mean_tps={mean(tps):.3f} calls_ok={ok}/{len(CASES[group])}')
        if group == 'primary':
            metric('decode_tps', round(mean(tps), 4))
            metric('prefill_tps', round(mean(ptps), 4))
            metric('gen_tokens', sum(c['tokens'] for c in group_tps))
            metric('min_case_tps', round(min(c['tps'] for c in group_tps), 4))
        elif group == 'extended':
            metric('ext_decode_tps', round(mean(tps), 4))
        elif group == 'think':
            metric('think_tps', round(mean(tps), 4))

    path = GOLDEN['device']
    golden = json.loads(path.read_text())['cases'] if path.is_file() else {}
    # The baseline may only be frozen by a run that covers the whole set. This has
    # to gate EVERY save, not just the first one: with the guard only on the empty
    # path, `AUTO_GROUPS=primary AUTO_SAVE=1` would overwrite a 14-case golden with
    # 6 cases, and the 8 dropped cases would then compare against nothing and pass
    # forever - silently destroying the campaign's quality baseline.
    if args.save_golden and not measured_groups_full(args.groups):
        print('REFUSING to save a partial golden; run all groups')
        args.save_golden = False
    if args.save_golden or not golden:
        save_golden(path, results)
        golden = results
    exact, delta = compare(results, golden, 'device')
    metric('device_output_exact', exact)
    metric('device_cases', len(results))
    metric('device_golden_missing', missing_golden(results, golden))
    # Reference is the 12-case golden; a short AUTO_GROUPS run only covers some
    # of it, so scale the pass criterion by the fraction actually measured.
    measured_cases = len(results)   # the selected set, whatever --groups/--cases did
    metric('device_cases_total', len(golden))
    if len(golden) > measured_cases:
        metric('device_output_exact_min', round(exact * len(golden) / measured_cases))
    metric('device_token_delta', delta)

    bench = re.search(r'EVT bench tokens=\d+ ms=\d+ ms_per_tok=(\S+) tps=(\S+)', text)
    if bench:
        print(f'BOOT ms_per_tok={bench.group(1)}')
        metric('boot_bench_tps', float(bench.group(2)))
    heaps = re.search(r'EVT ready .*psram_free=(\d+) internal_free=(\d+)', text)
    if heaps:
        metric('psram_free', int(heaps.group(1)))
        metric('internal_free', int(heaps.group(2)))
    dev.serial.close()


# --------------------------------------------------------------------- host

def host_case(case):
    """The firmware request path, mirrored in the host build of the engine."""
    schema = ROUTES if case['phase'] == 'route' else TOOLS
    cmd = [ND_DUMP, MODEL, 'genp', schema, case['input'], '128', 'nothink']
    if case['phase'] == 'route':
        cmd.append('onecall')
    out = subprocess.run(cmd, capture_output=True, text=True, timeout=900).stdout
    lines = out.split('\n')
    # The payload after the RAW marker is the full generated text and may span
    # several lines (the model emits literal newlines); it ends at END.
    start = next((i for i, ln in enumerate(lines) if ln.startswith('RAW ')), None)
    if start is None:
        raw = ''
    else:
        body = [lines[start][4:]] + lines[start + 1:]
        raw = '\n'.join(body[:body.index('END')] if 'END' in body else body)
    toks = re.search(r'EVT done tokens=(\d+)', out)
    return {'raw': raw, 'tokens': int(toks.group(1)) if toks else 0, 'calls': None}


def host_mode(args):
    results = {}
    for group in ('primary', 'extended'):
        if args.cases:
            # The flag means the same thing in both modes; a filter that silently
            # did nothing on the host would be the vacuous-control class.
            want = {c.strip() for c in args.cases.split(',') if c.strip()}
            cases = [c for c in CASES[group] if c['id'] in want]
            if not cases:
                continue
        else:
            cases = CASES[group]
        for case in cases:
            r = host_case(case)
            results[case['id']] = r
            print(f'HOST {case["id"]} tokens={r["tokens"]} raw={r["raw"][:90]!r}')
    path = GOLDEN['host']
    golden = json.loads(path.read_text())['cases'] if path.is_file() else {}
    if args.save_golden or not golden:
        save_golden(path, results)
        golden = results
    exact, delta = compare(results, golden, 'host')
    metric('host_output_exact', exact)
    metric('host_cases', len(results))
    metric('host_token_delta', delta)
    metric('host_golden_missing', missing_golden(results, golden))
    # Second, looser reading of the same run: does the model still pick the same
    # tokens? An accumulation-order change is allowed to break exact text at the
    # last mantissa bit, but it must not change a decision.
    same, lcp = 0, []
    for cid, res in results.items():
        ref = golden.get(cid)
        if ref is None:
            same += 1
            continue
        a, b = ref['raw'].split('\n'), res['raw'].split('\n')
        n = 0
        while n < len(a) and n < len(b) and a[n] == b[n]:
            n += 1
        lcp.append(f'{n}/{max(len(a), len(b))}')
        same += (a == b)
    metric('host_lines_same', same)
    print('LCP ' + ' '.join(f'{cid}:{v}' for cid, v in zip(results, lcp)))


# --------------------------------------------------------------- fidelity

# The probe sequence lives in prompts.json. `nd_dump logits` prints one block of
# logits per id, so the probe must be a true prefix of the frozen dump: run the
# ids in order and never compare a shorter run against a longer golden.
PROBE_IDS = json.loads((ROOT / '.auto/prompts.json').read_text())['probe_ids']


def fidelity_mode(args):
    # nd_ftest is the C-side gate: it opens the model, runs the probe and
    # compares against the frozen dump, exiting non-zero on drift.
    r = subprocess.run([ND_DUMP.replace('nd_dump', 'nd_ftest'), MODEL,
                        str(ROOT / '.auto/golden/logits.txt')] +
                       [str(i) for i in PROBE_IDS],
                       capture_output=True, text=True, timeout=900)
    line = (r.stdout or r.stderr).strip().splitlines()[-1] if (r.stdout or r.stderr) else 'no output'
    print(line)
    delta = re.search(r'max_delta=(\S+)', line)
    top1 = re.search(r'top1=(\d+)/(\d+)', line)
    metric('logit_max_delta', float(delta.group(1)) if delta else 999.0)
    metric('logit_top1_match', int(top1.group(1)) if top1 else 0)
    if r.returncode != 0:
        print('FIDELITY FAILED: forward pass drifted from the frozen baseline')



def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('mode', choices=['device', 'host', 'fidelity'])
    ap.add_argument('--port', default='/dev/ttyACM1')
    ap.add_argument('--groups', default='primary,extended,think')
    ap.add_argument('--cases', default='', help='comma list of case ids to run (diagnostics)')
    ap.add_argument('--boot-timeout', type=float, default=1500)
    ap.add_argument('--request-timeout', type=float, default=600)
    ap.add_argument('--save-golden', action='store_true')

    args = ap.parse_args()
    if args.mode == 'host':
        host_mode(args)
    elif args.mode == 'fidelity':
        fidelity_mode(args)
    else:
        device_mode(args)


if __name__ == '__main__':
    main()
