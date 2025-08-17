#include <sstream>
#include <string>
#include <utility>
#include <fstream>
#include <vector>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace std;

struct Problem {
    int nvars;
    int nclauses;
    vector<int> literals;
    vector<size_t> clause_starts;
    vector<int> literals_in_clauses;
    vector<size_t> literals_in_clauses_starts;
    vector<int> pos_count;
    vector<int> neg_count;
    Problem(int nvars, int nclauses) : nvars(nvars), nclauses(nclauses) {}
};

ostream& operator<<(ostream& os, const Problem& prob) {
    os << "Problem: " << prob.nvars << " vars, " << prob.nclauses << " clauses\n";
    for (int i = 0; i < prob.nclauses; ++i) {
        os << "Clause " << i+1 << ": ";
        size_t start = prob.clause_starts[i];
        size_t end = (i + 1 < prob.clause_starts.size()) ? prob.clause_starts[i + 1] : prob.literals.size();
        for (size_t j = start; j < end; ++j) {
            int lit = prob.literals[j];
            if (lit < 0) os << " ¬";
            else os << " ";
            os << "x" << abs(lit);
            if (j + 1 < end) os << " ∨";
        }
        os << '\n';
    }

    os << "\nVariable to clauses mapping:\n";
    for (int v = 1; v <= prob.nvars; ++v) {
        size_t start = prob.literals_in_clauses_starts[v-1];
        size_t end = prob.literals_in_clauses_starts[v];
        os << "x" << v << " in clauses:";
        for (size_t i = start; i < end; ++i) {
            os << " " << prob.literals_in_clauses[i]+1;
        }
        os << '\n';
    }
    return os;
}

pair<int,int> parse_dimacs_header(const string& line) {
    istringstream in(line);
    string ignore1, ignore2;
    int nvars, nclauses;
    in >> ignore1 >> ignore2 >> nvars >> nclauses;
    return {nvars, nclauses};
}

Problem parse(istream& in) {
    string line;
    getline(in, line);
    auto [nvars, nclauses] = parse_dimacs_header(line);
    Problem problem(nvars, nclauses);

    int var;
    bool new_clause = true;
    while (in >> var) {
        if (var == 0) {
            new_clause = true;
        } else {
            if (new_clause) {
                problem.clause_starts.push_back(problem.literals.size());
                new_clause = false;
            }
            problem.literals.push_back(var);
        }
    }
    problem.clause_starts.push_back(problem.literals.size());

    vector<vector<int>> temp_var_to_clauses(nvars + 1);
    for (int i = 0; i < nclauses; i++) {
        size_t start = problem.clause_starts[i];
        size_t end = problem.clause_starts[i + 1];
        for (size_t j = start; j < end; j++) {
            int v = abs(problem.literals[j]);
            temp_var_to_clauses[v].push_back(i);
        }
    }

    problem.literals_in_clauses_starts.push_back(0);
    for (int v = 1; v < nvars + 1; v++) {
        problem.literals_in_clauses.insert(
            problem.literals_in_clauses.end(),
            temp_var_to_clauses[v].begin(),
            temp_var_to_clauses[v].end()
        );
        problem.literals_in_clauses_starts.push_back(problem.literals_in_clauses.size());
    }
    vector<int> pos_count(nvars + 1, 0);
    vector<int> neg_count(nvars + 1, 0);

    for (int lit : problem.literals) {
        int var = abs(lit);
        if (lit > 0) pos_count[var]++;
        else neg_count[var]++;
    }
    problem.pos_count = std::move(pos_count);
    problem.neg_count = std::move(neg_count);

    cout << problem << endl;
    return problem;
}


void assign_literal(Problem& problem, vector<int8_t>& assignment, vector<int>& local_assignments, int var, int val) {
    assignment[var] = val;
    local_assignments.push_back(var);
    
    // Update the counts for the variable
    if (val == 1) {
        size_t start = problem.literals_in_clauses_starts[var - 1];
        size_t end = problem.literals_in_clauses_starts[var];
        for (size_t i = start; i < end; i++) {
            int clause_index = problem.literals_in_clauses[i];
            problem.neg_count[var]--;
        }
    } else {
        size_t start = problem.literals_in_clauses_starts[var - 1];
        size_t end = problem.literals_in_clauses_starts[var];
        for (size_t i = start; i < end; i++) {
            int clause_index = problem.literals_in_clauses[i];
            problem.pos_count[var]--;
        }
    }
}

void unassign_literal(Problem& problem, vector<int8_t>& assignment, int var) {
    int val = assignment[var];
    if (val == 0) return;

    assignment[var] = 0;

    if (val == 1) {
        size_t start = problem.literals_in_clauses_starts[var - 1];
        size_t end = problem.literals_in_clauses_starts[var];
        for (size_t i = start; i < end; i++) {
            int clause_index = problem.literals_in_clauses[i];
            problem.neg_count[var]++;
        }
    } else {
        size_t start = problem.literals_in_clauses_starts[var - 1];
        size_t end = problem.literals_in_clauses_starts[var];
        for (size_t i = start; i < end; i++) {
            int clause_index = problem.literals_in_clauses[i];
            problem.pos_count[var]++;
        }
    }
}

int is_unit_clause(Problem& problem, size_t clause_index, vector<int8_t>& assignment) {
    size_t start = problem.clause_starts[clause_index];
    size_t end = problem.clause_starts[clause_index + 1];

    int unit_literal = 0;
    for (size_t i = start; i < end; i++) {
        int lit = problem.literals[i];
        if (assignment[abs(lit)] == 0) {
            if (unit_literal != 0) {
                return 0; // More than one unassigned literal
            }
            unit_literal = lit;
        } else if (assignment[abs(lit)] == (lit < 0 ? -1 : 1)) {
            return 0; // Clause is satisfied
        }
    }
    return unit_literal; // Returns the unit literal if found, or 0 if none
}

bool propagate_units(Problem& problem, vector<int8_t>& assignment, vector<int>& local_assignments) {
    vector<int> unit_stack;
    unit_stack.reserve(problem.nvars);

    for (int i = 0; i < problem.nclauses; i++) {
        int lit = is_unit_clause(problem, i, assignment);
        if (lit != 0) unit_stack.push_back(lit);
    }

    while (!unit_stack.empty()) {
        int lit = unit_stack.back();
        unit_stack.pop_back();

        int var = abs(lit);
        int val = (lit > 0 ? 1 : -1);

        if (assignment[var] != 0) {
            if (assignment[var] != val) {
                return false; // conflict
            }
            continue;
        }

        assign_literal(problem, assignment, local_assignments, var, val);

        // scan clauses containing var (or -var) to find new units or conflicts
        size_t start = problem.literals_in_clauses_starts[var - 1];
        size_t end = problem.literals_in_clauses_starts[var];
        for (size_t i = start; i < end; i++) {
            int clause_index = problem.literals_in_clauses[i];
            int new_lit = is_unit_clause(problem, clause_index, assignment);
            if (new_lit != 0) {
                unit_stack.push_back(new_lit);
            }
        }
    }

    return true; // no conflicts
}

void assign_pure_literals(Problem& problem, vector<int8_t>& assignment, vector<int>& local_assignments) {
    for (int var = 1; var <= problem.nvars; var++) {
        if (assignment[var] != 0) continue; // skip already assigned

        if (problem.pos_count[var] > 0 && problem.neg_count[var] == 0) {
            assign_literal(problem, assignment, local_assignments, var, 1);
        } else if (problem.neg_count[var] > 0 && problem.pos_count[var] == 0) {
            assign_literal(problem, assignment, local_assignments, var, -1);
        }
    }
}

bool DPLL(Problem& problem, vector<int8_t>& assignment) {
    vector<int> local_assignments;
    if(!propagate_units(problem, assignment,local_assignments)) {
        for (int var : local_assignments) {
            unassign_literal(problem, assignment, var);
        }
        return false; // Conflict found during unit propagation
    }
    assign_pure_literals(problem, assignment, local_assignments);

    int remaining_lit = -1;

    for(int i = 1; i < problem.nvars + 1; i++){
        if (assignment[i] == 0) {
            remaining_lit = i;
            break;
        }
    }
    if (remaining_lit == -1) {
        return true; // All variables assigned, problem is satisfiable
    }

    // Check if any clause is unsatisfied
    for (int i = 0; i < problem.nclauses; i++) {
        size_t start = problem.clause_starts[i];
        size_t end = problem.clause_starts[i + 1];
        bool clause_satisfied = false;
        bool has_unassigned = false;

        for (size_t j = start; j < end; j++) {
            int lit = problem.literals[j];
            int val = assignment[abs(lit)];

            if ((lit > 0 && val == 1) || (lit < 0 && val == -1)) {
                clause_satisfied = true;
                break;
            }
            if (val == 0) {
                has_unassigned = true;
            }
        }

        if (!clause_satisfied && !has_unassigned) {
            for (int var : local_assignments) {
                unassign_literal(problem, assignment, var);
            }
            return false; // all literals false → clause unsatisfied
        }
    }

    assign_literal(problem, assignment, local_assignments, remaining_lit, 1);
    if (DPLL(problem, assignment)) {
        return true; // Satisfiable with current assignment
    }
    unassign_literal(problem, assignment, remaining_lit);
    local_assignments.pop_back();

    assign_literal(problem, assignment, local_assignments, remaining_lit, -1);
    if (DPLL(problem, assignment)) {
        return true; // Satisfiable with negated assignment
    }
    unassign_literal(problem, assignment, remaining_lit);
    local_assignments.pop_back();

    for (int var : local_assignments) {
        unassign_literal(problem, assignment, var);
    }
    return false;
}


atomic<bool> finished(false);

void watchdog(int seconds) {
    this_thread::sleep_for(chrono::seconds(seconds));
    if (!finished.load()) {
        cerr << "Timeout reached! Terminating solver.\n";
        exit(1);
    }
}

int main() {
    Problem sat_problem = parse(cin);

    thread(watchdog, 10).detach();

    vector<int8_t> assignment(sat_problem.nvars + 1, 0);
    if (DPLL(sat_problem, assignment)) {
        for (int i = 1; i < sat_problem.nvars+1; i++) {
            cout << "x" << i << " = " << (assignment[i] > 0 ? "true" : "false") << '\n';
        }
        cout << "SATISFIABLE\n";
    } else {
        cout << "UNSATISFIABLE\n";
    }

    cout.flush();
    return 0;
}
