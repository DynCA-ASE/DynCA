#include <fmt/core.h>
#include "covering_array.hpp"
#include "util/cpp23views.h"

int main(int argc, char *argv[]) {
    // convert cnf ca to casa ca
    if (argc != 4) {
        fmt::println("usage: {} <model> <cnf_ca> <casa_ca>", argv[0]);
        exit(1);
    }
    auto casa = CASA::parse(argv[1]);
    std::ifstream cnf_ca(argv[2]);
    CoveringArray ca(casa->get_strength(), 0);
    auto trim = std::views::drop_while(::isspace) | std::views::reverse |
                std::views::drop_while(::isspace) | std::views::reverse;
    for (std::string line; std::getline(cnf_ca, line);) {
        if (line.empty()) continue;
        ca.add_tuple(line | trim | std::views::split(' ') | std::views::transform([](auto &&s) {
                         std::string str(s.begin(), s.end());
                         return std::stoi(str);
                     }) |
                     cpp23::ranges::to<std::vector>());
    }
    ca = ca.cnf2casa(casa.get());
    std::ofstream casa_ca(argv[3]);
    casa_ca << ca;
    return 0;
}
