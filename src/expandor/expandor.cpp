#include "expandor/expandor.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <vector>
#include "expandor/optimizer.hpp"
#include "expandor/solvers.hpp"
#include "util/interrupt.hpp"
#include "util/logger.hpp"
#include "util/random.hpp"

using namespace expandor;
using std::pair;
using std::string;

Expandor::Expandor(const CNF *cnf, const CoveringArray &init_ca, const Argument &arg, int seed,
                   const char *ddnnf_path)
    : cnf(cnf) {
#ifndef NDEBUG
    need_all_valid_tuples = true;
#endif

    // settings
    strength = init_ca.get_strength();
    uncovered_tuples.set_strength(strength);
    valid_tuples.set_strength(strength);
    t_clauses.set_strength(strength);

    candidate_set_size = arg.candidate_set_size;

    // group
    const int nvar = cnf->get_num_variables();
    group_id.resize(nvar, -1);

    // read group_file
    group_num = cnf->get_group_info().size();
    member.resize(group_num);
    group_flag.resize(group_num);

    for (int i = 0; i < group_num; i++) {
        auto &group = cnf->get_group_info()[i];
        for (auto x : group) {
            x--;
            group_id[x] = i;
            member[i].emplace_back(x);
        }
    }

    new_testcase = init_ca.get_ca();
    reinit_CA();

    count_each_var_uncovered[0].resize(nvar + 2, 0);
    count_each_var_uncovered[1].resize(nvar + 2, 0);

    long long M = combinadic_nCr(nvar, strength - 1) << strength;
    covered_now_strength_bitmap.resize(M, BitSet(nvar));
    if (strength > 2) {
        for (int i = 0; i < group_num; i++) group_flag[i] = 0;
        for (const BitSet &tc : CA) set_covered_now_strength_bitmap(tc, 1, -1, 0);
    } else {
        for (auto testcase : CA)
            for (int i = 0; i < nvar; i++) {
                if (testcase.get(i)) {
                    covered_now_strength_bitmap[(nvar + i) << 1 | 1] |= testcase;
                    covered_now_strength_bitmap[(nvar + i) << 1].or_not(testcase);
                } else {
                    covered_now_strength_bitmap[i << 1 | 1] |= testcase;
                    covered_now_strength_bitmap[i << 1].or_not(testcase);
                }
            }
    }

    switch (arg.use_validator) {
    case ValidatorEnum::ddnnf:
        if (ddnnf_path)
            validator = std::make_unique<DDNNF_Validator>(ddnnf_path, strength);
        else
            validator = std::make_unique<DDNNF_Validator>(cnf, strength);
        break;
    case ValidatorEnum::minisat:
        validator = std::make_unique<Minisat_Validator>(cnf, strength);
        break;
    case ValidatorEnum::cadical:
        validator = std::make_unique<Cadical_Validator>(cnf, strength);
        break;
    }
    switch (arg.use_solver) {
    case SolverEnum::minisat:
        cdcl_sampler = std::make_unique<Minisat_SamplingSolver>(cnf, seed);
        break;
    case SolverEnum::cadical:
        cdcl_sampler = std::make_unique<Cadical_SamplingSolver>(cnf, seed);
        break;
    }
}

vector<vector<int>> Expandor::GetFinalCA() {
    vector<vector<int>> final_tc;
    final_tc.reserve(old_testcase.size() + new_testcase.size());
    for (auto tc : old_testcase) final_tc.emplace_back(tc);
    for (auto tc : new_testcase) final_tc.emplace_back(tc);
    return final_tc;
}

void Expandor::reinit_CA() {
    CA.clear();
    BitSet tmp(cnf->get_num_cared());
    for (const auto &row : old_testcase) {
        int p = 0;
        for (auto c : row) {
            tmp[p] = c;
            p++;
        }
        CA.push_back(tmp);
    }
    for (const auto &row : new_testcase) {
        int p = 0;
        for (auto c : row) {
            tmp[p] = c;
            p++;
        }
        CA.push_back(tmp);
    }
}

void Expandor::set_covered_now_strength_bitmap(const BitSet &tc, int idx, int last, t_tuple tuple) {
    if (idx == strength) {
        uint64_t offset = TupleToIndex(strength - 1, tuple);
        covered_now_strength_bitmap[offset << 1 | 1] |= tc;
        covered_now_strength_bitmap[offset << 1].or_not(tc);
        return;
    }
    const int nvar = cnf->get_num_variables();
    for (int i = last + 1; i < nvar - (strength - idx) + 1; i++) {
        int gid = group_id[i];
        if (gid == -1 || (!group_flag[gid])) {
            if (~gid) {
                if (!tc.get(i)) continue;
                group_flag[gid] = 1;
            }
            tuple.v[idx - 1] = tc.get(i) ? (i + 1) : -(i + 1);
            set_covered_now_strength_bitmap(tc, idx + 1, i, tuple);

            if (~gid) group_flag[gid] = 0;
        }
    }
}
bool Expandor::check_part_invalid(t_tuple tp) {
    for (int ban = 0; ban < strength; ban++) {
        t_tuple tmp(strength - 1);
        for (int i = 0, j = 0; i < strength; i++) {
            if (i == ban) continue;

            j++;
            if (j == strength - 1) {
                int p = abs(tp.v[i]) - 1, v = tp.v[i] > 0;
                uint64_t state = TupleToIndex(strength - 2, tmp);
                if (!covered_last_strength_bitmap[state << 1 | v].get(p)) return 1;
            } else
                tmp.v[j - 1] = tp.v[i];
        }
    }
    return 0;
}

bool Expandor::check_clauses_invalid(t_tuple tp) {
    for (t_tuple t : t_clauses) {
        bool flag = true;
        for (int i = 0; i < strength; i++)
            if (t.v[i] != -tp.v[i]) {
                flag = false;
                break;
            }
        if (flag) return 1;
    }
    return 0;
}

void Expandor::get_remaining_valid_tuples(int value, int idx, int last, int _ed, t_tuple tuple) {
    const int nvar = cnf->get_num_variables();
    if (idx == strength) {
        for (int i = strength - 2, j = value; i >= 0; i--, j >>= 1)
            if (!(j & 1)) tuple.v[i] = -tuple.v[i];

        uint64_t offset = TupleToIndex(strength - 1, tuple);
        for (int i = last + 1; i < nvar; i++) {
            int gid = group_id[i];
            if ((~gid) && group_flag[gid]) continue;

            for (int v = gid == -1 ? 0 : 1; v <= 1; v++) {
                tuple.v[idx - 1] = v ? (i + 1) : -(i + 1);

                if (covered_now_strength_bitmap[offset << 1 | v].get(i)) {
                    valid_nums++;
                    if (need_all_valid_tuples) valid_tuples.push_back(tuple);
                } else {
                    if (use_invalid_expand &&
                        (check_part_invalid(tuple) || check_clauses_invalid(tuple)))
                        /* invalid */;
                    else if (validator->check_tuple(tuple)) {
                        t_tuple tep(strength);
                        for (int i = 0; i < strength; i++) tep.v[i] = tuple.v[i];
                        uncovered_tuples.push_back(tep);
                        valid_nums++;
                        if (need_all_valid_tuples) valid_tuples.push_back(tep);
                    }
                }
            }
        }
        return;
    }

    int ed = std::min(nvar - (strength - idx) + 1, _ed);
    for (int i = last + 1; i < ed; i++) {
        int gid = group_id[i];
        if (gid == -1 || (!group_flag[gid])) {
            if (~gid) {
                if (!(value & (1 << (strength - idx - 1)))) continue;
                group_flag[gid] = 1;
            }
            tuple.v[idx - 1] = i + 1;
            get_remaining_valid_tuples(value, idx + 1, i, nvar, tuple);

            if (~gid) group_flag[gid] = 0;
        }
    }
}

void Expandor::Expand() {
    std::cout << "start expand: " << strength << "\n";
    const int nvar = cnf->get_num_variables();
    strength++;
    if (auto ddnf_v = dynamic_cast<DDNNF_Validator *>(validator.get())) {
        if (strength == 3)
            validator = std::make_unique<DDNNF_Validator_3wise>(std::move(*ddnf_v));
        else
            ddnf_v->strength = strength;
    } else if (auto ddnf_3_v = dynamic_cast<DDNNF_Validator_3wise *>(validator.get())) {
        if (strength != 3)
            validator = std::make_unique<DDNNF_Validator>(std::move(*ddnf_3_v), strength);
    } else if (auto minisat_v = dynamic_cast<Minisat_Validator *>(validator.get())) {
        minisat_v->strength = strength;
    } else if (auto cadical_v = dynamic_cast<Cadical_Validator *>(validator.get())) {
        cadical_v->strength = strength;
    } else {
        throw std::runtime_error("Error: unknown validator type.");
    }

    t_clauses.clear();
    t_clauses.set_strength(strength);
    for (const vector<int> &c : cnf->get_clauses())
        if ((int)c.size() == strength) {
            t_tuple tep(c);
            t_clauses.push_back(tep);
        }

    old_testcase.reserve(old_testcase.size() + new_testcase.size());
    for (vector<int> &tc : new_testcase) old_testcase.emplace_back(std::move(tc));
    new_testcase.clear();
    reinit_CA();

    auto num_combination_all_possible_ = combinadic_nCr(nvar, strength) << strength;
    std::cout << "num_combination_all_possible_ = " << num_combination_all_possible_ << std::endl;

    long long M = combinadic_nCr(nvar, strength - 1) << strength;
    covered_last_strength_bitmap =
        std::exchange(covered_now_strength_bitmap, vector(M, BitSet(nvar)));

    for (int i = 0; i < group_num; i++) group_flag[i] = 0;

    for (const auto &tc : CA) set_covered_now_strength_bitmap(tc, 1, -1, 0);

    std::cout << "begin to get remaining valid tuples" << std::endl;
    uncovered_tuples.clear();
    valid_tuples.clear();
    valid_nums = 0;
    uncovered_tuples.set_strength(strength);
    valid_tuples.set_strength(strength);
    const int mx_value = 1 << (strength - 1);
    for (int value = 0; value < mx_value; ++value) {
        get_remaining_valid_tuples(value, 1, -1, nvar, t_tuple(strength));
    }

    for (int i = 0; i <= nvar; i++) {
        count_each_var_uncovered[0][i] = 0;
        count_each_var_uncovered[1][i] = 0;
    }
    for (const t_tuple &t : uncovered_tuples) {
        for (int k = 0; k < strength; k++) {
            int j = abs(t.v[k]) - 1, vj = t.v[k] > 0;
            count_each_var_uncovered[vj][j]++;
        }
    }

    tuples_U = uncovered_tuples;  // backup

    uncovered_nums = uncovered_tuples.size();
    size_t covered_nums = valid_nums - uncovered_nums;
    size_t invalid_nums = num_combination_all_possible_ - valid_nums;
    std::cout << "covered valid " << strength << "-wise tuple nums: " << covered_nums << "\n";
    std::cout << "uncovered valid " << strength << "-wise tuple nums: " << uncovered_nums << "\n";
    std::cout << "all valid " << strength << "-wise tuple nums: " << valid_nums << "\n";
    std::cout << "invalid " << strength << "-wise tuple nums: " << invalid_nums << "\n";
    std::cout << "all tuples: " << num_combination_all_possible_ << std::endl;

    // Caching Candidate Set
    if (use_cache) {
        candidate_testcase_set_.resize(2 * candidate_set_size);
        gain.resize(2 * candidate_set_size);
        vector<pair<int, int>> prob;
        prob.reserve(nvar);
        for (int i = 0; i < nvar; i++) {
            int v1 = count_each_var_uncovered[1][i];
            int v2 = count_each_var_uncovered[0][i];
            prob.emplace_back(v1, v2);
        }

        for (int i = 0; i < candidate_set_size; i++) {
            cdcl_sampler->set_prob(prob);
            cdcl_sampler->get_solution(candidate_testcase_set_[i]);
        }
    } else {
        gain.resize(candidate_set_size);
        candidate_testcase_set_.resize(candidate_set_size);
    }

    // Generate CA
    GenerateCoveringArray();

    logger::infof(logger::Color::GREEN, "Expand CA to {} wise. size: {} -> {}.", strength,
                  old_testcase.size(), old_testcase.size() + new_testcase.size());
}

void Expandor::Optimize(OptMethod opt_method, const OptArgument &arg) {
    using namespace std::chrono_literals;
    if ((opt_method & (1 << strength)) == OptMethod::NONE) return;
    if (new_testcase.empty()) {
        logger::infof("The newly expanded CA is empty. No optimization.");
        return;
    }
    auto original_size = old_testcase.size() + new_testcase.size();
    std::chrono::milliseconds time(arg.opt_time_ms);
    if (global_stop_flag.load(std::memory_order_acquire)) return;
    auto start_time = std::chrono::high_resolution_clock::now();
    Optimizer optimizer(*this, arg);
    auto init_used = std::chrono::high_resolution_clock::now() - start_time;
    auto used_ms = std::chrono::duration_cast<std::chrono::milliseconds>(init_used);
    optimizer.search(time - used_ms);
    optimizer.swap_testcase_out(*this);
    logger::infof(logger::Color::GREEN, "Optimize newly expanded {}-wise CA. size: {} -> {}.",
                  strength, original_size, old_testcase.size() + new_testcase.size());
}

void Expandor::GenerateCandidateTestcaseSet() {
    const int nvar = cnf->get_num_variables();

    vector<pair<int, int>> prob;
    prob.reserve(nvar);

    for (int i = 0; i < nvar; i++) {
        int v1 = count_each_var_uncovered[1][i];
        int v2 = count_each_var_uncovered[0][i];
        prob.emplace_back(v1, v2);
    }

    if (use_cache) {
        for (int i = 0; i < candidate_set_size; i++) {
            cdcl_sampler->set_prob(prob);
            cdcl_sampler->get_solution(candidate_testcase_set_[candidate_set_size + i]);
        }
    } else {
        for (int i = 0; i < candidate_set_size; i++) {
            cdcl_sampler->set_prob(prob);
            cdcl_sampler->get_solution(candidate_testcase_set_[i]);
        }
    }
}

int Expandor::get_gain(const vector<int> &testcase) {
    int score = 0;
    for (const t_tuple &t : uncovered_tuples)
        if (is_covered(testcase, t)) score++;
    return score;
}

int Expandor::SelectTestcaseFromCandidateSetByTupleNum() {
    if (use_cache) {
        for (int i = 0; i < candidate_set_size; i++) {
            int k = i + candidate_set_size;
            gain[k] = std::make_pair(get_gain(candidate_testcase_set_[k]), k);
        }

        sort(gain.begin(), gain.end());

        if (gain[2 * candidate_set_size - 1].first <= 0) return -1;

        vector<vector<int>> tmp;
        tmp.resize(candidate_set_size);
        for (int i = candidate_set_size; i < 2 * candidate_set_size; i++)
            tmp[i - candidate_set_size] = candidate_testcase_set_[gain[i].second];
        for (int i = 0; i < candidate_set_size; i++) candidate_testcase_set_[i] = tmp[i];
        return candidate_set_size - 1;
    } else {
        for (int i = 0; i < candidate_set_size; i++) {
            int k = i;
            gain[k] = std::make_pair(get_gain(candidate_testcase_set_[k]), k);
        }

        int mx = gain[0].first, mxi = gain[0].second;
        for (int i = 1; i < candidate_set_size; i++)
            if (gain[i].first > mx) mx = gain[i].first, mxi = gain[i].second;
        return mx > 0 ? mxi : -1;
    }
}

int Expandor::GenerateTestcase() {
    GenerateCandidateTestcaseSet();
    return SelectTestcaseFromCandidateSetByTupleNum();
}

void Expandor::Update_t_TupleInfo(const vector<int> &testcase, bool setmap) {
    TupleVector tep(strength);
    for (const t_tuple &t : uncovered_tuples) {
        if (!is_covered(testcase, t)) {
            tep.push_back(t);
            continue;
        }

        for (int i = 0; i < strength; i++) {
            int pi = abs(t.v[i]) - 1, vi = t.v[i] > 0;
            count_each_var_uncovered[vi][pi]--;
        }

        uint64_t state = TupleToIndex(strength - 1, t);
        int p = abs(t.v[strength - 1]) - 1, v = t.v[strength - 1] > 0;
        if (setmap) covered_now_strength_bitmap[state << 1 | v].set(p);
    }
    uncovered_tuples = std::move(tep);
    uncovered_nums = uncovered_tuples.size();
}

void Expandor::GenerateCoveringArray() {
    const int nvar = cnf->get_num_variables();

    int old_size = old_testcase.size();
    for (int num_generated_testcase_ = 1;; num_generated_testcase_++) {
        int idx = GenerateTestcase();
        if (idx == -1) break;

        new_testcase.emplace_back(candidate_testcase_set_[idx]);
        Update_t_TupleInfo(candidate_testcase_set_[idx], true);

        std::cout << "\033[;32mc current size: " << old_size + num_generated_testcase_
                  << ", current uncovered: " << uncovered_nums << " \033[0m" << std::endl;
    }
    ReplenishTestCase();

    // Update CA
    for (const vector<int> &tc : new_testcase) {
        BitSet tep(nvar);
        for (int i = 0; i < nvar; i++)
            if (tc[i]) tep.set(i);
        CA.emplace_back(tep);
    }
}

void adaptive_adjustment() {}

void Expandor::Update_t_TupleInfo(int st, const vector<int> &testcase, const vector<int> &sidx) {
    int sz = uncovered_tuples.size();
    for (int p = st; p < sz; p++) {
        t_tuple t = uncovered_tuples[sidx[p]];

        if (!is_covered(testcase, t)) continue;

        uint64_t state = TupleToIndex(strength - 1, t);
        int pl = abs(t.v[strength - 1]) - 1, vl = t.v[strength - 1] > 0;
        if (covered_now_strength_bitmap[state << 1 | vl].get(pl)) continue;

        uncovered_nums--;
        covered_now_strength_bitmap[state << 1 | vl].set(pl);
        for (int i = 0; i < strength; i++) {
            int pi = abs(t.v[i]) - 1, vi = t.v[i] > 0;
            count_each_var_uncovered[vi][pi]--;
        }
    }
}

void Expandor::ReplenishTestCase() {
    const int nvar = cnf->get_num_variables();
    std::cout << "uncovered_nums: " << uncovered_nums << "\n";
    std::cout << "add new testcase: " << new_testcase.size() << "\n";

    vector<int> sidx(uncovered_nums, 0);
    std::iota(sidx.begin(), sidx.end(), 0);
    std::shuffle(sidx.begin(), sidx.end(), Random::get());

    int old_size = old_testcase.size(), sz = uncovered_nums;
    for (int p = 0; p < sz; p++) {
        t_tuple t = uncovered_tuples[sidx[p]];

        if (uncovered_nums <= 0) break;

        uint64_t state = TupleToIndex(strength - 1, t);
        int pl = abs(t.v[strength - 1]) - 1, vl = t.v[strength - 1] > 0;
        if (covered_now_strength_bitmap[state << 1 | vl].get(pl)) continue;

        for (int i = 0; i < strength; i++) {
            cdcl_sampler->assume(t.v[i]);
        }

        bool use_random = dynamic_cast<Minisat_SamplingSolver *>(cdcl_sampler.get()) != nullptr;
        vector<pair<int, int>> prob;
        if (use_random) {
            prob.resize(nvar, {1, 1});
        } else {
            prob.reserve(nvar);
            for (int i = 0; i < nvar; i++) {
                int v1 = count_each_var_uncovered[1][i];
                int v2 = count_each_var_uncovered[0][i];
                prob.emplace_back(v1, v2);
            }
        }
        cdcl_sampler->set_prob(prob);

        vector<int> tep(nvar, 0);
        cdcl_sampler->get_solution(tep);

        new_testcase.emplace_back(tep);
        Update_t_TupleInfo(p, tep, sidx);

        std::cout << "\033[;32mc current size: " << old_size + new_testcase.size()
                  << ", current uncovered: " << uncovered_nums << " \033[0m" << std::endl;
    }
    std::cout << "uncovered_nums: " << uncovered_nums << "\n";
}
