#include "io/cnf.hpp"
#include <fmt/core.h>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include "io/casa.hpp"
#include "io/ddnnf.hpp"
#include "util/cpp23views.h"
#include "util/logger.hpp"

void CNF::reset_cnf_path() {
    if (delete_cnf_file_ && !cnf_file_path_.empty()) {
        std::filesystem::remove(cnf_file_path_);
    }
    cnf_file_path_.clear();
    delete_cnf_file_ = false;
}

CNF::~CNF() {
    if (delete_cnf_file_ && !cnf_file_path_.empty()) {
        std::filesystem::remove(cnf_file_path_);
    }
}

CNF::CNF(const CNF &other)
    : cnf_file_path_(""),
      delete_cnf_file_(false),
      num_variables_(other.num_variables_),
      num_cared_(other.num_cared_),
      clauses_(other.clauses_),
      pos_in_cls_(other.pos_in_cls_),
      neg_in_cls_(other.neg_in_cls_),
      group_info_(other.group_info_) {}

CNF::CNF(CNF &&other) noexcept
    : cnf_file_path_(std::move(other.cnf_file_path_)),
      delete_cnf_file_(other.delete_cnf_file_),
      num_variables_(other.num_variables_),
      num_cared_(other.num_cared_),
      clauses_(std::move(other.clauses_)),
      pos_in_cls_(std::move(other.pos_in_cls_)),
      neg_in_cls_(std::move(other.neg_in_cls_)),
      group_info_(std::move(other.group_info_)) {
    other.cnf_file_path_ = "";
    other.delete_cnf_file_ = false;
}

CNF &CNF::operator=(const CNF &other) {
    if (this == &other) return *this;
    if (delete_cnf_file_ && !cnf_file_path_.empty()) {
        std::filesystem::remove(cnf_file_path_);
    }
    cnf_file_path_ = "";
    delete_cnf_file_ = false;
    num_variables_ = other.num_variables_;
    num_cared_ = other.num_cared_;
    clauses_ = other.clauses_;
    pos_in_cls_ = other.pos_in_cls_;
    neg_in_cls_ = other.neg_in_cls_;
    group_info_ = other.group_info_;
    return *this;
}

CNF &CNF::operator=(CNF &&other) noexcept {
    if (this == &other) return *this;
    if (delete_cnf_file_ && !cnf_file_path_.empty()) {
        std::filesystem::remove(cnf_file_path_);
    }
    cnf_file_path_ = std::move(other.cnf_file_path_);
    delete_cnf_file_ = other.delete_cnf_file_;
    num_variables_ = other.num_variables_;
    num_cared_ = other.num_cared_;
    clauses_ = std::move(other.clauses_);
    pos_in_cls_ = std::move(other.pos_in_cls_);
    neg_in_cls_ = std::move(other.neg_in_cls_);
    group_info_ = std::move(other.group_info_);
    other.cnf_file_path_ = "";
    other.delete_cnf_file_ = false;
    return *this;
}

auto CNF::parse(std::istream &cnf) -> std::unique_ptr<CNF> {
    std::unique_ptr<CNF> result(new CNF());
    unsigned num_clauses = 0;
    std::string line;
    while (std::getline(cnf, line)) {
        if (line.empty() || line[0] == 'c') continue;  // Skip comments and empty lines
        if (line[0] == 'p') {
            std::istringstream iss(line);
            std::string type;
            iss >> type >> type >> result->num_variables_ >> num_clauses;
            result->num_cared_ = result->num_variables_;
            result->clauses_.reserve(num_clauses);
            assert(type == "cnf" && "Expected CNF format");
            continue;
        }
        std::istringstream iss(line);
        std::vector<int> clause;
        int literal;
        while (iss >> literal) {
            if (literal == 0) break;  // End of clause
            clause.push_back(literal);
        }
        result->clauses_.push_back(std::move(clause));
    }
    result->calc_cnf_info();
    return result;
}

void CNF::print(std::ostream &cnf) const {
    cnf << "p cnf " << num_variables_ << " " << get_num_clauses() << "\n";
    for (const auto &clause : clauses_) {
        for (auto lit : clause) {
            cnf << lit << " ";
        }
        cnf << "0\n";  // End of clause
    }
}

static std::string create_tmpfile() {
    std::string result = "/tmp/cnfXXXXXX";
    int fd = mkstemp(result.data());
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary file for CNF");
    }
    close(fd);  // Close the file descriptor, we just need the name
    return result;
}

void CNF::reduce_cnf() {
    if (coprocessor_path.empty()) return;
    if (!std::filesystem::exists(coprocessor_path)) {
        logger::debugf(logger::Color::YELLOW, "Warning: coprocessor_path `{}` not exists!",
                       coprocessor_path);
        return;
    }
    auto &before_file = get_cnf_file_path();
    std::string after_file = create_tmpfile();
    std::string cmd =
        fmt::format("{} -enabled_cp3 -up -subsimp -no-bve -no-bce -no-dense -dimacs={} {} 2>&1",
                    coprocessor_path, after_file, before_file);
    logger::exec_with_cout(cmd.c_str());
    auto reduced_cnf = CNF::parse(after_file);
    reduced_cnf->delete_cnf_file_ = true;
    if (num_variables_ != reduced_cnf->num_variables_) {
        logger::infof(logger::Color::YELLOW,
                      "Warning: coprocessor: num variables mismatch ({} vs {})", num_variables_,
                      reduced_cnf->num_variables_);
        return;
    }
    *this = std::move(*reduced_cnf);
}

const std::string &CNF::get_cnf_file_path() {
    if (cnf_file_path_.empty()) {
        cnf_file_path_ = create_tmpfile();
        delete_cnf_file_ = true;
        print(cnf_file_path_);
    }
    return cnf_file_path_;
}

void CNF::calc_cnf_info() {
    pos_in_cls_.resize(num_variables_ + 1);
    neg_in_cls_.resize(num_variables_ + 1);
    for (auto [idx, cl] : clauses_ | cpp23::views::enumerate) {
        for (auto lit : cl) {
            if (lit < 0) {
                neg_in_cls_[-lit].push_back(idx);
            } else {
                pos_in_cls_[lit].push_back(idx);
            }
        }
    }
}

template <>
std::unique_ptr<CNF> CNF::convert_to<CNF>() const {
    return std::make_unique<CNF>(*this);
}

template <>
std::unique_ptr<CASA> CNF::convert_to<CASA>() const {
    std::unique_ptr<CASA> casa(new CASA());
    std::vector<int> group_of(num_variables_, -1);
    for (auto [idx, group] : group_info_ | cpp23::views::enumerate) {
        for (auto &x : group) {
            group_of[x] = idx;
        }
    }
    /// options
    std::vector<int> option_of_group(group_info_.size(), -1);
    std::vector<uint> option_of(num_variables_);
    uint num_options = 0;
    for (auto [var, my_group] : group_of | cpp23::views::enumerate) {
        if (my_group == -1) {
            option_of[var] = num_options++;
            casa->options_.push_back(2);
        } else if (auto opt = option_of_group[my_group]; opt != -1) {
            option_of[var] = opt;
        } else {
            option_of[var] = option_of_group[my_group] = num_options++;
            casa->options_.push_back(group_info_[my_group].size());
        }
        if (var == num_cared_ - 1) {
            casa->num_cared_ = num_options;
        }
    }
    /// symbols
    casa->cumulative_value_counts_.reserve(num_options);
    uint num_symbol = 0;
    for (auto opt : casa->options_) {
        num_symbol += opt;
        casa->cumulative_value_counts_.push_back(num_symbol);
    }
    casa->owing_options_.reserve(num_symbol);
    for (uint opt = 0; opt < num_options; ++opt) {
        for (uint i = 0; i < casa->options_[opt]; ++i) {
            casa->owing_options_.push_back(opt);
        }
    }
    /// casa --> cnf-lit
    casa->casa2cnf_.resize(num_symbol, 0);
    std::vector<uint> symbol_of(num_variables_);
    for (std::vector<uint> count(num_options);
         auto [var, opt] : option_of | cpp23::views::enumerate) {
        auto casa_symbol = casa->symbol_count(opt) == 2 ? casa->last_symbol(opt)
                                                        : casa->first_symbol(opt) + count[opt]++;
        symbol_of[var] = casa_symbol;
        casa->casa2cnf_[casa_symbol] = var + 1;
    }
    for (uint i = 0; i < num_symbol; ++i) {
        if (casa->casa2cnf_[i] == 0) {
            casa->casa2cnf_[i] = -casa->casa2cnf_[i + 1];
        }
    }
    /// constraints
    casa->clauses_.reserve(clauses_.size());
    for (auto &cl : clauses_) {
        Clause casa_cl;
        casa_cl.reserve(cl.size());
        for (auto &lit : cl) {
            casa_cl.push_back(Lit(symbol_of[std::abs(lit) - 1], lit < 0));
        }
        casa->clauses_.push_back(std::move(casa_cl));
    }
    casa->reduce_casa();
    return casa;
}

template <>
std::unique_ptr<DDNNF> CNF::convert_to<DDNNF>() const {
    if (!std::filesystem::exists(DDNNF::d4v2_path))
        throw std::runtime_error("d4v2 not found");
    if (!std::filesystem::exists(DDNNF::fastfmc_path))
        throw std::runtime_error("FastFMC not found");
    auto cnf_path = const_cast<CNF *>(this)->get_cnf_file_path();
    auto tmpfile = create_tmpfile();
    auto cmd1 = fmt::format("{} -i {} -m ddnnf-compiler --dump-ddnnf {}", DDNNF::d4v2_path, cnf_path,
                            tmpfile);
    int ret1 = logger::exec_with_cout(cmd1.c_str());
    if (ret1 != 0) throw std::runtime_error("Error while running d4v2");
    auto cmd2 = fmt::format("{0} {1} --save-ddnnf {1}", DDNNF::fastfmc_path, tmpfile);
    int ret2 = logger::exec_with_cout(cmd2.c_str());
    if (ret2 != 0) throw std::runtime_error("Error while running FastFMC");
    std::filesystem::remove(tmpfile);
    auto ddnf_filename = tmpfile + "-saved.nnf";
    auto result = DDNNF::parse(ddnf_filename);
    std::filesystem::remove(ddnf_filename);
    if ((uint)result->nvar > num_variables_)
        throw std::runtime_error("smooth d-DNNF file broken");
    return result;
}
