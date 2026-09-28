#!/usr/bin/env python3
"""Non-hardware checks for the reproducible benchmark package."""

import subprocess
import unittest

from run_matrix import (CONFIG_PATH, MODEL_DIR, ROOT, check_assets, equal_calls,
                        checked_file, metrics, output_drift, read_json,
                        selected_layers, sha256)


class BenchmarkPackageTests(unittest.TestCase):
    def setUp(self):
        self.config = read_json(CONFIG_PATH)

    def test_all_image_hashes_and_tasks(self):
        self.assertEqual(len(check_assets(self.config)), 12)
        one_layer = read_json(ROOT / "benchmarks/one_layer_image.json")
        checked_file(one_layer)
        checked_file({"path": one_layer["bootloader"],
                      "sha256": one_layer["bootloader_sha256"]})

    def test_tasks_are_verbatim_project_cases(self):
        prompts = read_json(ROOT / ".auto/prompts.json")
        by_id = {case["id"]: case for group in ("primary", "extended")
                 for case in prompts[group]}
        for case in read_json(ROOT / self.config["tasks"])["cases"]:
            for key in ("phase", "input", "expect"):
                self.assertEqual(case[key], by_id[case["id"]][key])

    def test_slice_selection_and_eight_layer_hash(self):
        self.assertEqual(selected_layers(1), [0])
        self.assertEqual(selected_layers(8), [0, 4, 6, 9, 11, 14, 16, 19])
        reference = MODEL_DIR / "depth-08.cact"
        if reference.exists():
            self.assertEqual(sha256(reference), self.config["reference_model_sha256"])

    def test_one_layer_allocation_fix_preserves_host_logits(self):
        model = MODEL_DIR / "depth-01.cact"
        if not model.exists():
            self.skipTest("run benchmarks/run_matrix.py prepare first")
        original = ROOT / "benchmarks/assets/needle-matrix-nd_dump"
        patched = ROOT / "benchmarks/assets/needle-one-layer-nd_dump"
        self.assertEqual(sha256(original),
                         "06b667e63c9148a8b3aa8fc645da4d341e6d1fe440eb47a6910835e4252b2ea9")
        self.assertEqual(sha256(patched),
                         "6b36d802b03a4eb6ed6e4e1008ec5efd2d9862b23ac5eba3b6f3a04b62e7a381")
        probe = read_json(ROOT / ".auto/prompts.json")["probe_ids"]

        def dump(engine):
            return subprocess.check_output([str(engine), str(model), "logits",
                                            *map(str, probe)])

        self.assertEqual(dump(original), dump(patched))

    def test_archive_boundary_fix_preserves_eight_layer_host_logits(self):
        model = MODEL_DIR / "depth-08.cact"
        if not model.exists():
            self.skipTest("run benchmarks/run_matrix.py prepare first")
        original = ROOT / "benchmarks/assets/needle-b1w3-nd_dump"
        patched = ROOT / "benchmarks/assets/needle-matrix-nd_dump"
        self.assertEqual(sha256(original),
                         "fc67c0dc0d4b6351a302bdf2809893964e8ea33532bb95a46ed5ac552dc72387")
        probe = read_json(ROOT / ".auto/prompts.json")["probe_ids"]

        def dump(engine):
            return subprocess.check_output([str(engine), str(model), "logits",
                                            *map(str, probe)])

        self.assertEqual(dump(original), dump(patched))

    def test_exact_calls_including_empty_list(self):
        self.assertTrue(equal_calls([], []))
        self.assertFalse(equal_calls([{"name": "get_status", "arguments": {}}], []))
        self.assertFalse(equal_calls([{"name": "set_timer", "arguments": {"seconds": 5}}],
                                     [{"name": "set_timer", "arguments": {"seconds": 6}}]))

    def test_failures_do_not_score_and_missing_timing_is_visible(self):
        case = {"id": "x", "expect": []}
        row = {"case": case, "calls_exact": False,
               "result": {"success": False, "decode_tps": None, "prefill_tps": None}}
        summary = metrics([row])
        self.assertEqual(summary["exact_calls"], 0)
        self.assertEqual(summary["scored_cases"], 1)
        self.assertEqual(summary["decode_timed_cases"], 0)
        self.assertIsNone(summary["decode_tps_mean"])
        self.assertIsNone(summary["decode_tps_pooled"])

    def test_output_drift_requires_bytes_and_token_count(self):
        case = {"id": "x"}
        reference = [{"case": case, "result": {"raw": "same", "decode_tokens": 3,
                                               "function_calls": []}}]
        measured = [{"case": case, "result": {"raw": "same", "decode_tokens": 4,
                                              "function_calls": []}}]
        self.assertEqual(output_drift(measured, reference)["raw_and_tokens_exact"], 0)


if __name__ == "__main__":
    unittest.main()
