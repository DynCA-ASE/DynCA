#include "optimizer_scalable.hpp"
#include "util/random.hpp"

using namespace expandor;

int _Optimizer_Scalable::new_uncovered_tuples_after_remove_testcase(int tcid) {
    uptate_unique_covered(tcid);
    int tcid_p = testcase_idx_to_pos[tcid];
    return unique_covered_tuples[tcid_p].size();
}

int _Optimizer_Scalable::get_which_remove() {
    int besttc = -1, mini = 0;
    int testcase_size = testcases.size();
    for (int i = 0; i < testcase_size; ++i) {
        int idx = testcase_pos_to_idx[i];
        int res = new_uncovered_tuples_after_remove_testcase(idx);
        if (besttc == -1 || res < mini) {
            mini = res, besttc = i;
            if (mini == 0) break;
        }
    }
    return testcase_pos_to_idx[besttc];
}

void _Optimizer_Scalable::remove_testcase_greedily() {
    int idx = get_which_remove();
    UpdateInfo_remove_testcase(idx);
}

void _Optimizer_Scalable::change_bit(int v, int ad, const vector<int> &tc,
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

pair<bool, pair<int, int>> _Optimizer_Scalable::get_gain_for_forcetuple(int tcid,
                                                                        t_tuple chosen_tp) {
    const vector<int> &tc = testcases[tcid];
    const int nclauses = cnf->get_num_clauses();
    vector<int> tc2 = tc;
    vector<int> &cur_clauses_cov = clauses_cov[tcid];

    for (int i = 0; i < strength; i++) {
        change_bit(chosen_tp.v[i], 1, tc, cur_clauses_cov);
    }
    bool has0 = false;
    for (int i = 0; i < nclauses; ++i) {
        if (cur_clauses_cov[i] == 0) {
            has0 = true;
            break;
        }
    }

    for (int i = 0; i < strength; i++) {
        change_bit(chosen_tp.v[i], -1, tc, cur_clauses_cov);
    }
    if (has0) return {false, {0, 0}};

    for (int i = 0; i < strength; i++) {
        tc2[abs(chosen_tp.v[i]) - 1] = (chosen_tp.v[i] > 0);
    }
    auto res = get_gain_for_forcetestcase(tcid, tc2);
    return {true, res};
}

void _Optimizer_Scalable::forcetuple(int tid, t_tuple tp) {
    vector<int> tc2 = testcases[tid];

    for (int i = 0; i < strength; i++) {
        tc2[abs(tp.v[i]) - 1] = (tp.v[i] > 0);
    }

    forcetestcase(tid, tc2);
}

bool _Optimizer_Scalable::random_greedy_step() {
    int uncovered_cnt = uncovered_tuples.size();
    int picked_tuple = Random::bound(uncovered_cnt);

    int tpid = uncovered_tuples[picked_tuple];
    const t_tuple &tp = tuples_U[tpid];
    int besttcid = -1;
    long long maxi = 0;

    int testcase_size = testcases.size();
    for (int i = 0; i < testcase_size; i++) {
        if (greedy_limit - last_greedy_time[i] <= config->taboo_size) continue;

        auto res = get_gain_for_forcetuple(i, tp);
        if (res.first) {
            int net_gain = res.second.second - res.second.first;
            if (net_gain > maxi) besttcid = i, maxi = net_gain;
        }
    }

    if (besttcid != -1) {
        forcetuple(besttcid, tp);
        ++greedy_limit;
        last_greedy_time[besttcid] = greedy_limit;
        return true;
    }

    if (Random::bound(100) < (uint)config->forced_greedy_percent) {
        return greedy_step_forced(tp);
    }

    return false;
}

bool _Optimizer_Scalable::greedy_step_forced(t_tuple tp) {
    const int nvar = cnf->get_num_variables();
    int vid[mx_strength], bit[mx_strength];
    for (int i = 0; i < strength; i++) {
        vid[i] = abs(tp.v[i]) - 1;
        bit[i] = tp.v[i] > 0;
    }

    if (use_cdcl_solver) {
        cdcl_solver->clear_assumptions();
        for (int i = 0; i < strength; i++) {
            cdcl_solver->add_assumption(vid[i], bit[i]);
        }
    } else {
        cdcl_sampler->clear_assumptions();
        for (int i = 0; i < strength; i++) {
            cdcl_sampler->add_assumption(vid[i], bit[i]);
        }
    }

    int besttcid = -1;
    long long maxi = 0;
    vector<int> besttc2;

    vector prob(nvar, std::make_pair(0, 0));

    int testcase_size = testcases.size();
    for (int i = 0; i < testcase_size; i++) {
        if (greedy_limit - last_greedy_time[i] <= config->taboo_size) continue;

        if (use_cdcl_solver) {
            for (int j = 0; j < nvar; j++) {
                cdcl_solver->set_polarity(j, testcases[i][j] == 0);
            }
        } else {
            for (int j = 0; j < nvar; j++) {
                if (testcases[i][j])
                    prob[j] = {1, 0};
                else
                    prob[j] = {0, 1};
            }
            cdcl_sampler->set_prob(prob);
        }

        vector<int> tc2(nvar, 0);
        if (use_cdcl_solver) {
            bool ret = cdcl_solver->solve();
            if (!ret) {
                std::cout << "c \033[1;31mError: SAT solve failing!\033[0m" << std::endl;
                return false;
            }
            cdcl_solver->get_solution(tc2);
        } else {
            vector<int> tc2 = vector<int>(nvar, 0);
            cdcl_sampler->get_solution(tc2);
        }

        auto res = get_gain_for_forcetestcase(i, tc2);

        int net_gain = res.second - res.first;
        if (besttcid == -1 || net_gain > maxi) {
            besttcid = i;
            besttc2 = tc2;
            maxi = net_gain;
        }
    }
    if (besttcid == -1) return false;

    forcetestcase(besttcid, besttc2);

    ++greedy_limit;
    last_greedy_time[besttcid] = greedy_limit;
    return true;
}

pair<int, int> _Optimizer_Scalable::get_gain_for_forcetestcase(int tcid, const vector<int> &tc2) {
    int tcid_idx = testcase_pos_to_idx[tcid];
    uptate_unique_covered(tcid_idx);
    int break_cnt = unique_covered_tuples[tcid].size();
    int gain_cnt = 0;

    for (IntrusiveListNode *p = unique_covered_tuples[tcid].head; p != NULL; p = p->nxt) {
        int tpid = p->val;
        const t_tuple &t = tuples_U[tpid];
        if (is_covered(tc2, t)) gain_cnt++;
    }

    for (int tpid : uncovered_tuples) {
        const t_tuple &t = tuples_U[tpid];
        if (is_covered(tc2, t)) gain_cnt++;
    }

    return {break_cnt, gain_cnt};
}

bool _Optimizer_Scalable::check_for_flip(int tcid, int vid) {
    const auto &pos_in_cls = cnf->get_pos_in_cls();
    const auto &neg_in_cls = cnf->get_neg_in_cls();

    const vector<int> &tc = testcases[tcid];
    int curbit = tc[vid];

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

void _Optimizer_Scalable::flip_bit(int tid, int vid) {
    vector<int> tc2 = testcases[tid];
    tc2[vid] ^= 1;
    forcetestcase(tid, tc2);
}

void _Optimizer_Scalable::random_step() {
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
