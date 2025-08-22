#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <bits/stdc++.h>
#include <random>

using namespace std;

struct Stats {
    int propagations = 0;
    int conflicts = 0;
    int decisions = 0;
    int learned_clauses = 0;
};

Stats stats;

struct Clause{
    bool satisfied;
    bool learned;
    vector<int> literals;
    int watch1 = -1;
    int watch2 = -1;
};

struct Problem {
    int nvars;
    int nclauses;
    vector<int> literals;
    vector<Clause> clauses;
    vector<vector<int>> literal_to_clauses;
    vector<int> pos_count;
    vector<int> neg_count;
    vector<double> activity;
    vector<int8_t> saved_phase;
    vector<vector<int>> watches;
    Problem(int nvars, int nclauses) : nvars(nvars), nclauses(nclauses), activity(nvars+1, 0.0) {}
};

struct Trail {
    int var;
};

struct Assignment {
    bool decision;
    int clause;
    int8_t val;
    int dl;
};

struct LubySequence {
    uint64_t u = 1;
    uint64_t v = 1;

    uint64_t advance() {
        uint64_t result = v;
        if ((u & -u) == v) {
            u += 1;
            v = 1;
        } else {
            v <<= 1;
        }
        return result;
    }
};

bool random_bool(double p) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::bernoulli_distribution dist(p);
    return dist(rng);
}

ostream &operator<<(ostream &os, const Problem &prob) {
    os << "Problem: " << prob.nvars << " vars, " << prob.nclauses
       << " clauses\n";
    for (int i = 0; i < prob.nclauses; i++) {
        os << "Clause " << i + 1 << ": ";
        const Clause &c = prob.clauses[i];
        for (size_t j = 0; j < c.literals.size(); j++) {
            int lit = c.literals[j];
            if (lit < 0)
                os << " ¬";
            else
                os << " ";
            os << "x" << abs(lit);
        }
        os << '\n';
    }

    os << "\nLiteral to clauses mapping:\n";
    for (int v = 1; v < prob.nvars + 1; v++) {
        os << "x" << v << " in clauses:";
        for (size_t i = 0; i < prob.literal_to_clauses[v].size(); i++) {
            os << " " << prob.literal_to_clauses[v][i] + 1;
        }
        os << '\n';
    }
    return os;
}

pair<int, int> parse_dimacs_header(const string &line) {
    istringstream in(line);
    string ignore1, ignore2;
    int nvars, nclauses;
    in >> ignore1 >> ignore2 >> nvars >> nclauses;
    return {nvars, nclauses};
}

//indexing for negative literals is nvars to 2 * nvars - 1, so we need to convert
inline int lit_to_watch_idx(int lit, int nvars) {
    return lit > 0 ? lit : nvars - lit;
}

void init_watches(Problem &problem) {
    problem.watches.assign(2 * problem.nvars, vector<int>());
    
    for (int i = 0; i < problem.clauses.size(); i++) {
        Clause &c = problem.clauses[i];
        
        if (c.literals.size() == 1) {
            c.watch1 = c.literals[0];
            c.watch2 = 0;
            int watch_idx = lit_to_watch_idx(c.watch1, problem.nvars) - 1;
            problem.watches[watch_idx].push_back(i);
        } else {
            c.watch1 = c.literals[0];
            c.watch2 = c.literals[1];
            int watch1_idx = lit_to_watch_idx(c.watch1, problem.nvars) - 1;
            int watch2_idx = lit_to_watch_idx(c.watch2, problem.nvars) - 1;
            problem.watches[watch1_idx].push_back(i);
            problem.watches[watch2_idx].push_back(i);
        }
    }
}

Problem parse(istream &in) {
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
                Clause c;
                c.satisfied = false;
                c.learned = false;
                c.literals = vector<int>(0);
                problem.clauses.push_back(c);
                new_clause = false;
            }
            problem.clauses.back().literals.push_back(var);
        }
    }

    problem.literal_to_clauses.resize(nvars + 1);
    for (int i = 0; i < nclauses; i++) {
        Clause &c = problem.clauses[i];
        for (size_t j = 0; j < c.literals.size(); j++) {
            int v = abs(c.literals[j]);
            problem.literal_to_clauses[v].push_back(i);
        }
    }

    vector<int> pos_count(nvars + 1, 0);
    vector<int> neg_count(nvars + 1, 0);

    for (const Clause &clause : problem.clauses) {
        for (int lit : clause.literals) {
            int var = abs(lit);
            if (lit > 0){
                pos_count[var]++;
            }
            else{
                neg_count[var]++;
            }
            problem.activity[var] += 1.0;
        }
    }

    problem.pos_count = std::move(pos_count);
    problem.neg_count = std::move(neg_count);

    for (int lit : problem.literals) {

    }

    problem.saved_phase.resize(nvars + 1, 0);
    init_watches(problem);


    cout << problem << endl;
    return problem;
}


double decay_factor = 0.95;
double increment = 1.0;
const double activity_threshold = 1e100;

void bump_activity(Problem &problem,int var) {
    problem.activity[var] += increment;

    if (problem.activity[var] > activity_threshold) {
        for (int i = 1; i <= problem.nvars; i++) {
            problem.activity[i] *= 1e-100;
        }
        increment *= 1e-100;
    }
}

void decay_activities(Problem &problem) {
    increment /= decay_factor;
}

int select_vsids(Problem &problem, vector<Assignment> &assignment) {
    int best_var = -1;
    double best_score = -1.0;
    for (int v = 1; v < problem.nvars + 1; v++) {
        if (assignment[v].val == 0 && problem.activity[v] > best_score) {
            best_score = problem.activity[v];
            best_var = v;
        }
    }
    return best_var;
}

bool is_lit_satisfied(int lit, const vector<Assignment> &assignment) {
    int8_t val = assignment[abs(lit)].val;
    return val != 0 && (lit ^ val) > 0;
}

bool is_lit_falsified(int lit, const vector<Assignment> &assignment) {
    int8_t val = assignment[abs(lit)].val;
    return val != 0 && (lit ^ val) < 0;
}

bool update_watches(Problem &problem, int clause_idx, int false_lit, const vector<Assignment> &assignment) {
    Clause &clause = problem.clauses[clause_idx];

    int other_watch = (clause.watch1 == false_lit) ? clause.watch2 : clause.watch1;

    for (int lit : clause.literals) {
        bool is_lit_false = is_lit_falsified(lit, assignment);
        if (lit != false_lit && lit != other_watch && !is_lit_false) {
            if (clause.watch1 == false_lit) {
                clause.watch1 = lit;
            } else {
                clause.watch2 = lit;
            }
            int new_watch_idx = lit_to_watch_idx(lit, problem.nvars) - 1;
            problem.watches[new_watch_idx].push_back(clause_idx);
            return true;
        }
    }
    return false;
}

int propagate_units_watched(Problem &problem, vector<Assignment> &assignment,
                           vector<Trail> &local_assignments, int dl,bool scan_all) {
    vector<int> propagation_queue;

    if(local_assignments.empty()) {
        return -1; // No assignments to propagate
    }
    int var = local_assignments.back().var;
    int assigned_lit = assignment[var].val > 0 ? var : -var;
    propagation_queue.push_back(assigned_lit);
    
    if(scan_all){
        for (int var = 1; var < problem.nvars+1; var++) {
            if (assignment[var].val != 0) {
                int assigned_lit = assignment[var].val > 0 ? var : -var;
                propagation_queue.push_back(assigned_lit);
            }
        }
    }

    
    while (!propagation_queue.empty()) {
        int propagated_lit = propagation_queue.back();
        propagation_queue.pop_back();
        
        // Look at clauses watching the negation of this literal
        int false_lit = -propagated_lit;
        int false_watch_idx = lit_to_watch_idx(false_lit, problem.nvars) - 1;
        
        vector<int> &watch_list = problem.watches[false_watch_idx];
        
        // Process all clauses watching this now-false literal
        for (int i = 0; i < watch_list.size(); ) {
            int clause_idx = watch_list[i];
            Clause &clause = problem.clauses[clause_idx];
            
            // Check if clause is already satisfied
            bool satisfied = false;
            for (int lit : clause.literals) {
                if (is_lit_satisfied(lit, assignment)) {
                    satisfied = true;
                    break;
                }
            }
            
            if (satisfied) {
                i++;
                continue;
            }
            
            if (update_watches(problem, clause_idx, false_lit, assignment)) {
                //found new watch so done
                watch_list[i] = watch_list.back();
                watch_list.pop_back();
                continue;
            }
            
            // Could not find new watch - check the other watched literal
            int other_watch = (clause.watch1 == false_lit) ? clause.watch2 : clause.watch1;

            if(other_watch==0){
                return clause_idx; // Clause is unit clause, return it
            }

            if (is_lit_satisfied(other_watch, assignment)) {
                i++;
                continue;
            }
            
            if (is_lit_falsified(other_watch, assignment)) {
                stats.conflicts++;
                return clause_idx;
            }
            
            int unit_var = abs(other_watch);
            int8_t unit_val = (other_watch > 0) ? 1 : -1;
            
            if (assignment[unit_var].val == 0) {
                stats.propagations++;
                assignment[unit_var].val = unit_val;
                assignment[unit_var].decision = false;
                assignment[unit_var].clause = clause_idx;
                assignment[unit_var].dl = dl;
                problem.saved_phase[unit_var] = unit_val;
                
                local_assignments.push_back({.var = unit_var});
                propagation_queue.push_back(other_watch);
            } else if (assignment[unit_var].val != unit_val) {
                stats.conflicts++;
                return clause_idx;
            }
            
            i++;
        }
    }
    
    return -1;
}

void assign_pure_literals(Problem &problem, vector<Assignment> &assignment) {
    for (int var = 1; var <= problem.nvars; var++) {
        if (assignment[var].val != 0){
            continue; // skip already assigned
        }
        if (problem.pos_count[var] > 0 && problem.neg_count[var] == 0) {
            assignment[var].decision = false;
            assignment[var].clause = -1;
            assignment[var].dl = 0;
            assignment[var].val = 1;
        } 
        else if (problem.neg_count[var] > 0 && problem.pos_count[var] == 0) {
            assignment[var].decision = false;
            assignment[var].clause = -1;
            assignment[var].dl = 0;
            assignment[var].val = -1;
        }
    }
}

void analyze_conflict(Problem &problem, vector<Assignment> &assignment,
                      vector<Trail> &local_assignments, int conflict_clause, Clause &learned_clause, int &beta) {
    vector<bool> seen(problem.nvars + 1, false);
    int pathC = 0;
    int p = -1;
    int idx = local_assignments.size() - 1;
    Clause conflict = problem.clauses[conflict_clause];
    learned_clause.satisfied = false;
    learned_clause.learned = true;
    learned_clause.literals.clear();

    int curr_dl = assignment[local_assignments.back().var].dl;
    do {
        for (int i : conflict.literals) {
            int var = abs(i);
            if (!seen[var] && assignment[var].dl != 0) {
                seen[var] = true;
                if (assignment[var].dl == curr_dl) {
                    pathC++;
                } else {
                    learned_clause.literals.push_back(i);
                }
            }
        }

        while (idx >= 0 && !seen[local_assignments[idx].var]) idx--;
        p = local_assignments[idx].var;
        seen[p] = false;
        pathC--;
        idx--;

        if (pathC > 0) {
            int ante = assignment[p].clause;
            if (ante != -1) {
                conflict = problem.clauses[ante]; // update conflict
            } else {
                break;
            }
        }
    } while (pathC > 0);

    int up_lit = (assignment[p].val > 0) ? -p : p;
    learned_clause.literals.insert(learned_clause.literals.begin(), up_lit);

    for (int i = 0; i < learned_clause.literals.size(); i++) {
        bump_activity(problem, abs(learned_clause.literals[i]));
    }
    if(stats.learned_clauses % 500 == 0) {
        decay_activities(problem);
    }

    beta = 0;
    //start at 1 to find second lowest decision level
    for (int i = 1; i < learned_clause.literals.size(); i++) {
        int var = abs(learned_clause.literals[i]);
        beta = max(beta, assignment[var].dl);
    }
}

void add_learned_clause(Problem &problem, const Clause &learned_clause) {
    int clause_idx = problem.clauses.size();
    problem.clauses.push_back(learned_clause);
    problem.nclauses++;
    stats.learned_clauses++;

    // Update literal_to_clauses mapping
    for (int lit : learned_clause.literals) {
        int var = abs(lit);
        problem.literal_to_clauses[var].push_back(clause_idx);
    }

    // Set up watches for the new learned clause
    Clause &new_clause = problem.clauses[clause_idx];
    if (new_clause.literals.size() == 1) {
        new_clause.watch1 = new_clause.literals[0];
        new_clause.watch2 = 0;
        int watch_idx = lit_to_watch_idx(new_clause.watch1, problem.nvars) - 1;
        problem.watches[watch_idx].push_back(clause_idx);
    } else if (new_clause.literals.size() >= 2) {
        new_clause.watch1 = new_clause.literals[0];
        new_clause.watch2 = new_clause.literals[1];
        int watch1_idx = lit_to_watch_idx(new_clause.watch1, problem.nvars) - 1;
        int watch2_idx = lit_to_watch_idx(new_clause.watch2, problem.nvars) - 1;
        problem.watches[watch1_idx].push_back(clause_idx);
        problem.watches[watch2_idx].push_back(clause_idx);
    }
}

bool CDCL(Problem &problem, vector<Assignment> &assignment, vector<Trail> &local_assignments) {
    int dl = 0;
    int restart_conflict = 0;
    int restart_count = 0;
    LubySequence luby;
    int luby_val = luby.advance();
    while(true){
        int conflict_clause = propagate_units_watched(problem, assignment, local_assignments, dl,false);
        if(conflict_clause != -1) {
            if(dl==0){
                return false; // Conflict at root level, unsatisfiable
            }
            Clause learned_clause;
            int beta = 0;
            analyze_conflict(problem, assignment, local_assignments, conflict_clause, learned_clause, beta);
            add_learned_clause(problem, learned_clause);
            bool restarted = false;
            if(stats.conflicts >= restart_conflict+luby_val){
                restart_conflict = stats.conflicts;
                restart_count++;
                restarted = true;
                beta = 0;
                dl=0;
                luby_val = luby.advance();
            }

           // Backtrack
            while(!local_assignments.empty() && assignment[local_assignments.back().var].dl > beta) {
                int var = local_assignments.back().var;
                assignment[var].val = 0;
                assignment[var].decision = false;
                assignment[var].clause = -1;
                assignment[var].dl = 0;
                local_assignments.pop_back();
            }
            if(restarted) {
                continue;
            }

            dl = beta;
            int lit = learned_clause.literals[0];
            int var = abs(lit);
            int8_t val = (lit > 0 ? 1 : -1);
            assignment[var].val = val;
            problem.saved_phase[var] = val;
            assignment[var].decision = false;
            assignment[var].clause = problem.nclauses - 1;
            assignment[var].dl = dl;
            local_assignments.push_back({
                .var = var,
            });
        }
        else{
            int remaining_var = select_vsids(problem, assignment);
            if( remaining_var == -1) {
                return true;
            }
            dl++;
            stats.decisions++;
            if(problem.saved_phase[remaining_var] != 0) {
                assignment[remaining_var].val = problem.saved_phase[remaining_var];
            } else {
                assignment[remaining_var].val = random_bool(0.5) ? 1 : -1; // Randomly choose phase if not saved
            }
            problem.saved_phase[remaining_var] = assignment[remaining_var].val;
            assignment[remaining_var].decision = true;
            assignment[remaining_var].clause = -1;
            assignment[remaining_var].dl = dl;
            local_assignments.push_back({
                .var = remaining_var,
            });
        }
    }
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

    thread(watchdog, 20).detach();

    vector<Assignment> assignment(sat_problem.nvars + 1);
    for (int i = 1; i < sat_problem.nvars+1; i++) {
        assignment[i] = (Assignment){
            .decision = false,
            .clause = -1,
            .val = 0,
            .dl = 0
        };
    }

    vector<Trail> local_assignments;
    assign_pure_literals(sat_problem, assignment);
    int conflict = propagate_units_watched(sat_problem, assignment, local_assignments, 0,false);
    if( conflict != -1) {
        cout << "UNSATISFIABLE\n";
        cout.flush();
        return 0;
    }
    if (CDCL(sat_problem, assignment, local_assignments)) {
        for (int i = 1; i < sat_problem.nvars + 1; i++) {
            cout << "x" << i << " = " << (assignment[i].val > 0 ? "true" : "false")
                 << '\n';
        }
        cout << "SATISFIABLE\n";
    } else {
        cout << "UNSATISFIABLE\n";
    }
    cout << "\nSolver statistics:\n";
    cout << "Decisions: " << stats.decisions << '\n';
    cout << "Propagations: " << stats.propagations << '\n';
    cout << "Conflicts: " << stats.conflicts << '\n';
    cout << "Learned clauses: " << stats.learned_clauses << '\n';
    cout.flush();
    return 0;
}
