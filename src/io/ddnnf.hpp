#pragma once

#include "io/base.hpp"

#include <cassert>
#include <span>
#include <unordered_map>

class DDNNF : public InputBase {
    friend class CNF;
    DDNNF() = default;

 public:
    enum NodeType { AND, OR, LIT };
    static inline char to_char(NodeType ty) {
        constexpr char chs[] = {'A', 'O', 'L'};
        return chs[static_cast<int>(ty)];
    }

 private:
    struct Node {
        std::unordered_map<int, int> vars;
        std::vector<int> tar;
        std::vector<int> from;
        NodeType ty;
        int var_at(int v) const {
            if (auto it = vars.find(v); it != vars.end()) {
                return it->second;
            } else {
                return -1;
            }
        }
    };
    static inline const std::vector<int> empty_tar{};

    bool check_valid_impl(int node_id, std::span<const int> tuple) const {
        auto &node = nodes[node_id];
        switch (node.ty) {
        case LIT:
            assert(tuple.size() == 1);
            if (tuple.front() == node.tar.front()) return true;
            if (tuple.front() == -node.tar.front()) return false;
            return true;
        case OR:
            for (auto t : node.tar) {
                if (check_valid_impl(t, tuple)) return true;
            }
            return false;
        case AND: {
            std::unordered_map<int, std::vector<int>> children;
            for (auto lit : tuple) {
                int child = node.var_at(var_of(lit));
                if (child == -1) continue;
                children[child].push_back(lit);
            }
            for (auto &[c, t] : children) {
                if (!check_valid_impl(c, t)) return false;
            }
            return true;
        }
        default: __builtin_unreachable();
        }
    }

    template <size_t N>
    bool check_valid_impl(int node_id, const std::array<int, N> &tuple) const {
        if (node_id == -1) return true;
        auto &tar = nodes[node_id].tar;
        switch (nodes[node_id].ty) {
        case LIT: {
            if (tuple[0] == tar.front()) return true;
            if (tuple[0] == -tar.front()) return false;
            return true;
        }
        case OR: {
            for (auto t : tar) {
                if (check_valid_impl(t, tuple)) return true;
            }
            return false;
        }
        case AND: {
            auto &node = nodes[node_id];
            if constexpr (N == 1) {
                int child = node.var_at(var_of(tuple[0]));
                return check_valid_impl(child, tuple);
            } else if constexpr (N == 2) {
                int c1 = node.var_at(var_of(tuple[0])),
                    c2 = node.var_at(var_of(tuple[1]));
                if (c1 == c2) {
                    return check_valid_impl(c1, tuple);
                } else {
                    std::array<int, 1> t1{tuple[0]};
                    std::array<int, 1> t2{tuple[1]};
                    return check_valid_impl(c1, t1) && check_valid_impl(c2, t2);
                }
            } else if constexpr (N == 3) {
                auto [v1, v2, v3] = tuple;
                int c1 = node.var_at(var_of(v1)),
                    c2 = node.var_at(var_of(v2)),
                    c3 = node.var_at(var_of(v3));
                if (c1 == c2 && c2 == c3) {
                    return check_valid_impl(c1, tuple);
                } else if (c1 == c2) {
                    std::array<int, 2> t12{v1, v2};
                    std::array<int, 1> t3{v3};
                    return check_valid_impl(c1, t12) && check_valid_impl(c3, t3);
                } else if (c1 == c3) {
                    std::array<int, 2> t13{v1, v3};
                    std::array<int, 1> t2{v2};
                    return check_valid_impl(c1, t13) && check_valid_impl(c2, t2);
                } else if (c2 == c3) {
                    std::array<int, 1> t1{v1};
                    std::array<int, 2> t23{v2, v3};
                    return check_valid_impl(c1, t1) && check_valid_impl(c2, t23);
                } else {
                    std::array<int, 1> t1{v1}, t2{v2}, t3{v3};
                    return check_valid_impl(c1, t1) && check_valid_impl(c2, t2) &&
                           check_valid_impl(c3, t3);
                }
            } else {
                static_assert(N >= 1 && N <= 3);
            }
            break;
        }
        default: __builtin_unreachable();
        }
    }

 public:
    static inline std::string_view d4v2_path = "bin/d4v2";
    static inline std::string_view fastfmc_path = "bin/FastFMC";

    static uint var_of(int lit) { return std::abs(lit) - 1; }
    static int mk_lit(uint var, bool neg) { return neg ? -(int)(var + 1) : (int)(var + 1); }

    static auto parse(std::istream &ddnf) -> std::unique_ptr<DDNNF>;
    static auto parse(const std::string &ddnf_file) -> std::unique_ptr<DDNNF> {
        std::ifstream ddnf(ddnf_file);
        if (!ddnf.is_open()) {
            throw std::runtime_error("Failed to open smooth d-DNNF file.");
        }
        auto result = parse(ddnf);
        return result;
    }
    void print(std::ostream &ddnf) const;
    void print(const std::string &ddnf_file) const {
        std::ofstream ddnf(ddnf_file);
        if (!ddnf.is_open()) {
            throw std::runtime_error("Failed to open smooth d-DNNF file for writing.");
        }
        print(ddnf);
    }

    NodeType get_nodetype(int node_id) const { return nodes[node_id].ty; }
    const std::vector<int> &get_targets(int node_id) const {
        return nodes[node_id].ty == LIT ? empty_tar : nodes[node_id].tar;
    }
    int get_literal(int node_id) const {
        assert(nodes[node_id].ty == LIT);
        return nodes[node_id].tar.front();
    }
    const auto &get_vars(int node_id) const { return nodes[node_id].vars; }

    bool check_valid(std::span<const int> tuple) const { return check_valid_impl(root, tuple); }
    template <size_t N>
        requires(N >= 1 && N <= 3)
    bool check_valid(const std::array<int, N> &tuple) const {
        return check_valid_impl(root, tuple);
    }

 private:
    int nvar, root;
    std::vector<Node> nodes;
};
