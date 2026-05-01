#!/usr/bin/env bash
# M3.1 -- bevelhex grid verification sweep.
#
# Generates free bevelhex polyforms at each requested size via
# upstream `gen -bevelhex`, classifies them with `sat -isohedral
# -hh`, and reports the per-size Hc distribution + isohedral and
# inconclusive counts. Output is a small CSV that captures the
# Heesch-number result set for sizes that fit on a single
# workstation.
#
# Defaults sweep sizes 4..7 (49..7,796 shapes per size, ~3.7 s
# total wall on a ThinkPad P15 Gen 2i). Pass sizes as arguments
# to override.

set -euo pipefail
cd "$(dirname "$0")/../.."   # repo root
ROOT="$PWD"
GEN="$ROOT/src/sat/src/gen"
SAT="$ROOT/src/sat/src/sat"
RESULTS="$ROOT/benchmarks/lateral/results"
mkdir -p "$RESULTS"

[[ -x "$GEN" && -x "$SAT" ]] || ( cd "$ROOT/src/sat/src" && make )

if [[ $# -gt 0 ]]; then
	SIZES=( "$@" )
else
	SIZES=( 4 5 6 7 )
fi

CSV="$RESULTS/m3.1-bevelhex-sweep.csv"
LOG="$RESULTS/m3.1-bevelhex-sweep.log"
{
	echo "# heesch-forge M3.1 bevelhex sweep"
	echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
	echo "# host: $(uname -srm)"
	echo
} >"$LOG"

echo "size,shapes,wall_s,hc0,hc1,hc2,hc3plus,isohedral,inconclusive" >"$CSV"

for n in "${SIZES[@]}"; do
	in_file="$RESULTS/bvh_n${n}.in"
	out_file="$RESULTS/bvh_n${n}.out"
	"$GEN" -bevelhex -size "$n" -free >"$in_file"
	count=$(wc -l <"$in_file")

	t0=$(date +%s.%N)
	"$SAT" -isohedral -hh "$in_file" -o "$out_file" >>"$LOG" 2>&1
	t1=$(date +%s.%N)
	wall=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.3f", b-a}')

	# Tally classifications. The classification line for non-tilers
	# starts with "~", isohedral with "I", inconclusive with "!".
	hc0=$(awk '/^~/ && $2=="0"' "$out_file" | wc -l)
	hc1=$(awk '/^~/ && $2=="1"' "$out_file" | wc -l)
	hc2=$(awk '/^~/ && $2=="2"' "$out_file" | wc -l)
	hc3=$(awk '/^~/ && ($2 >= 3)' "$out_file" | wc -l)
	iso=$(grep -c '^I' "$out_file" || true)
	inc=$(grep -c '^!' "$out_file" || true)

	echo "$n,$count,$wall,$hc0,$hc1,$hc2,$hc3,$iso,$inc" >>"$CSV"
	printf "  size=%d  shapes=%5d  wall=%6ss  Hc=0:%5d  Hc=1:%4d  Hc=2:%3d  Hc>=3:%3d  iso:%4d  inc:%3d\n" \
		"$n" "$count" "$wall" "$hc0" "$hc1" "$hc2" "$hc3" "$iso" "$inc"
done

echo
echo "# CSV: $CSV"
echo "# log: $LOG"
