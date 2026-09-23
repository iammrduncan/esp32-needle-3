#!/bin/bash
# Launch the three Experiment 30 lanes, one per board, each under its own lock.
#   .auto/exp30/launch.sh <batch-dir>
# Written as a file because nested tmux/needle-board/bash -c quoting is how a
# lane silently fails to start (run #347: 0 panes, 14 s of dead scheduler time).
set -uo pipefail
DIR=${1:?usage: launch.sh <batch-dir>}
mkdir -p "$DIR"
for n in 1 2 3; do
    : > "$DIR/lane$n.log"
    tmux kill-session -t "e31$n" 2>/dev/null
    cat > "/tmp/e31lane$n.sh" <<EOF
#!/bin/bash
{
  echo "start=\$(date -u +%FT%TZ)"
  /root/bin/needle-board run $n -- bash -c "EXP30=0 EXP31=1 LANE_OUT=$DIR/lane$n.out bash .auto/exp29/lane.sh $n"
  echo "LANE rc=\$?"
  sleep 60
} >> "$DIR/lane$n.log" 2>&1
EOF
    chmod +x "/tmp/e31lane$n.sh"
    tmux new-session -d -s "e31$n" -c "/root/board-pool/board$n" "/tmp/e31lane$n.sh"
done
sleep 12
echo "panes=$(tmux ls 2>/dev/null | grep -c e31)"
echo "build_pids=$(pgrep -f 'idf.py|ninja' | wc -l)"
for n in 1 2 3; do
    echo "lane$n log_bytes=$(wc -c < "$DIR/lane$n.log") out_bytes=$(wc -c < "$DIR/lane$n.out" 2>/dev/null || echo 0)"
done
