#!/usr/bin/env bash
# M3.3 -- octasquare (4.8.8) grid sweep.
#
# Same structure as run_bevelhex_sweep.sh but octasquare's `gen` takes
# `-sizes <squares>,<octagons>` so the sweep is two-dimensional.
# Defaults to all (s, o) with s, o >= 1 and s + o in 2..6, which
# stays under a few thousand shapes per pair on this hardware.
# Override by passing pairs as arguments, e.g.:
#
#   bash run_octasquare_sweep.sh 4,4 5,3 3,5

set -euo pipefail
cd "$(dirname "$0")/../.."   # repo root
ROOT="$PWD"
GEN="$ROOT/src/sat/src/gen"
SAT="$ROOT/src/sat/src/sat"
RESULTS="$ROOT/benchmarks/lateral/results"
mkdir -p "$RESULTS"

[[ -x "$GEN" && -x "$SAT" ]] || ( cd "$ROOT/src/sat/src" && make )

if [[ $# -gt 0 ]]; then
	PAIRS=( "$@" )
else
	PAIRS=()
	for total in 2 3 4 5 6; do
		for s in $(seq 1 $((total - 1))); do
			o=$((total - s))
			PAIRS+=( "$s,$o" )
		done
	done
fi

CSV="$RESULTS/m3.3-octasquare-sweep.csv"
LOG="$RESULTS/m3.3-octasquare-sweep.log"
{
	echo "# heesch-forge M3.3 octasquare sweep"
	echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
	echo "# host: $(uname -srm)"
	echo
} >"$LOG"

echo "squares,octagons,total,shapes,wall_s,hc0,hc1,hc2,hc3plus,isohedral,inconclusive" >"$CSV"

for pair in "${PAIRS[@]}"; do
	s=${pair%,*}
	o=${pair#*,}
	total=$((s + o))
	in_file="$RESULTS/oct_${s}_${o}.in"
	out_file="$RESULTS/oct_${s}_${o}.out"

	"$GEN" -octasquare -sizes "$s,$o" -free >"$in_file"
	count=$(wc -l <"$in_file")

	if [[ "$count" -eq 0 ]]; then
		echo "  $s squares + $o octagons -> 0 shapes (skipped)"
		echo "$s,$o,$total,0,0,0,0,0,0,0,0" >>"$CSV"
		continue
	fi

	t0=$(date +%s.%N)
	"$SAT" -isohedral -hh "$in_file" -o "$out_file" >>"$LOG" 2>&1
	t1=$(date +%s.%N)
	wall=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.3f", b-a}')

	hc0=$(awk '/^~/ && $2=="0"' "$out_file" | wc -l)
	hc1=$(awk '/^~/ && $2=="1"' "$out_file" | wc -l)
	hc2=$(awk '/^~/ && $2=="2"' "$out_file" | wc -l)
	hc3=$(awk '/^~/ && ($2 >= 3)' "$out_file" | wc -l)
	iso=$(grep -c '^I' "$out_file" || true)
	inc=$(grep -c '^!' "$out_file" || true)

	echo "$s,$o,$total,$count,$wall,$hc0,$hc1,$hc2,$hc3,$iso,$inc" >>"$CSV"
	printf "  %d squares + %d octagons (total %d): shapes=%5d wall=%6ss  Hc=0:%5d  Hc=1:%4d  Hc=2:%3d  Hc>=3:%3d  iso:%4d  inc:%3d\n" \
		"$s" "$o" "$total" "$count" "$wall" \
		"$hc0" "$hc1" "$hc2" "$hc3" "$iso" "$inc"
done

echo
echo "# CSV: $CSV"
echo "# log: $LOG"
