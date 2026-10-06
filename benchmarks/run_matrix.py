#!/usr/bin/env python3
"""Reproduce the ESP32-S3 baseline comparison and Needle 3 layer matrix.

Run in the needle-pi container: python3 benchmarks/run_matrix.py prepare,
then python3 benchmarks/run_matrix.py run --run-id NAME.  `run` launches three
independent board processes; each holds the board-pool flock for its whole lane.
"""

import argparse
import fcntl
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = ROOT / "benchmarks/config.json"
MODEL_DIR = ROOT / ".runtime/benchmarks/models"
RESULT_DIR = ROOT / "benchmarks/results"
sys.path.insert(0, str(ROOT / "tools"))

from fetch_assets import ensure as ensure_asset  # noqa: E402  (benchmarks/ is on sys.path)


def read_json(path):
    return json.loads(Path(path).read_text())


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    temp.replace(path)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def checked_file(spec):
    path = ensure_asset(ROOT / spec["path"])
    if not path.is_file():
        raise FileNotFoundError(f"missing {path}; see benchmarks/README.md")
    actual = sha256(path)
    if actual != spec["sha256"]:
        raise ValueError(f"SHA mismatch: {path}: {actual} != {spec['sha256']}")
    return path


def model_path(depth):
    return MODEL_DIR / f"depth-{depth:02d}.cact"


def prepare(config):
    from slice_cact import load, save, slice_model

    source_path = ROOT / config["source"]["path"]
    if not source_path.exists():
        pinned = read_json(ROOT / "model/manifest.json")
        if pinned["source_sha256"] != config["source"]["sha256"]:
            raise ValueError("source SHA differs between benchmark and model manifests")
        url = (f"https://huggingface.co/{pinned['repository']}/resolve/"
               f"{pinned['revision']}/{pinned['source_file']}")
        temporary = source_path.with_suffix(".download")
        print(f"DOWNLOAD {url}", flush=True)
        urllib.request.urlretrieve(url, temporary)
        if sha256(temporary) != pinned["source_sha256"]:
            raise ValueError("downloaded source SHA mismatch")
        temporary.replace(source_path)
    source = checked_file(config["source"])
    header, codebooks, tensors = load(source)
    if int(header[10]) != 20:
        raise ValueError("expected the published 20-layer source")
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    models = {}
    for depth in range(1, config["extension_limit"] + 1):
        sliced_header, sliced = slice_model(header, tensors, depth)
        sliced_header[13] = config["context"]
        sliced_header[16] = min(sliced_header[16], config["context"])
        target = model_path(depth)
        save(target, sliced_header, codebooks, sliced)
        size = target.stat().st_size
        if size > config["flash"]["model_partition_bytes"]:
            raise ValueError(f"depth {depth} exceeds the model partition")
        models[str(depth)] = {"bytes": size, "sha256": sha256(target),
                              "source_layers": selected_layers(depth)}
        print(f"MODEL depth={depth} bytes={size} sha256={models[str(depth)]['sha256']}", flush=True)
    if models["8"]["sha256"] != config["reference_model_sha256"]:
        raise ValueError("8-layer slice differs from the pinned model")
    write_json(MODEL_DIR.parent / "model-manifest.json", models)
    return models


def selected_layers(depth):
    from slice_cact import layer_order

    return [0] if depth == 1 else sorted(layer_order(20)[:depth])


def check_assets(config, tasks_path=None):
    checked_file(config["partition"])
    for image in config["images"].values():
        checked_file(image)
        checked_file({"path": image["bootloader"],
                      "sha256": image["bootloader_sha256"]})
    tasks = read_json(ROOT / (tasks_path or config["tasks"]))["cases"]
    ids = [case["id"] for case in tasks]
    if not tasks or len(ids) != len(set(ids)):
        raise ValueError("task IDs must be nonempty and unique")
    for case in tasks:
        if case["phase"] not in ("tools", "route") or not isinstance(case["expect"], list):
            raise ValueError(f"invalid task {case['id']}")
    return tasks


def job_info(config, name):
    if name == "baseline8":
        return "baseline", 8
    if name == "original8":
        return "original", 8
    match = re.fullmatch(r"current(\d+)", name)
    if not match:
        raise ValueError(f"unknown job {name}")
    depth = int(match.group(1))
    if depth not in range(1, config["extension_limit"] + 1):
        raise ValueError(f"invalid depth {depth}")
    return "current", depth


def flash(board, image, model, config):
    params = config["flash"]
    command = [params["python"], "-m", "esptool", "--chip", "esp32s3", "--port",
               board["flash"], "--baud", str(params["baud"]),
               "--before", "default_reset", "--after", "hard_reset",
               "write_flash", "--flash_mode", "dout", "--flash_size", "32MB",
               "--flash_freq", "80m",
               params["bootloader_offset"], str(ROOT / image["bootloader"]),
               params["partition_offset"], str(ROOT / config["partition"]["path"]),
               params["app_offset"], str(ROOT / image["path"]),
               params["model_offset"], str(model)]
    print("FLASH", " ".join(command), flush=True)
    subprocess.run(command, check=True)


def equal_calls(actual, expected):
    def normalized(calls):
        return [(call.get("name"), call.get("arguments") or {}) for call in calls]
    return normalized(actual) == normalized(expected)


def mean(values):
    return round(sum(values) / len(values), 4) if values else None


def metrics(rows):
    def values(key):
        return [r["result"][key] for r in rows if r["result"].get(key) is not None]
    scored = [r for r in rows if r["case"]["expect"] is not None]
    timed_decode = [(r["result"]["decode_tokens"], r["result"]["decode_ms"])
                    for r in rows if r["result"].get("decode_tokens") is not None
                    and r["result"].get("decode_ms", 0) > 0]
    return {
        "cases": len(rows), "success": sum(bool(r["result"].get("success")) for r in rows),
        "scored_cases": len(scored),
        "exact_calls": sum(bool(r["calls_exact"]) for r in scored),
        "decode_tps_mean": mean(values("decode_tps")),
        "decode_tps_pooled": (round(1000 * sum(tokens for tokens, _ in timed_decode) /
                                    sum(ms for _, ms in timed_decode), 4)
                              if timed_decode else None),
        "prefill_tps_mean": mean(values("prefill_tps")),
        "decode_timed_cases": len(values("decode_tps")),
        "prefill_timed_cases": len(values("prefill_tps")),
        "decode_ms_mean": mean(values("decode_ms")),
        "prefill_ms_mean": mean(values("prefill_ms")),
        "latency_ms_mean": mean(values("latency_ms")),
        "decode_tokens_sum": sum(values("decode_tokens")),
        "retried": sum(bool(r["result"].get("retried")) for r in rows),
    }


def one_job(config, board_id, name, run_id, tasks, tasks_path):
    from serial_api import Device

    class BenchDevice(Device):
        """Fail immediately on a terminal boot error, not after 750 seconds."""

        booting = True

        def _line(self):
            line = super()._line()
            if self.booting and (line.startswith(("ERR model_open", "ERR mmap_failed",
                                                 "ERR model_partition", "Guru Meditation Error"))):
                raise RuntimeError(f"firmware boot failed: {line}")
            return line

    image_name, depth = job_info(config, name)
    board = config["boards"][board_id]
    image = config["images"][image_name]
    model = model_path(depth)
    manifest = read_json(MODEL_DIR.parent / "model-manifest.json")
    if sha256(model) != manifest[str(depth)]["sha256"]:
        raise ValueError(f"slice hash changed: {model}")
    task_hash = sha256(ROOT / tasks_path)
    result_path = RESULT_DIR / run_id / f"{name}.json"
    identity = {"job": name, "board": board_id, "depth": depth,
                "image": image_name, "image_sha256": image["sha256"],
                "model_sha256": manifest[str(depth)]["sha256"],
                "tasks_sha256": task_hash}
    if result_path.exists():
        old = read_json(result_path)
        same = all(old.get(k) == v for k, v in identity.items())
        if not same:
            raise FileExistsError(f"existing job identity differs: {result_path}")
        if old.get("status") == "completed":
            print(f"SKIP completed {name}", flush=True)
            return True
        if name == "current1" and old.get("status") == "failed":
            print("SKIP known one-layer diagnostic failure", flush=True)
            return False
        if old.get("status") not in ("failed", "interrupted", "flashing", "booting", "measuring"):
            raise FileExistsError(f"unrecognized existing job state: {result_path}")
        attempts = result_path.parent / "attempts"
        attempts.mkdir(parents=True, exist_ok=True)
        stamp = re.sub(r"[^0-9TZ]", "", old.get("started_utc", "unknown"))
        backup = attempts / f"{name}-{stamp}.json"
        if backup.exists():
            raise FileExistsError(f"prior attempt already exists: {backup}")
        result_path.replace(backup)
        print(f"ARCHIVE incomplete {name} -> {backup}", flush=True)
    result = {**identity, "status": "flashing", "started_utc": utc_now(),
              "model_bytes": model.stat().st_size, "rows": []}
    write_json(result_path, result)
    dev = None
    try:
        flash(board, image, model, config)
        result["status"] = "booting"
        write_json(result_path, result)
        dev = BenchDevice(board["console"], 115200, config["boot_timeout_seconds"],
                          config["request_timeout_seconds"], False)
        dev.booting = False
        state = dev.state()
        result["boot_state"] = state
        if state.get("layers") != depth or state.get("model_bytes") != model.stat().st_size:
            raise RuntimeError(f"wrong model on board: {state}")
        result["status"] = "measuring"
        write_json(result_path, result)
        last_phase = None
        for case in tasks:
            if last_phase is not None and case["phase"] != last_phase:
                dev._reconnect()
                dev._handshake(config["request_timeout_seconds"])
            last_phase = case["phase"]
            try:
                measured = dev.complete(case["input"], phase=case["phase"])
            except Exception as exc:
                measured = {"success": False, "error": f"{type(exc).__name__}: {exc}",
                            "function_calls": [], "raw": "", "decode_tokens": None}
                result["rows"].append({"case": case, "result": measured, "calls_exact": False})
                write_json(result_path, result)
                print(f"CASE {name} {case['id']} ERROR {exc}", flush=True)
                raise
            exact = bool(measured["success"] and equal_calls(measured["function_calls"], case["expect"]))
            result["rows"].append({"case": case, "result": measured, "calls_exact": exact})
            result["metrics"] = metrics(result["rows"])
            write_json(result_path, result)
            print(f"CASE {name} {case['id']} exact={int(exact)} "
                  f"prefill_tps={measured['prefill_tps']} decode_tps={measured['decode_tps']} "
                  f"tokens={measured['decode_tokens']}", flush=True)
        result["metrics"] = metrics(result["rows"])
        result["status"] = "completed"
        result["finished_utc"] = utc_now()
        write_json(result_path, result)
        print(f"COMPLETE {name} {result['metrics']}", flush=True)
        return True
    except Exception as exc:
        result["status"] = "failed"
        result["error"] = f"{type(exc).__name__}: {exc}"
        result["finished_utc"] = utc_now()
        write_json(result_path, result)
        print(f"FAILED {name}: {result['error']}", flush=True)
        return False
    finally:
        if dev is not None:
            dev.serial.close()


def utc_now():
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def run_board(config, board_id, jobs, run_id, tasks_path=None):
    board = config["boards"][board_id]
    for key in ("flash", "console"):
        if not Path(board[key]).exists():
            raise FileNotFoundError(board[key])
    lock_path = Path(board["lock"])
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    tasks_path = tasks_path or config["tasks"]
    tasks = check_assets(config, tasks_path)
    with lock_path.open("a+") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError(f"board {board_id} busy; not bypassing board-pool lock")
        failed = False
        for name in jobs:
            good = one_job(config, board_id, name, run_id, tasks, tasks_path)
            if not good:
                failed = True
                if name == "current1":
                    print("CONTINUE after requested 1-layer diagnostic failure", flush=True)
                    continue
                if job_info(config, name)[1] > 8:
                    print("STOP extension at first device failure", flush=True)
                raise RuntimeError(f"board {board_id} failed job {name}; stopping lane")
        if failed:
            raise RuntimeError(f"board {board_id} finished with one or more failed jobs")


def launch(config, run_id, extension, matrix_only, compare_only, extension_board):
    check_assets(config)
    if not (MODEL_DIR.parent / "model-manifest.json").is_file():
        raise FileNotFoundError("run `prepare` first")
    depths = config["matrix"]
    if not depths or len(depths) != len(set(depths)) or any(
            not isinstance(depth, int) or depth < 1 or depth > config["extension_limit"]
            for depth in depths):
        raise ValueError("config.matrix needs unique supported depths")
    if extension:
        lanes = {board: ([f"current{n}" for n in range(max(depths) + 1,
                                                            config["extension_limit"] + 1)]
                         if board == extension_board else []) for board in config["boards"]}
    elif compare_only:
        lanes = {"1": ["baseline8", "original8"], "2": [], "3": []}
    elif matrix_only:
        lanes = {"1": [], "2": [f"current{n}" for n in depths[::2]],
                 "3": [f"current{n}" for n in depths[1::2]]}
    else:
        remaining = [n for n in depths if n != 8]
        lanes = {"1": ["baseline8", "current8"],
                 "2": [f"current{n}" for n in remaining[::2]],
                 "3": [f"current{n}" for n in remaining[1::2]]}
    result_root = RESULT_DIR / run_id
    result_root.mkdir(parents=True, exist_ok=True)
    run_config_path = result_root / "run-config.json"
    task_hash = sha256(ROOT / config["tasks"])
    if run_config_path.exists():
        prior = read_json(run_config_path)
        if prior.get("config") != config or prior.get("tasks_sha256") != task_hash:
            raise ValueError("run ID already exists with a different config or task set")
    else:
        write_json(run_config_path, {"config": config, "tasks_sha256": task_hash,
                                     "started_utc": utc_now(), "extension": extension,
                                     "matrix_only": matrix_only, "compare_only": compare_only})
    processes = []
    for board_id, jobs in lanes.items():
        if not jobs:
            continue
        log = (result_root / f"board{board_id}{'-extension' if extension else ''}.log").open("a")
        command = [sys.executable, str(Path(__file__).resolve()), "run-board", "--board", board_id,
                   "--run-id", run_id, "--jobs", *jobs]
        proc = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, cwd=ROOT)
        processes.append((board_id, proc, log))
        print(f"BOARD {board_id} pid={proc.pid} jobs={','.join(jobs)} log={log.name}", flush=True)
    failures = []
    for board_id, proc, log in processes:
        code = proc.wait()
        log.close()
        print(f"BOARD {board_id} exit={code}", flush=True)
        if code:
            failures.append(board_id)
    if failures:
        raise RuntimeError(f"board worker failed: {failures}")


def output_drift(rows, reference_rows):
    reference = {row["case"]["id"]: row["result"] for row in reference_rows}
    paired = [(row["result"], reference[row["case"]["id"]]) for row in rows
              if row["case"]["id"] in reference]
    return {"paired_cases": len(paired),
            "raw_and_tokens_exact": sum(a.get("raw") == b.get("raw") and
                                         a.get("decode_tokens") == b.get("decode_tokens")
                                         for a, b in paired),
            "calls_same": sum(equal_calls(a.get("function_calls") or [],
                                          b.get("function_calls") or []) for a, b in paired)}


def summarize(run_id, extra_run_ids=(), reference_run_id=None):
    directory = RESULT_DIR / run_id
    reference_path = RESULT_DIR / (reference_run_id or run_id) / "current8.json"
    reference = read_json(reference_path) if reference_path.exists() else None
    task_hashes = set()
    summaries = []
    for source_run in (run_id, *extra_run_ids):
        for path in sorted((RESULT_DIR / source_run).glob("*.json")):
            result = read_json(path)
            if not isinstance(result, dict) or "job" not in result:
                continue
            task_hashes.add(result["tasks_sha256"])
            item = {"run_id": source_run, "job": result["job"], "board": result["board"],
                    "depth": result["depth"], "status": result["status"],
                    "image_sha256": result["image_sha256"],
                    "model_bytes": result.get("model_bytes"),
                    "free_psram_bytes": result.get("boot_state", {}).get("free_psram_bytes"),
                    "free_internal_bytes": result.get("boot_state", {}).get("free_internal_bytes"),
                    **metrics(result.get("rows", []))}
            if reference is not None and result.get("rows"):
                item["device_drift_vs_current8"] = output_drift(result["rows"], reference["rows"])
            summaries.append(item)
    if len(task_hashes) > 1:
        raise ValueError("cannot combine runs with different task sets")
    write_json(directory / "summary.json", summaries)
    for item in summaries:
        print(json.dumps(item, sort_keys=True))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("prepare")
    run = commands.add_parser("run")
    run.add_argument("--run-id", required=True)
    run.add_argument("--extension", action="store_true")
    run.add_argument("--extension-board", choices=("1", "2", "3"), default="2")
    run.add_argument("--matrix-only", action="store_true")
    run.add_argument("--compare-only", action="store_true")
    worker = commands.add_parser("run-board")
    worker.add_argument("--board", required=True, choices=("1", "2", "3"))
    worker.add_argument("--run-id", required=True)
    worker.add_argument("--jobs", nargs="+", required=True)
    worker.add_argument("--tasks", default=None, help="alternate task JSON, relative to repo root")
    worker.add_argument("--image-spec", default=None,
                        help="alternate current image JSON; allowed only for a single current1 diagnostic")
    summary = commands.add_parser("summarize")
    summary.add_argument("--run-id", required=True)
    summary.add_argument("--extra-run-id", action="append", default=[])
    summary.add_argument("--reference-run-id", default=None)
    args = parser.parse_args()
    config = read_json(CONFIG_PATH)
    if args.command == "prepare":
        prepare(config)
    elif args.command == "run":
        if sum((args.extension, args.matrix_only, args.compare_only)) > 1:
            parser.error("--extension, --matrix-only and --compare-only are mutually exclusive")
        launch(config, args.run_id, args.extension, args.matrix_only,
               args.compare_only, args.extension_board)
    elif args.command == "run-board":
        if args.image_spec:
            if args.jobs != ["current1"]:
                parser.error("--image-spec is only allowed with --jobs current1")
            config["images"]["current"] = read_json(ROOT / args.image_spec)
        run_board(config, args.board, args.jobs, args.run_id, args.tasks)
    else:
        summarize(args.run_id, args.extra_run_id, args.reference_run_id)


if __name__ == "__main__":
    main()
