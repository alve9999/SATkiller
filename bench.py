#!/usr/bin/env python3
import subprocess
import time
import csv
import subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

TEST_DIR = Path("./SATfamlies")
RESULTS_DIR = Path("./results")
RESULTS_DIR.mkdir(exist_ok=True)
CSV_FILE = RESULTS_DIR / "summary.csv"

# Collect all cnf files recursively
cnf_files = list(TEST_DIR.rglob("*.cnf"))

# Stats counters
stats = {}


def run_solver(cmd, infile, outfile, use_stdin=False, timeout_sec=20):
    start = time.time()
    print(infile, "->", outfile)
    try:
        with open(outfile, "w") as fout:
            if use_stdin:
                with open(infile, "r") as fin:
                    subprocess.run(
                        cmd, stdin=fin, stdout=fout, stderr=subprocess.DEVNULL,
                        check=True, text=True, timeout=timeout_sec
                    )
            else:
                subprocess.run(
                    cmd + [str(infile)], stdout=fout, stderr=subprocess.DEVNULL,
                    text=True, timeout=timeout_sec
                )
        elapsed = time.time() - start
    except subprocess.CalledProcessError as e:
        print(f"Error running {cmd}: exit code {e.returncode}")
        print(f"stderr: {e.stderr}")
        elapsed = timeout_sec
        with open(outfile, "w") as fout:
            fout.write("TIMEOUT\n")
    except subprocess.TimeoutExpired:
        elapsed = timeout_sec
        with open(outfile, "w") as fout:
            fout.write("TIMEOUT\n")
    return elapsed


def process_file(file_path):
    fname = file_path.name
    category = file_path.parent.name
    stats.setdefault(category, {"total": 0, "match": 0})
    stats[category]["total"] += 1

    my_out = RESULTS_DIR / f"{fname}.mysat"
    mini_out = RESULTS_DIR / f"{fname}.minisat"

    my_time = run_solver(["./sat"], file_path, my_out,True)
    mini_time = run_solver(["minisat"], file_path, mini_out)

    def get_result(f):
        for line in f.read_text().splitlines():
            if line.startswith(("SAT", "UNSAT")):
                return line
        return "UNKNOWN"

    my_result = get_result(my_out)
    mini_result = get_result(mini_out)

    match = "YES" if my_result == mini_result else "NO"
    if match == "YES":
        stats[category]["match"] += 1

    return [str(fname), f"{my_time:.3f}", f"{mini_time:.3f}", match, category]

# Run in parallel
with ThreadPoolExecutor(max_workers=8) as executor:
    results = list(executor.map(process_file, cnf_files))

# Write CSV
with CSV_FILE.open("w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["File", "MySAT_Time", "MiniSAT_Time", "Match", "Category"])
    writer.writerows(results)

# Print summary
for cat, val in stats.items():
    print(f"{cat}: {val['match']} / {val['total']} matched")

