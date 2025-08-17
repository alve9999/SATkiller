import random
import os

output_dir = "test"
os.makedirs(output_dir, exist_ok=True)

n_files = 200
n_vars = 50       # number of variables per file
n_clauses = 200   # number of clauses per file
k_min = 2         # min literals per clause
k_max = 5         # max literals per clause

for i in range(1, n_files + 1):
    filename = os.path.join(output_dir, f"random_{i}.cnf")
    with open(filename, "w") as f:
        f.write(f"p cnf {n_vars} {n_clauses}\n")
        for _ in range(n_clauses):
            k = random.randint(k_min, k_max)
            clause = []
            while len(clause) < k:
                var = random.randint(1, n_vars)
                if random.choice([True, False]):
                    var = -var
                if var not in clause and -var not in clause:
                    clause.append(var)
            f.write(" ".join(map(str, clause)) + " 0\n")

print(f"Generated {n_files} random CNF files in '{output_dir}/'")
