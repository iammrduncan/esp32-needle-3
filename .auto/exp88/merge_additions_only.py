#!/usr/bin/env python3
"""Keep a device-golden save ADDITIONS-ONLY.

`bench.py device --save-golden` replaces the whole file, so a coverage session would
also re-capture heldout_interval_one and heldout_long_tools_note_only - the two cases
that encode the firmware's demo timer and sampling counters and diverge on every board
(#551, #574, #589). Regenerating those would move the quality oracle rather than meet
it, which the mentor queue forbids explicitly. Restore every pre-existing entry from
the backup and keep only ids that did not exist before, then report exactly what was
added and which pre-existing entries disagreed with this boot.

Usage: python3 .auto/exp88/merge_additions_only.py <backup_dir>
"""
import json, pathlib, sys

bak = pathlib.Path(sys.argv[1])
old = json.loads((bak / 'device.json').read_text())['cases']
new = json.loads(pathlib.Path('.auto/golden/device.json').read_text())['cases']

added = [k for k in new if k not in old]
changed = [k for k in old if k in new and new[k] != old[k]]
missing = [k for k in old if k not in new]

merged = dict(old)                      # pre-existing goldens, byte-preserved
merged.update({k: new[k] for k in added})
if missing:
    print(f"REFUSE session did not cover {len(missing)} pre-existing cases: {missing[:4]}")
    sys.exit(3)                          # a partial session must not widen the golden

pathlib.Path('.auto/golden/device.json').write_text(json.dumps(
    {'saved_at': json.loads(pathlib.Path('.auto/golden/device.json').read_text())
      .get('saved_at', ''), 'cases': merged}, indent=1) + "\n")
print(f"added={len(added)} {added}")
print(f"pre_existing_preserved={len(old)} pre_existing_disagreed_with_this_boot={len(changed)} {changed[:4]}")
