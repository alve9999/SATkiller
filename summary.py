import math
from collections import defaultdict

TIMEOUT = 19.9

mysat_par2_sum = 0.0
minisat_par2_sum = 0.0
mysat_solved = 0
minisat_solved = 0
yes_matches = 0
no_not_timeout = 0
total = 0

# Per-category accumulators
cats = defaultdict(lambda: {
    "total": 0,
    "mysat_par2_sum": 0.0,
    "minisat_par2_sum": 0.0,
    "mysat_solved": 0,
    "minisat_solved": 0,
    "yes_matches": 0,
    "no_not_timeout": 0,
})

with open("results/summary.csv") as f:
    for line in f:
        if line.startswith("File") or not line.strip():
            continue
        file, myt_str, mint_str, match, cat = line.strip().split(",")
        myt, mint = float(myt_str), float(mint_str)
        total += 1
        cats[cat]["total"] += 1

        # Solved status
        my_timeout = myt >= TIMEOUT
        mini_timeout = mint >= TIMEOUT
        if not my_timeout:
            mysat_solved += 1
            cats[cat]["mysat_solved"] += 1
        if not mini_timeout:
            minisat_solved += 1
            cats[cat]["minisat_solved"] += 1

        # PAR-2 contribution
        mysat_par2_sum += myt if not my_timeout else 2 * TIMEOUT
        minisat_par2_sum += mint if not mini_timeout else 2 * TIMEOUT
        cats[cat]["mysat_par2_sum"] += myt if not my_timeout else 2 * TIMEOUT
        cats[cat]["minisat_par2_sum"] += mint if not mini_timeout else 2 * TIMEOUT

        # YES matches
        if match == "YES" and not my_timeout and not mini_timeout:
            yes_matches += 1
            cats[cat]["yes_matches"] += 1

        # NO not due to timeout
        if match == "NO" and not my_timeout and not mini_timeout:
            no_not_timeout += 1
            cats[cat]["no_not_timeout"] += 1

mysat_par2 = mysat_par2_sum / total
minisat_par2 = minisat_par2_sum / total

print(f"Total instances: {total}")
print(f"MySAT solved: {mysat_solved}, MiniSAT solved: {minisat_solved}")
print(f"YES matches (no timeout): {yes_matches}")
print(f"NO cases not due to timeout: {no_not_timeout}")
print(f"MySAT PAR-2: {mysat_par2:.3f}, MiniSAT PAR-2: {minisat_par2:.3f}")

# Weighted overall (log weighting)
total_my_weighted = 0.0
total_mini_weighted = 0.0
total_weight = 0.0

print("\nPer-category results (log-weighted):")
for cat, val in cats.items():
    if val["total"] == 0:
        continue
    weight = math.log1p(val["total"])
    my_par2 = val["mysat_par2_sum"] / val["total"]
    mini_par2 = val["minisat_par2_sum"] / val["total"]
    total_my_weighted += my_par2 * weight
    total_mini_weighted += mini_par2 * weight
    total_weight += weight

    print(f"{cat:<15} n={val['total']:<4} "
          f"MyPAR2={my_par2:.3f} MiniPAR2={mini_par2:.3f} "
          f"YES={val['yes_matches']} NO_noTO={val['no_not_timeout']}")

overall_my = total_my_weighted / total_weight
overall_mini = total_mini_weighted / total_weight
print(f"\nOverall weighted (log by category size): "
      f"MySAT={overall_my:.3f}, MiniSAT={overall_mini:.3f}")

