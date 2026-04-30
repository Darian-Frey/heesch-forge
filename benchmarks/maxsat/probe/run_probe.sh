#!/usr/bin/env bash
# M1.5 partial-corona MaxSAT probe.
#
# Builds sat-wcnf-dump if needed, runs it on a configurable set of shapes,
# then walks the dumped .wcnf files with RC2 (via .venv) and writes a CSV
# of per-instance MaxSAT outcomes. The CSV is what M1.5's writeup
# references; the negative architectural finding ("the existing encoding
# does not admit a meaningful partial-corona MaxSAT score without
# restructuring") falls out of those rows.
#
# Default: probes the first Hc=1 heptomino + the first Hc=1 octomino from
# the Kaplan dataset; pass shape coordinates as arguments to override.

set -euo pipefail
cd "$(dirname "$0")/../../.."   # repo root
ROOT="$PWD"
SAT="$ROOT/src/sat/src/sat-wcnf-dump"
PYTHON="$ROOT/.venv/bin/python"
RESULTS="$ROOT/benchmarks/maxsat/results"
mkdir -p "$RESULTS"

[[ -x "$SAT" ]] || ( cd "$ROOT/src/sat/src" && make sat-wcnf-dump )
[[ -x "$PYTHON" ]] || { echo "missing $PYTHON; run: python3 -m venv .venv && .venv/bin/pip install python-sat[aiger]"; exit 2; }

declare -a SHAPES NAMES
if [[ $# -gt 0 ]]; then
	for s in "$@"; do
		SHAPES+=( "$s" )
		NAMES+=( "custom-$(echo "$s" | tr ' ' '_' | head -c 32)" )
	done
else
	# First Hc=1 heptomino (line 1) and Hc=1 octomino (first H>=1 entry).
	SHAPES+=( "$(sed -n '1p' data/kaplan-2022/omino/07omino_0up.txt)" )
	NAMES+=( "n07-omino-1" )
	SHAPES+=( "$(sed -n '1p' data/kaplan-2022/omino/08omino_0up.txt)" )
	NAMES+=( "n08-omino-1" )
fi

LOG="$RESULTS/m1.5-probe.log"
CSV="$RESULTS/m1.5-probe.csv"
{
	echo "# heesch-forge M1.5 MaxSAT partial-corona probe"
	echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
	echo "# host: $(uname -srm)"
	echo
} >"$LOG"
: >"$CSV"

# Aggregate CSV header on first run.
HEADER=("shape" "instance" "nvars" "nhard" "nsoft" "cost" "score" "wall_s" "timeout" "unsat_hard")

PRINT_HEADER=1
for i in "${!SHAPES[@]}"; do
	shape="${SHAPES[$i]}"
	name="${NAMES[$i]}"
	echo "==== $name : $shape ====" | tee -a "$LOG"

	dump_dir=$(mktemp -d -t maxsat-probe-XXXXXX)
	in_file="$dump_dir/in.txt"
	out_file="$dump_dir/out.txt"
	csv_part="$dump_dir/scores.csv"
	printf 'O? %s\n' "$shape" >"$in_file"
	HEESCH_DUMP_DIR="$dump_dir" "$SAT" -isohedral -hh "$in_file" -o "$out_file"

	if ! ls "$dump_dir"/*.wcnf >/dev/null 2>&1; then
		echo "  (no UNSAT solves; nothing dumped)" | tee -a "$LOG"
		continue
	fi

	"$PYTHON" benchmarks/maxsat/run_rc2.py \
		--dump-dir "$dump_dir" \
		--out "$csv_part" \
		--cap 60 2>&1 | tee -a "$LOG"

	# Prepend the shape name to each row and append into the aggregate CSV.
	if [[ $PRINT_HEADER -eq 1 ]]; then
		(IFS=','; echo "${HEADER[*]}") >"$CSV"
		PRINT_HEADER=0
	fi
	tail -n +2 "$csv_part" | sed "s|^|$name,|" >>"$CSV"

	rm -rf "$dump_dir"
done

echo
echo "== probe complete =="
echo "log: $LOG"
echo "csv: $CSV"
