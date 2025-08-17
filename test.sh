#!/bin/bash
TEST_DIR="./test"
RESULTS_DIR="./results"
mkdir -p "$RESULTS_DIR"
./run.sh
echo "File,MySAT_Time,MiniSAT_Time,Match" > "$RESULTS_DIR/summary.csv"

for file in "$TEST_DIR"/*.cnf; do
    fname=$(basename "$file")
    echo "Processing $fname..."


    mytime=$( { /usr/bin/time -f "%e" ./sat < "$file" > "$RESULTS_DIR/${fname}.mysat"; } 2>&1 | tail -n1 )
    mintime=$( { /usr/bin/time -f "%e" minisat "$file" > "$RESULTS_DIR/${fname}.minisat"; } 2>&1 | tail -n1 )


    myresult=$(grep -E '^(SAT|UNSAT)' "$RESULTS_DIR/${fname}.mysat" | head -n1)
    minresult=$(grep -E '^(SAT|UNSAT)' "$RESULTS_DIR/${fname}.minisat" | head -n1)
    myresult=$(grep -E '^(SAT|UNSAT)' "$RESULTS_DIR/${fname}.mysat" | head -n1)
    minresult=$(grep -E '^(SAT|UNSAT)' "$RESULTS_DIR/${fname}.minisat" | head -n1)

    # Compare results
    if [ "$myresult" = "$minresult" ]; then
        match_str="YES"
    else
        match_str="NO"
    fi

    echo "$fname,$mytime,$mintime,$match_str"
    echo "$fname,$mytime,$mintime,$match_str" >> "$RESULTS_DIR/summary.csv"
done

echo "All done. Results saved in $RESULTS_DIR/summary.csv"
