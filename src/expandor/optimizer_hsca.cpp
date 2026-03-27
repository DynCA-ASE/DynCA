#include "optimizer_hsca.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>
#include "expandor/optimizer_scalable.hpp"
#include "util/random.hpp"

using namespace expandor;

int _Optimizer_HSCA::get_which_remove() {
    uint64_t mini = 0;
    vector<int> besttcs;

    int testcase_size = testcases.size();
    for (int i = 0; i < testcase_size; i++) {
        uint64_t res = sum_weight[i];

        if (besttcs.empty() || res == mini) {
            mini = res;
            besttcs.emplace_back(i);
        } else if (res < mini) {
            mini = res;
            besttcs.clear(), besttcs.emplace_back(i);
        }
    }
    int pos = besttcs[Random::bound(besttcs.size())];
    return testcase_pos_to_idx[pos];
}

void _Optimizer_HSCA::remove_testcase_greedily() {
    int idx = get_which_remove();
    UpdateInfo_remove_testcase(idx);
}

void _Optimizer_HSCA::remove_testcase_randomly() {
    int pos = Random::bound(testcases.size());
    UpdateInfo_remove_testcase(testcase_pos_to_idx[pos]);
}

void _Optimizer_HSCA::change_bit(int v, int ad, const vector<int> &tc,
                                 vector<int> &cur_clauses_cov) {
    const auto &pos_in_cls = cnf->get_pos_in_cls();
    const auto &neg_in_cls = cnf->get_neg_in_cls();
    int vid = abs(v) - 1;
    int curbit = tc[vid], tt = v > 0;
    if (curbit != tt) {
        const vector<int> &var_cov_old = (curbit ? pos_in_cls[vid + 1] : neg_in_cls[vid + 1]);
        const vector<int> &var_cov_new = (tt ? pos_in_cls[vid + 1] : neg_in_cls[vid + 1]);
        for (int cid : var_cov_new) cur_clauses_cov[cid] += ad;
        for (int cid : var_cov_old) cur_clauses_cov[cid] -= ad;
    }
}

bool _Optimizer_HSCA::check_force_tuple(const vector<int> &tc, t_tuple tp,
                                        vector<int> &cur_clauses_cov) {
    for (int i = 0; i < strength; i++) {
        change_bit(tp.v[i], 1, tc, cur_clauses_cov);

        int pi = abs(tp.v[i]) - 1;
        if (~saved_expandor->group_id[pi])
            for (int x : saved_expandor->member[saved_expandor->group_id[pi]])
                if (x != pi) change_bit(-(x + 1), 1, tc, cur_clauses_cov);
    }

    bool has0 = false;
    const int nclauses = cnf->get_num_clauses();
    for (int i = 0; i < nclauses; ++i) {
        if (cur_clauses_cov[i] == 0) {
            has0 = true;
            break;
        }
    }

    for (int i = 0; i < strength; i++) {
        change_bit(tp.v[i], -1, tc, cur_clauses_cov);

        int pi = abs(tp.v[i]) - 1;
        if (~saved_expandor->group_id[pi])
            for (int x : saved_expandor->member[saved_expandor->group_id[pi]])
                if (x != pi) change_bit(-(x + 1), -1, tc, cur_clauses_cov);
    }
    return has0 ? false : true;
}

pair<uint64_t, uint64_t> _Optimizer_HSCA::get_gain_for_forcetestcase_2(int tcid,
                                                                       const vector<int> &tc2) {
    const vector<int> &tc = testcases[tcid];
    uint64_t break_cnt = 0, gain_cnt = 0;

    for (int tpid : uncovered_tuples) {
        const t_tuple &t = tuples_U[tpid];
        if (is_covered(tc2, t)) gain_cnt += weight[tpid];
    }

    int different_num = 0;
    for (uint i = 0; i < cnf->get_num_cared(); i++)
        if (tc[i] != tc2[i]) different_num++;
    vector<uint64_t> break_cnt_num(different_num + 2, 0);

    for (IntrusiveListNode *p = unique_covered_tuples[tcid].head; p != NULL; p = p->nxt) {
        int tpid = p->val;
        const t_tuple &t = tuples_U[tpid];

        if (!is_covered(tc2, t)) continue;
        int num = get_different_var(tc, t).first;
        break_cnt_num[num] += weight[tpid];
    }

    for (int i = 1; i <= different_num; i++) break_cnt += break_cnt_num[i] / i;

    return {break_cnt, gain_cnt};
}

pair<bool, pair<uint64_t, uint64_t>> _Optimizer_HSCA::get_gain_for_forcetuple_2(int tcid,
                                                                                t_tuple tp) {
    const vector<int> &tc = testcases[tcid];
    vector<int> &cur_clauses_cov = clauses_cov[tcid];

    if (!check_force_tuple(tc, tp, cur_clauses_cov)) return {false, {0, 0}};
    vector<int> new_tc = get_new_tc(tc, tp);

    return {true, get_gain_for_forcetestcase_2(tcid, new_tc)};
}

pair<uint64_t, uint64_t> _Optimizer_HSCA::get_gain_for_forcetestcase(int tcid,
                                                                     const vector<int> &tc2) {
    uint64_t break_cnt = sum_weight[tcid];
    uint64_t gain_cnt = 0;

    for (IntrusiveListNode *p = unique_covered_tuples[tcid].head; p != NULL; p = p->nxt) {
        int tpid = p->val;
        const t_tuple &t = tuples_U[tpid];
        if (is_covered(tc2, t)) gain_cnt += weight[tpid];
    }
    for (int tpid : uncovered_tuples) {
        const t_tuple &t = tuples_U[tpid];
        if (is_covered(tc2, t)) gain_cnt += weight[tpid];
    }
    return {break_cnt, gain_cnt};
}

pair<bool, pair<uint64_t, uint64_t>> _Optimizer_HSCA::get_gain_for_forcetuple(int tcid,
                                                                              t_tuple tp) {
    const vector<int> &tc = testcases[tcid];
    vector<int> &cur_clauses_cov = clauses_cov[tcid];

    if (!check_force_tuple(tc, tp, cur_clauses_cov)) return {false, {0, 0}};
    vector<int> new_tc = get_new_tc(tc, tp);

    return {true, get_gain_for_forcetestcase(tcid, new_tc)};
}

void _Optimizer_HSCA::forcetuple(int tcid, t_tuple tp) {
    forcetestcase(tcid, get_new_tc(testcases[tcid], tp));
}

void _Optimizer_HSCA::backtrack_row(int tcid, t_tuple tp) {
    for (const vector<int> &tc : best_testcases)
        if (is_covered(tc, tp)) {
            forcetestcase(tcid, tc);
            break;
        }
}

bool _Optimizer_HSCA::gradient_descent() {
    int uncovered_cnt = uncovered_tuples.size(), sampling_num = 100;
    if (use_cdcl_solver) sampling_num = uncovered_cnt;

    if (uncovered_cnt > sampling_num)
        for (int i = 0; i < sampling_num; i++) {
            int offset = Random::bound(uncovered_cnt - i);
            std::swap(uncovered_tuples[i], uncovered_tuples[i + offset]);
        }
    else {
        int offset = Random::bound(uncovered_cnt);
        std::swap(uncovered_tuples[0], uncovered_tuples[offset]);
    }

    long long maxi = 0;
    vector<pair<int, int>> best_choices;
    vector<int> first_best;

    int num = std::min(uncovered_cnt, sampling_num);
    int testcase_size = testcases.size();
    for (int i = 0; i < num; i++) {
        int tpid = uncovered_tuples[i];
        const t_tuple &tp = tuples_U[tpid];

        for (int j = 0; j < testcase_size; j++) {
            if (is_taboo(j, tp)) continue;

            const vector<int> &tc = testcases[j];
            pair<int, int> difference = get_different_var(tc, tp);
            if (difference.first != 1) continue;

            auto res = get_gain_for_forcetuple(j, tp);
            if (res.first) {
                long long net_gain = res.second.second - res.second.first;

                if (best_choices.empty() || net_gain == maxi) {
                    maxi = net_gain;
                    best_choices.emplace_back(std::make_pair(tpid, j));
                    if (i == 0) first_best.emplace_back(j);
                } else if (net_gain > maxi) {
                    best_choices.clear();
                    best_choices.emplace_back(std::make_pair(tpid, j));
                    maxi = net_gain;
                    if (i == 0) {
                        first_best.clear();
                        first_best.emplace_back(j);
                    }
                }
            }
        }
    }

    if (maxi > 0 && (!best_choices.empty())) {
        int choice = Random::bound(best_choices.size());
        int tpid = best_choices[choice].first;

        const t_tuple &tp = tuples_U[tpid];
        int besttcid = best_choices[choice].second;

        set_taboo(besttcid, tp);
        forcetuple(besttcid, tp);
        return true;
    }

    if (config->use_weight)
        for (int tpid : uncovered_tuples) weight[tpid]++;

    const t_tuple &tp = tuples_U[uncovered_tuples[0]];

    if (Random::bound(1000) < 1) {
        backtrack_row(Random::bound(testcase_size), tp);
        return true;
    }

    if (!first_best.empty()) {
        int besttcid = first_best[Random::bound(first_best.size())];
        set_taboo(besttcid, tp);
        forcetuple(besttcid, tp);
        return true;
    }

    random_greedy_step(maxi);
    return false;
}

bool _Optimizer_HSCA::greedy_step_forced(t_tuple tp) {
    const int nvar = cnf->get_num_variables();

    cdcl_solver->clear_assumptions();
    for (int i = 0; i < strength; i++) {
        int pi = abs(tp.v[i]) - 1, vi = tp.v[i] > 0;
        cdcl_solver->add_assumption(pi, vi);
    }

    long long maxi = 0;
    vector<int> besttcids;
    vector<vector<int>> besttc2s;

    int testcase_size = testcases.size();
    for (int i = 0; i < testcase_size; i++) {
        if (is_taboo(i, tp)) continue;

        for (int j = 0; j < nvar; j++) cdcl_solver->set_polarity(j, testcases[i][j] == 0);
        vector<int> tc2 = vector<int>(nvar, 0);
        bool ret = cdcl_solver->solve();
        if (!ret) {
            std::cout << "c \033[1;31mError: SAT solve failing!\033[0m" << std::endl;
            std::exit(1);
        }
        cdcl_solver->get_solution(tc2);

        auto res = get_gain_for_forcetestcase_2(i, tc2);
        long long net_gain = res.second - res.first;

        if (besttcids.empty() || net_gain == maxi) {
            maxi = net_gain;
            besttcids.emplace_back(i);
            besttc2s.emplace_back(tc2);
        } else if (net_gain > maxi) {
            maxi = net_gain;
            besttcids.clear(), besttcids.emplace_back(i);
            besttc2s.clear(), besttc2s.emplace_back(tc2);
        }
    }

    if (besttcids.empty()) return false;

    int idx = Random::bound(besttcids.size());
    int besttcid = besttcids[idx];
    vector<int> besttc2 = besttc2s[idx];

    set_taboo(besttcid, besttc2);
    forcetestcase(besttcid, besttc2);

    return true;
}

void _Optimizer_HSCA::random_greedy_step(long long maxi) {
    int uncovered_cnt = uncovered_tuples.size();
    int picked_tuple = Random::bound(uncovered_cnt);
    int tpid = uncovered_tuples[picked_tuple];
    const t_tuple &tp = tuples_U[tpid];

    vector<int> besttcids;

    int testcase_size = testcases.size();
    for (int i = 0; i < testcase_size; i++) {
        if (is_taboo(i, tp)) continue;

        auto res = get_gain_for_forcetuple_2(i, tp);
        if (res.first) {
            long long net_gain = res.second.second - res.second.first;

            if (net_gain == maxi)
                besttcids.emplace_back(i);
            else if (net_gain > maxi) {
                maxi = net_gain;
                besttcids.clear();
                besttcids.emplace_back(i);
            }
        }
    }

    if (Random::bound(1000) < 1 && greedy_step_forced(tp)) return;

    if (!besttcids.empty()) {
        int besttcid = besttcids[Random::bound(besttcids.size())];
        set_taboo(besttcid, tp);
        forcetuple(besttcid, tp);
        return;
    }

    if (Random::bound(1000) < (uint)config->forced_greedy_percent && greedy_step_forced(tp)) return;

    if (Random::bound(10000) < 1) random_step();

    backtrack_row(Random::bound(testcase_size), tp);
}

bool _Optimizer_HSCA::check_for_flip(int tcid, int vid) {
    const vector<int> &tc = testcases[tcid];
    int curbit = tc[vid];

    const auto &pos_in_cls = cnf->get_pos_in_cls();
    const auto &neg_in_cls = cnf->get_neg_in_cls();
    const vector<int> &var_cov_old = (curbit ? pos_in_cls[vid + 1] : neg_in_cls[vid + 1]);
    const vector<int> &var_cov_new = (curbit ? neg_in_cls[vid + 1] : pos_in_cls[vid + 1]);
    vector<int> &cur_clauses_cov = clauses_cov[tcid];

    bool has0 = true;
    for (int cid : var_cov_new) cur_clauses_cov[cid]++;
    for (int cid : var_cov_old) {
        cur_clauses_cov[cid]--;
        if (cur_clauses_cov[cid] == 0) has0 = false;
    }

    for (int cid : var_cov_new) --cur_clauses_cov[cid];
    for (int cid : var_cov_old) ++cur_clauses_cov[cid];

    return has0;
}

void _Optimizer_HSCA::flip_bit(int tid, int vid) {
    vector<int> tc2 = testcases[tid];
    tc2[vid] ^= 1;
    forcetestcase(tid, tc2);
}

void _Optimizer_HSCA::random_step() {
    const int nvar = cnf->get_num_variables();
    long long all_nums = testcases.size() * nvar;
    vector<int> flip_order;
    flip_order = vector<int>(all_nums, 0);
    std::iota(flip_order.begin(), flip_order.end(), 0);
    std::shuffle(flip_order.begin(), flip_order.end(), Random::get());

    for (int idx : flip_order) {
        int tid = idx / nvar, vid = idx % nvar;

        if (check_for_flip(tid, vid)) {
            flip_bit(tid, vid);
            break;
        }
    }
}

void _Optimizer_HSCA::remove_unnecessary_tc() {
    int testcase_size = testcases.size();
    for (int i = 0; i < testcase_size; i++) {
        if (!sum_weight[i]) {
            UpdateInfo_remove_testcase(testcase_pos_to_idx[i]);
            std::cout << "\033[;32mc current remove " << i << " \033[0m" << std::endl;
            return;
        }
    }
}
