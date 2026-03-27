#include "expandor/optimizer.hpp"
#include <queue>
#include <thread>
#include "expandor/optimizer_hsca.hpp"
#include "expandor/optimizer_scalable.hpp"
#include "util/cpp23views.h"
#include "util/interrupt.hpp"
#include "util/random.hpp"

using namespace expandor;

static constexpr size_t thread_num = 16;

static_assert(sizeof(Optimizer) == sizeof(_Optimizer_Scalable),
              "_Optimizer_Scalable should not extend any fields");
static_assert(sizeof(Optimizer) == sizeof(_Optimizer_HSCA),
              "_Optimizer_HSCA should not extend any fields");

Optimizer::Optimizer(Expandor &expandor, const OptArgument &arg)
    : arguments(arg), saved_expandor(&expandor) {
    config = arg.configs.begin().base();
    cnf = expandor.cnf;
    strength = expandor.strength;

    last_strength_testcases.swap(expandor.old_testcase);
    testcases.swap(expandor.new_testcase);
    auto testcase_size = testcases.size();
    testcase_idx = testcase_size - 1;

    tuples_U = std::move(expandor.tuples_U);

    rebind_pos_to_idx();

    const int nvar = cnf->get_num_variables();
    cdcl_solver = std::make_unique<CDCLSolver::Solver>();
    cdcl_solver->read_clauses(nvar, cnf->get_clauses());
    cdcl_sampler =
        std::make_unique<ExtMinisat::SamplingSolver>(nvar, cnf->get_clauses(), 1, true, 0);

    std::cout << "Optimizer init success" << std::endl;
}

void Optimizer::rebind_pos_to_idx() {
    int testcase_size = testcases.size(), tuples_nums = tuples_U.size();

    covered_tuples.clear();
    uncovered_tuples.clear();
    tuples_idx_to_pos.clear();
    covered_tuples.resize(tuples_nums);
    tuples_idx_to_pos.resize(tuples_nums);
    std::iota(covered_tuples.begin(), covered_tuples.end(), 0);
    std::iota(tuples_idx_to_pos.begin(), tuples_idx_to_pos.end(), 0);

    testcase_pos_to_idx.clear();
    testcase_idx_to_pos.clear();
    testcase_pos_to_idx.resize(testcase_size);
    testcase_idx_to_pos.resize(testcase_size);
    std::iota(testcase_pos_to_idx.begin(), testcase_pos_to_idx.end(), 0);
    std::iota(testcase_idx_to_pos.begin(), testcase_idx_to_pos.end(), 0);

    unique_covered_tuples.clear();
    tc_covered_tuples.clear();
    unique_covered_tuples.resize(testcase_size);
    tc_covered_tuples.resize(testcase_size);

    covered_times.clear();
    covered_testcases.clear();
    covered_times.resize(tuples_nums, 0);
    covered_testcases.resize(tuples_nums);
    const int every = tuples_nums / thread_num;
    std::vector<std::thread> threads;
    vector<std::queue<std::pair<int, int>>> thread_covered_testcases(thread_num),
        thread_tc_covered_tuples(thread_num);
    for (uint id = 0; id < thread_num; ++id) {
        threads.emplace_back([&, id] {
            auto &my_covered_testcases = thread_covered_testcases[id];
            auto &my_tc_covered_tuples = thread_tc_covered_tuples[id];
            const int st = every * id + std::min<int>(id, tuples_nums % thread_num);
            const int ed = every * (id + 1) + std::min<int>(id + 1, tuples_nums % thread_num);
            for (int p = st; p < ed; p++) {
                const t_tuple &t = tuples_U[p];
                for (int pos = 0; pos < testcase_size; pos++) {
                    const auto &tc = testcases[pos];
                    if (is_covered(tc, t)) {
                        covered_times[p] += 1;
                        my_covered_testcases.emplace(p, testcase_pos_to_idx[pos]);
                        my_tc_covered_tuples.emplace(pos, p);
                    }
                }
            }
        });
    }
    for (auto &t : threads) t.join();
    threads.clear();
    for (auto &q : thread_covered_testcases) {
        while (!q.empty()) {
            auto [k, v] = q.front();
            q.pop();
            covered_testcases[k].insert(v);
        }
    }
    for (auto &q : thread_tc_covered_tuples) {
        while (!q.empty()) {
            auto [k, v] = q.front();
            q.pop();
            tc_covered_tuples[k].insert(v);
        }
    }
    thread_covered_testcases.clear();
    thread_tc_covered_tuples.clear();

    weight.clear();
    sum_weight.clear();
    unique_node.clear();
    weight.resize(tuples_nums, 1);        // weight of tuples
    sum_weight.resize(testcase_size, 0);  // weight of testcase
    unique_node.resize(tuples_nums + 1);  // unique node for tuples
    for (int p = 0; p < tuples_nums; p++) {
        if (covered_times[p] == 1) {
            int idx = covered_testcases[p].front();
            int pos = testcase_idx_to_pos[idx];
            unique_covered_tuples[pos].insert(p);

            unique_node[p] = unique_covered_tuples[pos].tail;
            sum_weight[pos] += 1;
        }
    }

    const int nvar = cnf->get_num_variables();
    const auto &pos_in_cls = cnf->get_pos_in_cls();
    const auto &neg_in_cls = cnf->get_neg_in_cls();
    clauses_cov.clear();
    clauses_cov.resize(testcase_size, vector<int>(cnf->get_num_clauses(), 0));
    for (int i = 0; i < testcase_size; i++) {
        const vector<int> &testcase = testcases[i];
        vector<int> &cur_clauses_cov = clauses_cov[i];
        for (int j = 0; j < nvar; j++) {
            const vector<int> &vec = (testcase[j] ? pos_in_cls[j + 1] : neg_in_cls[j + 1]);
            for (int x : vec) ++cur_clauses_cov[x];
        }
    }

    greedy_limit = 0;
    last_greedy_time.clear();
    last_greedy_time_cell.clear();
    if (config->taboo_size) last_greedy_time.resize(testcase_size, -config->taboo_size - 1);
    last_greedy_time_cell.resize(testcase_size + 1);
    for (int i = 0; i < testcase_size; i++) {
        last_greedy_time_cell[i].resize(nvar + 1);
        for (int j = 0; j < nvar; j++) last_greedy_time_cell[i][j] = -config->taboo_size - 1;
    }
}

void Optimizer::UpdateInfo_remove_testcase(int tcid_idx) {
    int tcid = testcase_idx_to_pos[tcid_idx];

    vector<int> break_pos, unique_id;
    for (IntrusiveListNode *p = tc_covered_tuples[tcid].head; p != NULL; p = p->nxt) {
        int tpid = p->val;
        covered_times[tpid]--;
        if (covered_times[tpid] == 0) {
            int pos = tuples_idx_to_pos[tpid];
            break_pos.emplace_back(pos);
            uncovered_tuples.emplace_back(tpid);
        } else if (covered_times[tpid] == 1)
            unique_id.emplace_back(tpid);
    }

    testcase_idx_to_pos[tcid_idx] = -1;
    for (int tpid : unique_id) {
        update_covered_testcases(tpid);
        int idx = covered_testcases[tpid].front();
        int pos = testcase_idx_to_pos[idx];
        unique_covered_tuples[pos].insert(tpid);

        unique_node[tpid] = unique_covered_tuples[pos].tail;
        sum_weight[pos] += weight[tpid];
    }

    sort(break_pos.begin(), break_pos.end());
    int break_num = break_pos.size();
    for (int i = break_num - 1; i >= 0; i--) {
        int pos = break_pos[i];
        if (pos != (int)covered_tuples.size() - 1) {
            int idx_new = covered_tuples.back();
            covered_tuples[pos] = idx_new;
            tuples_idx_to_pos[idx_new] = pos;
        }
        covered_tuples.pop_back();
    }

    int testcase_size = testcases.size();
    if (tcid != testcase_size - 1) {
        testcases[tcid] = std::move(testcases[testcase_size - 1]);
        last_greedy_time[tcid] = last_greedy_time[testcase_size - 1];
        last_greedy_time_cell[tcid] = last_greedy_time_cell[testcase_size - 1];
        clauses_cov[tcid] = std::move(clauses_cov[testcase_size - 1]);
        tc_covered_tuples[tcid] = std::move(tc_covered_tuples[testcase_size - 1]);
        unique_covered_tuples[tcid] = std::move(unique_covered_tuples[testcase_size - 1]);
        sum_weight[tcid] = sum_weight[testcase_size - 1];

        int idx = testcase_pos_to_idx[testcase_size - 1];
        testcase_idx_to_pos[idx] = tcid;
        testcase_pos_to_idx[tcid] = idx;
    }

    testcases.pop_back();
    last_greedy_time.pop_back();
    last_greedy_time_cell.pop_back();
    clauses_cov.pop_back();
    tc_covered_tuples.pop_back();
    unique_covered_tuples.pop_back();
    sum_weight.pop_back();
}

void Optimizer::uptate_unique_covered(int tcid) {
    int tcid_p = testcase_idx_to_pos[tcid];
    for (IntrusiveListNode *p = unique_covered_tuples[tcid_p].head, *q; p != NULL; p = q) {
        q = p->nxt;
        int tpid = p->val;
        if (covered_times[tpid] != 1) unique_covered_tuples[tcid_p].erase(p);
    }
}

void Optimizer::update_covered_testcases(int tpid) {
    for (IntrusiveListNode *p = covered_testcases[tpid].head, *q; p != NULL; p = q) {
        q = p->nxt;
        int tcid = p->val;
        if (testcase_idx_to_pos[tcid] < 0) covered_testcases[tpid].erase(p);
    }
}

void Optimizer::forcetestcase(int tcid, const vector<int> &tc2) {
    int tcid_idx = testcase_pos_to_idx[tcid];

    testcase_idx_to_pos[tcid_idx] = -1;

    testcase_idx++;
    testcase_pos_to_idx[tcid] = testcase_idx;
    testcase_idx_to_pos.emplace_back(tcid);

    vector<int> break_pos, unique_id;

    for (IntrusiveListNode *p = tc_covered_tuples[tcid].head; p != NULL; p = p->nxt) {
        int tpid = p->val;
        covered_times[tpid]--;
        if (covered_times[tpid] == 0) {
            break_pos.emplace_back(tuples_idx_to_pos[tpid]);
            uncovered_tuples.emplace_back(tpid);
        }
        if (covered_times[tpid] == 1) unique_id.emplace_back(tpid);
    }

    sort(break_pos.begin(), break_pos.end());
    int break_num = break_pos.size();
    for (int i = break_num - 1; i >= 0; i--) {
        int p = break_pos[i];
        if (p != (int)covered_tuples.size()) {
            int idx_new = covered_tuples.back();
            covered_tuples[p] = idx_new;
            tuples_idx_to_pos[idx_new] = p;
        }
        covered_tuples.pop_back();
    }

    for (int tpid : unique_id) {
        update_covered_testcases(tpid);
        int idx = covered_testcases[tpid].front();
        int pos = testcase_idx_to_pos[idx];
        unique_covered_tuples[pos].insert(tpid);

        unique_node[tpid] = unique_covered_tuples[pos].tail;
        sum_weight[pos] += weight[tpid];
    }

    tc_covered_tuples[tcid].clear();
    unique_covered_tuples[tcid].clear();
    sum_weight[tcid] = 0;

    testcases[tcid] = tc2;

    for (int tpid : covered_tuples) {
        const t_tuple &t = tuples_U[tpid];
        if (is_covered(tc2, t)) {
            covered_times[tpid]++;

            if (covered_times[tpid] == 2) {
                update_covered_testcases(tpid);
                int idx = covered_testcases[tpid].front();
                int pos = testcase_idx_to_pos[idx];

                IntrusiveListNode *_node = unique_node[tpid];
                unique_covered_tuples[pos].erase(_node);
                sum_weight[pos] -= weight[tpid];
            }

            tc_covered_tuples[tcid].insert(tpid);
            covered_testcases[tpid].insert(testcase_idx);
        }
    }

    break_pos.clear();
    int u_p = 0;
    for (int tpid : uncovered_tuples) {
        const t_tuple &t = tuples_U[tpid];

        if (is_covered(tc2, t)) {
            tuples_idx_to_pos[tpid] = covered_tuples.size();
            covered_tuples.emplace_back(tpid);
            covered_times[tpid] = 1;

            tc_covered_tuples[tcid].insert(tpid);
            unique_covered_tuples[tcid].insert(tpid);
            covered_testcases[tpid].insert(testcase_idx);

            unique_node[tpid] = unique_covered_tuples[tcid].tail;
            sum_weight[tcid] += weight[tpid];

            break_pos.emplace_back(u_p);
        }
        u_p++;
    }

    sort(break_pos.begin(), break_pos.end());
    break_num = break_pos.size();
    int uncovered_tuples_nums = uncovered_tuples.size();
    for (int i = break_num - 1; i >= 0; i--) {
        int p = break_pos[i];
        if (p != uncovered_tuples_nums)
            uncovered_tuples[p] = uncovered_tuples[uncovered_tuples_nums - 1];
        uncovered_tuples.pop_back();
        uncovered_tuples_nums--;
    }

    const auto &pos_in_cls = cnf->get_pos_in_cls();
    const auto &neg_in_cls = cnf->get_neg_in_cls();
    const int nvar = cnf->get_num_variables();
    vector<int> &cur_clauses_cov = clauses_cov[tcid];
    cur_clauses_cov = vector<int>(cnf->get_num_clauses(), 0);
    for (int i = 0; i < nvar; i++) {
        const vector<int> &var = (tc2[i] ? pos_in_cls[i + 1] : neg_in_cls[i + 1]);
        for (int cid : var) cur_clauses_cov[cid]++;
    }
}

void Optimizer::search(std::chrono::milliseconds time) {
    auto begin_time = std::chrono::high_resolution_clock::now();
    cur_step = 0;
    int last_succ_step = 0;
    if (arguments.configs.empty()) arguments.configs.emplace_back();
    config = arguments.configs.begin().base();
    while (!global_stop_flag.load(std::memory_order_acquire) &&
           std::chrono::high_resolution_clock::now() - begin_time < time) {
        if (uncovered_tuples.empty()) {
            best_testcases = testcases;
            logger::debugf(logger::Color::GREEN, "c current {}-wise CA size: {}, step #{}",
                           strength, last_strength_testcases.size() + testcases.size(), cur_step);
            if ((int)Random::bound(100) < config->cell_level_percent)
                static_cast<_Optimizer_HSCA *>(this)->remove_testcase_greedily();
            else
                static_cast<_Optimizer_Scalable *>(this)->remove_testcase_greedily();
            last_succ_step = 0;
            continue;
        }
        cur_step++;
        last_succ_step++;
        if (arguments.alt_length && last_succ_step >= arguments.alt_length) {
            testcases = best_testcases;
            restart();
            last_succ_step = 0;
            arguments.alt_length *= 1.8;
            continue;
        }

        if ((int)Random::bound(100) < config->cell_level_percent) {
            static_cast<_Optimizer_HSCA *>(this)->gradient_descent();
        } else {
            auto self = static_cast<_Optimizer_Scalable *>(this);
            int cyc = Random::bound(100);
            if (cyc < 1 || !self->random_greedy_step()) {
                self->random_step();
            }
        }
    }

    if (!uncovered_tuples.empty()) {
        testcases = best_testcases;
    }
}

void Optimizer::restart() {
    std::cout << "Optimizer::restart..." << std::endl;
    std::thread gc([this]() {
        IntrusiveListNode::recreate_pool();
        unique_covered_tuples.clear();
        tc_covered_tuples.clear();
        covered_testcases.clear();
    });

    auto last_strength_size = last_strength_testcases.size();
    // If 2-wise CA covered too much tuples (>99.5%), do not shrink last strength size.
    // Otherwise, the tuples_U size would doubled, making the optimization slow.
    double uncovered_percentage = static_cast<double>(tuples_U.size()) / saved_expandor->valid_nums;
    // shrink 10% testcases from last strength
    auto popout = uncovered_percentage > 0.005 ? last_strength_size / 10 : 0;
    std::vector<uint> order(testcases.size());
    std::iota(order.begin(), order.end(), 0);
    std::ranges::shuffle(order, Random::get());
    for (size_t i = 0, sz = std::min(last_strength_size - popout, testcases.size()); i < sz; ++i)
        std::swap(last_strength_testcases[i], testcases[order[i]]);
    testcases.reserve(testcases.size() + popout);
    for (size_t i = 0; i < popout; ++i) {
        testcases.push_back(std::move(last_strength_testcases.back()));
        last_strength_testcases.pop_back();
    }
    testcase_idx = testcases.size() - 1;

    // recalculate tuples_U
    std::cout << "restart old tuples_U size: " << tuples_U.size() << std::endl;
    const uint nvar = cnf->get_num_variables();
    const uint64_t COM = combinadic_nCr(nvar, strength - 1);
    const uint64_t M = COM << strength;
    std::vector<BitSet> last_strength_testcase_bs;
    last_strength_testcase_bs.reserve(last_strength_testcases.size());
    for (auto &testcase : last_strength_testcases) {
        auto &bs = last_strength_testcase_bs.emplace_back(nvar);
        for (auto [idx, var] : testcase | cpp23::views::enumerate)
            if (var) bs.set(idx);
    }
    std::vector<BitSet> covered(M, BitSet(nvar));
    const uint64_t every = COM / thread_num;
    std::vector<std::thread> threads;
    for (uint id = 0; id < thread_num; ++id) {
        threads.emplace_back([&, id] {
            uint64_t st = every * id + std::min<uint64_t>(id, COM % thread_num);
            uint64_t ed = every * (id + 1) + std::min<uint64_t>(id + 1, COM % thread_num);
            for (auto col = combinadic_decode(st, strength - 1, nvar); st < ed;
                 ++st, combinadic_next(col)) {
                for (const auto &bs : last_strength_testcase_bs) {
                    t_tuple tuple;
                    for (auto [i, c] : col | cpp23::views::enumerate)
                        tuple.v[i] = bs.get(c) ? (int)(c + 1) : -(int)(c + 1);
                    auto encode = saved_expandor->TupleToIndex(strength - 1, tuple);
                    covered[encode << 1 | 0].or_not(bs);
                    covered[encode << 1 | 1] |= bs;
                }
            }
        });
    }
    for (auto &t : threads) t.join();
    threads.clear();

    std::cout << "Calulate current tuples_U..." << std::endl;
    if (global_stop_flag.load(std::memory_order_acquire)) return;

    tuples_U.clear();
    const uint64_t COM2 = combinadic_nCr(nvar, strength);
    const uint64_t every2 = COM2 / thread_num;
    std::vector<TupleVector> thread_tuples_U(thread_num, TupleVector(strength));
    for (uint id = 0; id < thread_num; ++id) {
        threads.emplace_back([&, id] {
            uint64_t st = every2 * id + std::min<uint64_t>(id, COM2 % thread_num);
            uint64_t ed = every2 * (id + 1) + std::min<uint64_t>(id + 1, COM2 % thread_num);
            for (auto col = combinadic_decode(st, strength, nvar); st < ed;
                 ++st, combinadic_next(col)) {
                for (uint bits = 0; bits < (1u << strength); ++bits) {
                    t_tuple t;
                    for (auto [i, c] : col | cpp23::views::enumerate) {
                        t.v[i] = (bits & (1 << i)) ? (int)c + 1 : -(int)(c + 1);
                    }
                    uint64_t encode = saved_expandor->TupleToIndex(strength - 1, t);
                    bool last = t.v[strength - 1] > 0;
                    uint idx = abs(t.v[strength - 1]) - 1;
                    if (!saved_expandor->covered_now_strength_bitmap[encode << 1 | last].get(idx))
                        continue;                                        // invalid
                    if (covered[encode << 1 | last].get(idx)) continue;  // covered
                    thread_tuples_U[id].push_back(t);
                }
            }
        });
    }
    for (auto &t : threads) t.join();
    threads.clear();
    size_t tuples_U_size =
        std::accumulate(thread_tuples_U.begin(), thread_tuples_U.end(), size_t{0},
                        [](size_t acc, const auto &ts) { return acc + ts.size(); });
    tuples_U.reserve(tuples_U_size);
    for (auto &ts : thread_tuples_U) tuples_U.extends(ts);
    thread_tuples_U.clear();
    std::cout << "restart new tuples_U size: " << tuples_U.size() << std::endl;

    gc.join();
    rebind_pos_to_idx();
    if (++config == arguments.configs.end().base()) {
        config = arguments.configs.begin().base();
    }
    std::cout << "Optimizer::restart end" << std::endl;
}
