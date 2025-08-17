#!/bin/bash
TEST_DIR="./test"
RESULTS_DIR="./results"
mkdir -p "$RESULTS_DIR"
./run.sh
echo "File,MySAT_Time,MiniSAT_Time,Match" >"$RESULTS_DIR/summary.csv"

process_file() {
    local file="$1"
    local fname
    fname=$(basename "$file")

    echo "Processing $fname..."

    local mytime mintime myresult minresult match_str

    mytime=$({ /usr/bin/time -f "%e" ./sat <"$file" >"$RESULTS_DIR/${fname}.mysat"; } 2>&1 | tail -n1)
    mintime=$({ /usr/bin/time -f "%e" minisat "$file" >"$RESULTS_DIR/${fname}.minisat"; } 2>&1 | tail -n1)

    myresult=$(grep -E '^(SAT|UNSAT)' "$RESULTS_DIR/${fname}.mysat" | head -n1)
    minresult=$(grep -E '^(SAT|UNSAT)' "$RESULTS_DIR/${fname}.minisat" | head -n1)

    if [ "$myresult" = "$minresult" ]; then
        match_str="YES"
    else
        match_str="NO"
    fi

    echo "$fname,$mytime,$mintime,$match_str"
    echo "$fname,$mytime,$mintime,$match_str" >>"$RESULTS_DIR/summary.csv"
}

export -f process_file
export RESULTS_DIR

# Run in parallel (4 jobs at a time)
find "$TEST_DIR" -name '*.cnf' | xargs -n1 -P4 -I{} bash -c 'process_file "$@"' _ {}

echo "All done. Results saved in $RESULTS_DIR/summary.csv"
