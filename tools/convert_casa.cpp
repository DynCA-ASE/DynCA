#include <fmt/core.h>
#include "io/casa.hpp"
#include "io/cnf.hpp"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fmt::println("usage: {} <cnf> <model> <constraints>", argv[0]);
        exit(1);
    }
    auto cnf = CNF::parse(argv[1]);
    auto casa = cnf->convert_to<CASA>();
    casa->set_strength(3);
    casa->print(argv[2], argv[3], false);
    return 0;
}
