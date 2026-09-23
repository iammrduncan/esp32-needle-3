#!/bin/bash
# Experiment 39 - the engine's own optimization level, which has never been varied.
#
#   variant o3        : accepted engine + -O3 on the needle component
#   variant o3bundle  : run #371's bundle            + -O3 on the needle component
#
# Why this is a real hypothesis and not a knob tour: every engine file compiles at
# -O2 (verified in compile_commands.json, not assumed), and the campaign's single
# most productive cheap family has been build configuration - the 64 B data cache
# line (+11.9 %, #238) and the 100 Hz tick (+0.34 %, #240). -O3 is exactly the
# mechanism class that produced the campaign's last kept win (#362's manual unroll,
# +0.332 %): more independent work in flight per loop. It is bit-exact by
# definition, because GCC may not reassociate floating point without -ffast-math,
# so the only thing that can move is scheduling and inlining.
#
# The flag is applied through the component's CMakeLists, NOT -DCMAKE_C_FLAGS:
# a flag already present in CMakeCache makes ninja see no change and relink the
# same binary, which is how several "identical md5 for every variant" results were
# produced early in this campaign. Each variant gets its own fresh build dir and
# the flag is proven on the compile line before anything is flashed.
set -uo pipefail
N=${1:?usage: launch.sh <board> <o3|o3bundle>}
V=${2:?variant}
MAIN=/workspace/esp32-needle-3
R=/root/board-pool/board$N
BATCH=${BATCH:?}
. /opt/esp/idf/export.sh > /dev/null 2>&1

[ -d "$R" ] || { echo "no worker $R"; exit 1; }
git -C "$MAIN" show HEAD:engine/src/nd_model.c > "$R/engine/src/nd_model.c" || exit 1
cp /tmp/ndq_rescale_only.c "$R/engine/src/nd_quant.c" || exit 1
cp "$MAIN/.auto/bench.py" "$R/.auto/bench.py"
cp "$MAIN/.auto/measure.sh" "$R/.auto/measure.sh"
cp "$MAIN/.auto/checks.sh" "$R/.auto/checks.sh"

if [ "$V" = o3bundle ]; then
  cp /tmp/ndm_bundle.c "$R/engine/src/nd_model.c" || exit 1
  cp /tmp/ndq_bundle.c "$R/engine/src/nd_quant.c" || exit 1
fi
# -O3 on the engine component only; esp32/main (the row splitter) stays at -O2 so
# the comparison is about engine codegen, not about the scheduler glue.
cp "$MAIN/esp32/components/needle/CMakeLists.txt" "$R/esp32/components/needle/CMakeLists.txt" || exit 1
grep -q 'EXPERIMENT-39' "$R/esp32/components/needle/CMakeLists.txt" ||
  printf '\n# EXPERIMENT-39: engine codegen screen, -O3 on this component only.\ntarget_compile_options(${COMPONENT_LIB} PRIVATE -O3)\n' \
    >> "$R/esp32/components/needle/CMakeLists.txt"

rm -f "$R/esp32/sdkconfig"
{
  echo "start=$(date -u +%FT%TZ) variant=$V"
  echo "nd_model_md5=$(md5sum < "$R/engine/src/nd_model.c" | cut -c1-12)"
  echo "nd_quant_md5=$(md5sum < "$R/engine/src/nd_quant.c" | cut -c1-12)"
  echo "component_cmake_md5=$(md5sum < "$R/esp32/components/needle/CMakeLists.txt" | cut -c1-12)"
} > "$BATCH/lane$N.log"

# Prove the flag reaches the engine files before spending 13 minutes on a board.
( cd "$R" && bash .auto/measure.sh ) >> "$BATCH/lane$N.log" 2>&1
rc=$?
echo "MEASURE rc=$rc" >> "$BATCH/lane$N.log"
exit $rc
