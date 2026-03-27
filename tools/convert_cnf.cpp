#include <fmt/core.h>
#include "io/casa.hpp"
#include "io/cnf.hpp"

int main(int argc, char *argv[]) {
    if (argc != 4 && argc != 5) {
        fmt::println("usage: {} <model> <constraints> <cnf> [group]", argv[0]);
        exit(1);
    }
    auto casa = CASA::parse(argv[1], argv[2]);
    auto cnf = casa->convert_to<CNF>();
    int top = cnf->get_num_variables();
    cnf->add_clause({top, -top});
    cnf->print(argv[3]);
    if (argc == 5) {
        std::ofstream group(argv[4]);
        auto &info = cnf->get_group_info();
        group << info.size() << '\n';
        for (auto &g : info) {
            group << g.size() << '\n';
            for (auto &v : g) {
                group << v << ' ';
            }
            group << '\n';
        }
    }
    return 0;
}
