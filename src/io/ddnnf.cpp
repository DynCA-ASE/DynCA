#include "io/ddnnf.hpp"
#include <fmt/format.h>
#include <cassert>
#include <queue>
#include "util/cpp23views.h"

std::unique_ptr<DDNNF> DDNNF::parse(std::istream &is) {
    auto ddnf = std::unique_ptr<DDNNF>{new DDNNF()};
    std::string c;
    is >> c;
    assert(c == "nnf");
    int N;
    is >> N >> ddnf->nvar /* ignore one integer */ >> ddnf->nvar;
    ddnf->root = N - 1;
    ddnf->nodes.reserve(N);
    while (is >> c) {
        auto &node = ddnf->nodes.emplace_back();
        if (c == "L") {
            int l;
            is >> l;
            node.ty = LIT;
            node.tar.push_back(l);
            node.vars.emplace(var_of(l), -1);
        } else if (c == "A") {
            int ch_num;
            is >> ch_num;
            node.ty = AND;
            for (int i = 0, ch; i < ch_num; i++) is >> ch, node.tar.push_back(ch);
        } else if (c == "O") {
            int tmp;
            is >> tmp;
            assert(tmp == 0);
            int ch_num;
            is >> ch_num;
            node.ty = OR;
            for (int i = 0, ch; i < ch_num; i++) is >> ch, node.tar.push_back(ch);
        } else {
            throw std::runtime_error(fmt::format("unknown node type: {}", c));
        }
    }
    std::vector<int> degrees(N);
    std::queue<int> queue;
    for (auto [idx, node] : ddnf->nodes | cpp23::views::enumerate)
        if (node.ty == LIT)
            queue.push(idx);
        else {
            degrees[idx] = node.tar.size();
            for (int x : node.tar) ddnf->nodes[x].from.push_back(idx);
        }
    while (!queue.empty()) {
        int u = queue.front();
        queue.pop();
        for (auto p : ddnf->nodes[u].from) {
            for (auto [v, _] : ddnf->nodes[u].vars) ddnf->nodes[p].vars.emplace(v, u);
            if (--degrees[p] == 0) queue.push(p);
        }
    }
    return ddnf;
}

void DDNNF::print(std::ostream &os) const {
    os << fmt::format("nnf {} {} {}\n", nodes.size(), 0, nvar);
    for (auto &node : nodes) {
        switch (node.ty) {
        case LIT: os << "L " << node.tar.front() << '\n'; break;
        case AND:
            os << "A " << node.tar.size();
            for (auto t : node.tar) os << ' ' << t;
            os << '\n';
            break;
        case OR:
            os << "O 0 " << node.tar.size();
            for (auto t : node.tar) os << ' ' << t;
            os << '\n';
            break;
        default: __builtin_unreachable();
        }
    }
}
