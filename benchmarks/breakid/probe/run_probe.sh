#!/usr/bin/env bash
# M1.3 BreakID empirical probe.
#
# 1) Builds sat-dump (CMSat backend that tees every SAT instance to DIMACS).
# 2) Runs sat-dump on a small set of polyominoes from data/kaplan-2022/omino,
#    capturing one CNF per HeeschSolver SAT call into a fresh dump dir.
# 3) Runs `breakid --verb 0` on each dumped CNF (with a per-call wall-time
#    cap), recording the symmetry-clause count and BreakID's own runtime.
# 4) For the small "fresh" CNFs (where BreakID succeeds), runs a quick A/B
#    via the bundled cms_solve helper to check whether the breaking clauses
#    actually change CMSat's solve behaviour.
#
# Invocation:
#   bash benchmarks/breakid/probe/run_probe.sh [shape-spec ...]
#
# A shape-spec is a newline of cell coordinates, e.g.
#   "1 0 1 1 0 2 1 2 1 3 2 3 3 3"
# If no shape-specs are passed, the script uses the first heptomino from
# data/kaplan-2022/omino/07omino_0up.txt and the first octomino from
# 08omino_0up.txt -- the same set that produced the published M1.3 numbers.

set -euo pipefail
cd "$(dirname "$0")/../../.."   # repo root
ROOT="$PWD"
SAT_DUMP="$ROOT/src/sat/src/sat-dump"
RESULTS_DIR="$ROOT/benchmarks/breakid/results"
mkdir -p "$RESULTS_DIR"

# 1) Build sat-dump if missing.
if [[ ! -x "$SAT_DUMP" ]]; then
	( cd "$ROOT/src/sat/src" && make sat-dump )
fi

# 2) Build the cms_solve helper (a thin DIMACS-driver around CMSat that
#    echoes wall_ms / conflicts / decisions per file). Compiled out of
#    line so the probe script stays self-contained.
HELPER_SRC="$ROOT/benchmarks/breakid/probe/cms_solve.cpp"
HELPER_BIN="$ROOT/benchmarks/breakid/probe/cms_solve"
if [[ ! -x "$HELPER_BIN" || "$HELPER_BIN" -ot "$HELPER_SRC" ]]; then
	g++ -O3 -std=c++20 \
		-I/usr/local/include/cryptominisat5 \
		"$HELPER_SRC" \
		-L/usr/local/lib -Wl,-rpath,/usr/local/lib -lcryptominisat5 \
		-o "$HELPER_BIN"
fi

# 3) Decide the shapes to probe.
declare -a SHAPES NAMES
if [[ $# -gt 0 ]]; then
	for s in "$@"; do
		SHAPES+=( "$s" )
		NAMES+=( "custom-$(echo "$s" | tr ' ' '_' | head -c 32)" )
	done
else
	SHAPES+=( "$(head -1 data/kaplan-2022/omino/07omino_0up.txt)" )
	NAMES+=( "n07-omino-1" )
	SHAPES+=( "$(head -1 data/kaplan-2022/omino/08omino_0up.txt)" )
	NAMES+=( "n08-omino-1" )
fi

LOG="$RESULTS_DIR/m1.3-probe.log"
SUM="$RESULTS_DIR/m1.3-probe.csv"
{
	echo "# heesch-forge M1.3 BreakID probe"
	echo "# date:    $(date -u +%Y-%m-%dT%H:%M:%SZ)"
	echo "# breakid: $(breakid --version 2>&1 | head -1)"
	echo "# host:    $(uname -srm)"
	echo
} >"$LOG"

echo "shape,instance,vars,clauses_in,breakid_wall_s,breaking_clauses,raw_solve_ms,broken_solve_ms,raw_conflicts,broken_conflicts,raw_decisions,broken_decisions" >"$SUM"

for i in "${!SHAPES[@]}"; do
	shape="${SHAPES[$i]}"
	name="${NAMES[$i]}"
	echo "==== $name : $shape ====" | tee -a "$LOG"

	dump_dir=$(mktemp -d -t breakid-probe-XXXXXX)
	in_file="$dump_dir/in.txt"
	out_file="$dump_dir/out.txt"
	printf 'O? %s\n' "$shape" >"$in_file"
	HEESCH_DUMP_DIR="$dump_dir" "$SAT_DUMP" -isohedral -hh "$in_file" -o "$out_file"

	for cnf in "$dump_dir"/inst-*.cnf; do
		inst=$(basename "$cnf" .cnf)
		hdr=$(grep -m1 '^p cnf' "$cnf")
		nvars=$(echo "$hdr" | awk '{print $3}')
		nclauses=$(echo "$hdr" | awk '{print $4}')
		broken="$dump_dir/$inst.broken.cnf"
		echo "-- $inst (vars=$nvars clauses=$nclauses)" | tee -a "$LOG"

		# BreakID with 60-second wall cap. If it times out, we record that
		# fact and skip the A/B (matches the M1.3 finding that walkback
		# formulas time out without symmetries).
		t0=$(date +%s.%N)
		if timeout 60 breakid --verb 0 "$cnf" "$broken" >>"$LOG" 2>&1; then
			t1=$(date +%s.%N)
			breakid_wall=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')
			added=$(( $(grep -c '^[-0-9]' "$broken") - $(grep -c '^[-0-9]' "$cnf") ))
		else
			t1=$(date +%s.%N)
			breakid_wall=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')
			added="-1"  # sentinel: timeout
			broken=""
		fi
		echo "   breakid_wall=${breakid_wall}s breaking_clauses=$added" | tee -a "$LOG"

		# A/B solve (CMSat). Skip if BreakID timed out.
		raw=$( "$HELPER_BIN" "$cnf" )
		raw_ms=$(echo "$raw" | grep -oE 'wall_ms=[0-9.]+'   | cut -d= -f2)
		raw_co=$(echo "$raw" | grep -oE 'conflicts=[0-9]+'  | cut -d= -f2)
		raw_dc=$(echo "$raw" | grep -oE 'decisions=[0-9]+'  | cut -d= -f2)
		if [[ -n "$broken" && -f "$broken" ]]; then
			brk=$( "$HELPER_BIN" "$broken" )
			brk_ms=$(echo "$brk" | grep -oE 'wall_ms=[0-9.]+'   | cut -d= -f2)
			brk_co=$(echo "$brk" | grep -oE 'conflicts=[0-9]+'  | cut -d= -f2)
			brk_dc=$(echo "$brk" | grep -oE 'decisions=[0-9]+'  | cut -d= -f2)
		else
			brk_ms="" ; brk_co="" ; brk_dc=""
		fi
		echo "   raw:    wall_ms=$raw_ms conflicts=$raw_co decisions=$raw_dc" | tee -a "$LOG"
		echo "   broken: wall_ms=$brk_ms conflicts=$brk_co decisions=$brk_dc" | tee -a "$LOG"
		echo "$name,$inst,$nvars,$nclauses,$breakid_wall,$added,$raw_ms,$brk_ms,$raw_co,$brk_co,$raw_dc,$brk_dc" >>"$SUM"
	done
	rm -rf "$dump_dir"
done

echo
echo "== Probe complete =="
echo "Log:      $LOG"
echo "Summary:  $SUM"
