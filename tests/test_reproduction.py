"""Guard the archived campaign evidence needed by a fresh clone."""

import hashlib
import json
from pathlib import Path
import re
import tarfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
TREES = ROOT / ".auto/trees"


def provenance_digest(snapshot):
    paths = []
    for directory, suffix in (("engine/src", ".c"), ("engine/src", ".S"),
                              ("engine/include", ".h"), ("esp32/main", ".c")):
        group = sorted((snapshot / directory).glob(f"*{suffix}"))
        if not group:
            raise AssertionError(f"missing {directory}/*{suffix} in {snapshot}")
        paths.extend(group)
    digest = hashlib.md5(usedforsecurity=False)  # match the measured PROV_ENGINE format
    for path in paths:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
    return digest.hexdigest()


class ReproductionArchiveTests(unittest.TestCase):
    SNAPSHOTS = {
        "final-b1w3-source": ("1ada94b5b3d0c038f5f47e80006958df", "B1w3.log"),
        "final-matrix-source": ("1bd2ba7c68ce57848a92d3406abf9612", None),
        "final-one-layer-source": ("b8b3b60013fd92c702bb5ee50f6a07f2", None),
        "final-b2resid2-source": ("d2b3c85ae2e45eee69a6de7393ea0c7d", "B2resid2.log"),
        "final-b3p1fix-source": ("9e4ca21af947fbde703efd702f867614", "B3p1fix.log"),
    }

    def test_final_source_matches_recorded_hardware_provenance(self):
        for name, (expected, log_name) in self.SNAPSHOTS.items():
            with self.subTest(name=name):
                self.assertEqual(provenance_digest(TREES / name), expected)
                if log_name:
                    log = (ROOT / ".auto/final-logs" / log_name).read_text()
                    found = re.search(r"^PROVENANCE .* engine_md5=([0-9a-f]{32})$",
                                      log, re.MULTILINE)
                    self.assertIsNotNone(found)
                    self.assertEqual(found.group(1), expected)

    def test_final_gate_and_run_records_are_present(self):
        logs = ROOT / ".auto/final-logs"
        self.assertIn("METRIC decode_tps=6.22", (logs / "B1w3.log").read_text())
        self.assertIn("HOSTGATE_RC=0", (logs / "HOSTGATE-B1w3dev.log").read_text())
        self.assertIn("METRIC decode_tps=6.19", (logs / "B2resid2.log").read_text())
        self.assertIn("METRIC decode_tps=6.2033", (logs / "B3p1fix.log").read_text())

    def test_run_ledger_is_complete_and_identity_map_is_preserved(self):
        rows = [json.loads(line) for line in
                (ROOT / ".auto/log.jsonl").read_text().splitlines()]
        self.assertEqual([row["run"] for row in rows if "run" in row],
                         list(range(1, 944)))
        mapping = [line.split() for line in
                   (ROOT / ".auto/identity-rewrite-map.tsv").read_text().splitlines()]
        self.assertEqual(len(mapping), 509)
        self.assertTrue(all(len(pair) == 2 and all(len(sha) == 40 for sha in pair)
                            for pair in mapping))
        self.assertEqual(sum(old != new for old, new in mapping), len(mapping))

    def test_archives_open_and_baseline_source_is_complete(self):
        archives = ROOT / ".auto/archives"
        for archive in archives.glob("*.tar.gz"):
            with self.subTest(archive=archive.name), tarfile.open(archive, "r:gz") as data:
                names = data.getnames()
                self.assertTrue(names)
                if archive.name.startswith("baseline-source-"):
                    self.assertIn("engine/src/nd_model.c", names)
                    self.assertIn("esp32/main/main.c", names)
                if archive.name.startswith("led-identify-source-"):
                    self.assertIn("led-identify/engine/src/nd_model.c", names)
                    self.assertNotIn("led-identify/esp32/build/needle_demo.bin", names)
                if archive.name.startswith("research-checkpoints-code-"):
                    self.assertTrue(any(name.endswith("/working-tree.patch") for name in names))
                    self.assertFalse(any(name.endswith("/pi-session.jsonl") for name in names))


if __name__ == "__main__":
    unittest.main()
