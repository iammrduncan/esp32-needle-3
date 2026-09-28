#!/usr/bin/env python3
"""Measure host logit drift of each sliced model against the pinned 8-layer rung.

This is cross-depth model drift, not ESP32 numerical error. A separate check
compares the 8-layer host engine to the project's frozen numerical golden.
"""

import argparse
import json
import math
import subprocess
from pathlib import Path

from run_matrix import MODEL_DIR, RESULT_DIR, ROOT, read_json, sha256, write_json

VOCAB = 8192
ENGINE = ROOT / "benchmarks/assets/needle-matrix-nd_dump"
ENGINE_SHA = "06b667e63c9148a8b3aa8fc645da4d341e6d1fe440eb47a6910835e4252b2ea9"


def logits(model, probe):
    command = [str(ENGINE), str(model), "logits", *map(str, probe)]
    output = subprocess.check_output(command, text=True)
    # The preserved B1w3 binary prints two instrumentation diagnostics; reject
    # all other nonnumeric output rather than letting a partial dump look valid.
    lines = [line for line in output.splitlines()
             if not line.startswith(("EG2 ok=", "SINKFIX calls="))]
    values = [float(line) for line in lines]
    expected = VOCAB * len(probe)
    if len(values) != expected or not all(math.isfinite(x) for x in values):
        raise ValueError(f"{model}: expected {expected} finite logits, got {len(values)}")
    return values


def compare(values, reference, probe):
    deltas = [abs(a - b) for a, b in zip(values, reference)]
    top1 = []
    for step in range(len(probe)):
        start, stop = step * VOCAB, (step + 1) * VOCAB
        actual = max(range(start, stop), key=lambda i: values[i]) - start
        target = max(range(start, stop), key=lambda i: reference[i]) - start
        top1.append({"step": step, "token": probe[step], "actual": actual,
                     "reference": target, "same": actual == target})
    return {"mean_abs": round(sum(deltas) / len(deltas), 6),
            "max_abs": round(max(deltas), 6),
            "rmse": round(math.sqrt(sum(x * x for x in deltas) / len(deltas)), 6),
            "top1_same": sum(item["same"] for item in top1), "top1_total": len(top1),
            "top1": top1}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--max-depth", type=int, default=8)
    args = parser.parse_args()
    if sha256(ENGINE) != ENGINE_SHA:
        raise ValueError("host engine SHA mismatch")
    manifest = read_json(MODEL_DIR.parent / "model-manifest.json")
    probe = read_json(ROOT / "benchmarks/prompts.json")["probe_ids"]
    reference_model = MODEL_DIR / "depth-08.cact"
    if sha256(reference_model) != manifest["8"]["sha256"]:
        raise ValueError("reference model SHA mismatch")
    reference = logits(reference_model, probe)
    golden = [float(line) for line in (ROOT / "benchmarks/golden/logits.txt").read_text().splitlines()]
    if len(golden) != len(reference):
        raise ValueError("frozen golden length differs")
    result = {"engine_sha256": ENGINE_SHA, "probe_ids": probe,
              "reference_model_sha256": manifest["8"]["sha256"],
              "frozen_golden": compare(reference, golden, probe), "depths": {}}
    output = RESULT_DIR / args.run_id / "host-drift.json"
    for depth in range(1, args.max_depth + 1):
        model = MODEL_DIR / f"depth-{depth:02d}.cact"
        if sha256(model) != manifest[str(depth)]["sha256"]:
            raise ValueError(f"model SHA mismatch at depth {depth}")
        try:
            values = reference if depth == 8 else logits(model, probe)
        except subprocess.CalledProcessError as exc:
            result["depths"][str(depth)] = {"model_sha256": manifest[str(depth)]["sha256"],
                                             "status": "failed", "error": f"host nd_dump exited {exc.returncode}"}
            write_json(output, result)
            print(f"DEPTH {depth} FAILED {exc}", flush=True)
            continue
        result["depths"][str(depth)] = {"model_sha256": manifest[str(depth)]["sha256"],
                                         "status": "completed", **compare(values, reference, probe)}
        write_json(output, result)
        row = result["depths"][str(depth)]
        print(f"DEPTH {depth} mean_abs={row['mean_abs']} max_abs={row['max_abs']} "
              f"top1={row['top1_same']}/{row['top1_total']}", flush=True)
    print(f"FROZEN-GOLDEN {result['frozen_golden']}")


if __name__ == "__main__":
    main()
