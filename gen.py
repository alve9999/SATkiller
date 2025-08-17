import random
import os
import subprocess
import time

output_dir = "test"
os.makedirs(output_dir, exist_ok=True)

n_files = 100
n_vars = 100         # smaller for generation speed
n_clauses = int(4.25 * n_vars)  # ratio ~4.2
k_min = 3
k_max = 3

def generate_file(filename):
    with open(filename, "w") as f:
        f.write(f"p cnf {n_vars} {n_clauses}\n")
        for _ in range(n_clauses):
            clause = []
            k = random.randint(k_min, k_max)
            while len(clause) < k:
                var = random.randint(1, n_vars)
                var = var if random.choice([True, False]) else -var
                if var not in clause and -var not in clause:
                    clause.append(var)
            f.write(" ".join(map(str, clause)) + " 0\n")

def runtime_ok(filename):
    try:
        start = time.time()
        subprocess.run(
            ["./sat"],
            stdin=open(filename, "r"),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=10  # hard cutoff
        )
        elapsed = time.time() - start
        print(f"Runtime for {filename}: {elapsed:.2f} seconds")
        return 0.5 <= elapsed <= 9
    except subprocess.TimeoutExpired:
        print(f"Timeout expired for {filename}")
        return False

for i in range(1, n_files + 1):
    filename = os.path.join(output_dir, f"random_{i}.cnf")
    tries = 0
    for i in range(1, n_files + 1):
        filename = os.path.join(output_dir, f"random_{i}.cnf")
        tries = 0
        while True:
            tries += 1
            generate_file(filename)
            if runtime_ok(filename):
                print(f"Generated {filename} (tries={tries})")
                break


