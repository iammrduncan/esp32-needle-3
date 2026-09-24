#!/bin/bash
# Mechanism proof for the expbc candidate: the ROM memcpy calls must actually be gone.
#
# A candidate that only changes a header can be silently re-taken by the compiler, so the
# evidence is the disassembly, not the source diff. The CONTROL object is the accepted
# engine/src/nd_model.c compiled by the SAME command line from the SAME build tree, with the
# accepted header shadowed in ahead of the candidate's - this isolates the one edit.
#
#   elf_proof.sh <worker>            e.g. elf_proof.sh /root/board-pool/board2
set -uo pipefail
W=${1:?worker path}
CC_JSON=$W/esp32/build/compile_commands.json
[ -f "$CC_JSON" ] || { echo "NO_COMPILE_COMMANDS $CC_JSON"; exit 1; }
MAIN=/workspace/esp32-needle-3
export SYSRC=${2:-engine/src/nd_model.c}
. /opt/esp/idf/export.sh >/dev/null 2>&1

mkdir -p /tmp/exp50_ctl/engine/include
( cd "$MAIN" && git show HEAD:engine/include/nd_quant.h ) > /tmp/exp50_ctl/engine/include/nd_quant.h
( cd "$MAIN" && git show "HEAD:$SYSRC" ) > /tmp/exp50_ctl/$(basename "$SYSRC")
for h in "$MAIN"/engine/include/*.h; do cp "$h" /tmp/exp50_ctl/engine/include/ 2>/dev/null; done
cp /tmp/exp50_ctl/engine/include/nd_quant.h /tmp/exp50_ctl/nq_accept.h
printf '#include "nd_quant.h"\n' >/dev/null

CMD=$(python3 - "$CC_JSON" <<'PY'
import json,os,shlex,sys
for e in json.load(open(sys.argv[1])):
    if e["file"].endswith(os.environ["SYSRC"]):
        a=shlex.split(e["command"]); out=a[:1]; i=1
        SHADOW=["-I/tmp/exp50_ctl/engine/include","-I/tmp/exp50_ctl"]
        # The shadow include MUST precede the tree's own -I flags: appended at the end, the
        # candidate header wins and the "control" silently re-compiles the candidate (measured:
        # CTL and CAND both reported 114 memcpy sites, which looked like a null candidate).
        while i < len(a):                     # drop the original -c <src> -o <obj> pair
            if a[i] in ("-c", "-o"): i += 2; continue
            out.append(a[i]); i += 1
        out=out[:1]+SHADOW+out[1:]
        print(e["directory"]); print(shlex.join(out)); break
PY
)
DIR=$(printf '%s\n' "$CMD" | head -1); CMD=$(printf '%s\n' "$CMD" | tail -1)
[ -n "$CMD" ] || { echo "NO_ND_MODEL_COMPILE_COMMAND"; exit 1; }
CTL="$CMD -c /tmp/exp50_ctl/$(basename "$SYSRC") -o /tmp/exp50_ctl/ctl.o"
( cd "$DIR" && eval "$CTL" ) || { echo "CTL_COMPILE_FAILED"; exit 1; }
CAND=$(find "$W/esp32/build" -name "$(basename "$SYSRC").obj" | head -1)
[ -f "$CAND" ] || { echo "NO_CANDIDATE_OBJECT"; exit 1; }

for tag in CTL CAND; do
    obj=/tmp/exp50_ctl/ctl.o; [ $tag = CAND ] && obj=$CAND
    xtensa-esp32s3-elf-objdump -dr "$obj" > /tmp/exp50_ctl/$tag.asm
    # Relocatable objects name the target through a relocation, so `-dr` + a bare
    # `memcpy` is the pattern; `<memcpy>` (the linked-ELF form) matches nothing here.
    n=$(grep -c 'memcpy' /tmp/exp50_ctl/$tag.asm)
    sig=$(awk '/<sigmoidf_pair>:/,/^[0-9a-f]+ <[^>]+>:$/' /tmp/exp50_ctl/$tag.asm | grep -c 'memcpy')
    echo "$tag src=$SYSRC obj=$obj memcpy_sites=$n memcpy_sites_in_sigmoidf_pair=$sig"
done
grep -c "__builtin\|memcpy" "$W/engine/include/nd_quant.h" | sed 's/^/candidate_header_memcpy_lines=/'
