#pragma once

#include "expandor/optimizer.hpp"
#include "util/random.hpp"

namespace expandor {
class _Optimizer_HSCA : Optimizer {
    friend Optimizer;

    int get_which_remove();
    void remove_testcase_greedily();
    void remove_testcase_randomly();

    void change_bit(int v, int ad, const vector<int> &tc, vector<int> &cur_clauses_cov);
    bool check_force_tuple(const vector<int> &tc, t_tuple tp, vector<int> &cur_clauses_cov);
    std::pair<uint64_t, uint64_t> get_gain_for_forcetestcase(int tcid, const vector<int> &tc2);
    std::pair<bool, std::pair<uint64_t, uint64_t>> get_gain_for_forcetuple(int tcid, t_tuple tp);
    std::pair<uint64_t, uint64_t> get_gain_for_forcetestcase_2(int tcid, const vector<int> &tc2);
    std::pair<bool, std::pair<uint64_t, uint64_t>> get_gain_for_forcetuple_2(int tcid, t_tuple tp);
    void forcetuple(int tcid, t_tuple tp);
    void backtrack_row(int tcid, t_tuple tp);
    bool gradient_descent();

    void adaptive_adjustment(int strength, int nvar, int nclauses, int group_num);
    bool greedy_step_forced(t_tuple tp);
    void random_greedy_step(t_tuple tp, long long maxi);
    void random_greedy_step();
    void random_greedy_step(long long maxi);

    bool check_for_flip(int tcid, int vid);
    void flip_bit(int tid, int vid);
    void random_step();

    void remove_unnecessary_tc();

    std::pair<int, int> get_different_var(const vector<int> &tc, const t_tuple &t) {
        int num = 0, first_var = -1;
        for (int i = 0; i < strength; i++) {
            int pi = abs(t.v[i]) - 1, vi = t.v[i] > 0;
            if (tc[pi] != vi) {
                num++;
                if (first_var == -1) first_var = pi;
            }
        }
        return {num, first_var};
    }

    vector<int> get_new_tc(const vector<int> &tc, const t_tuple &t) {
        vector<int> tc2 = tc;
        for (int i = 0; i < strength; i++) {
            int pi = abs(t.v[i]) - 1, vi = t.v[i] > 0;
            tc2[pi] = vi;
            if (~saved_expandor->group_id[pi])
                for (int x : saved_expandor->member[saved_expandor->group_id[pi]])
                    if (x != pi) tc2[x] = 0;
        }
        return tc2;
    }

    bool is_taboo(int tcid, const t_tuple &t) {
        if ((int) Random::bound(100) >= config->cell_taboo_percent) {
            return greedy_limit - last_greedy_time[tcid] <= config->taboo_size;
        } else {
            const vector<int> &tc = testcases[tcid];
            for (int i = 0; i < strength; i++) {
                int pi = abs(t.v[i]) - 1, vi = t.v[i] > 0;
                if (tc[pi] == vi) continue;

                if (greedy_limit - last_greedy_time_cell[tcid][pi] <= config->taboo_size) return true;
                if (~saved_expandor->group_id[pi])
                    for (int x : saved_expandor->member[saved_expandor->group_id[pi]])
                        if (greedy_limit - last_greedy_time_cell[tcid][x] <= config->taboo_size)
                            return true;
            }
            return false;
        }
    }

    void set_taboo(int tcid, const t_tuple &t) {
        ++greedy_limit;
        last_greedy_time[tcid] = greedy_limit;
        const vector<int> &tc = testcases[tcid];
        for (int i = 0; i < strength; i++) {
            int pi = abs(t.v[i]) - 1, vi = t.v[i] > 0;
            if (tc[pi] != vi) {
                last_greedy_time_cell[tcid][pi] = greedy_limit;

                if (~saved_expandor->group_id[pi])
                    for (int x : saved_expandor->member[saved_expandor->group_id[pi]])
                        last_greedy_time_cell[tcid][x] = greedy_limit;
            }
        }
    }

    void set_taboo(int tcid, const vector<int> &tc2) {
        ++greedy_limit;
        last_greedy_time[tcid] = greedy_limit;
        const vector<int> &tc = testcases[tcid];
        const int nvar = cnf->get_num_variables();
        for (int i = 0; i < nvar; i++)
            if (tc[i] != tc2[i]) last_greedy_time_cell[tcid][i] = greedy_limit;
    }
};
}  // namespace expandor
