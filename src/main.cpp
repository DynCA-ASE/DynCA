#include <csignal>
#include "cli/args.hpp"
#include "expandor/expandor.hpp"
#include "generator/samplingca.hpp"
#include "io/casa.hpp"
#include "io/cnf.hpp"
#include "io/ctw.hpp"
#include "util/interrupt.hpp"
#include "util/logger.hpp"
#include "util/random.hpp"

void handle_signal(int signal) {
    switch (signal) {
    case SIGINT: logger::info(logger::Color::RED, "SIGINT received..."); break;
    case SIGTERM: logger::info(logger::Color::RED, "SIGTERM received..."); break;
    default: __builtin_unreachable();
    }
    if (global_stop_flag.load(std::memory_order_relaxed)) std::quick_exit(-1);
    global_stop_flag = true;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    auto args = parse_args(argc, argv);
    logger::console.verbose = args->verbose_level;
    if (args->has_log()) {
        logger::console.open_log_file(args->log_path);
    }
    auto conf = parse_conf(*args);
    if (args->has_coprocessor()) CNF::coprocessor_path = args->coprocessor_path;
    Random::seed(args->seed);

    CoveringArray ca;
    std::unique_ptr<CASA> casa;
    std::unique_ptr<CNF> cnf;
    std::unique_ptr<CTWModel> ctw;
    std::vector<std::string> ctw_var_mapping;
    if (args->has_cnf()) {
        cnf = CNF::parse(args->cnf_path);
        logger::infof(logger::Color::GREEN, "Parsed CNF with {} variables and {} clauses.",
                      cnf->get_num_variables(), cnf->get_num_clauses());
        cnf->reduce_cnf();
        logger::infof(logger::Color::GREEN, "Reduced CNF to {} variables and {} clauses.",
                      cnf->get_num_variables(), cnf->get_num_clauses());
    } else if (args->has_casa()) {
        casa = CASA::parse(args->casa_model_path, args->casa_constr_path);
        logger::infof(logger::Color::GREEN, "Parsed CASA with {} variables and {} clauses.",
                      casa->get_option_size(), casa->get_clause_size());
        if (!args->strength) args->strength = casa->get_strength();
        if (casa->get_strength() != (uint)args->strength) {
            logger::infof(logger::Color::RED,
                          "Warning: strength specified in command line and in casa model file do "
                          "NOT match ({} vs {})",
                          args->strength, casa->get_strength());
            casa->set_strength(args->strength);
        }
        cnf = casa->convert_to<CNF>();
        logger::infof(logger::Color::GREEN, "Converted CNF: {} variables and {} clauses.",
                      cnf->get_num_variables(), cnf->get_num_clauses());
    } else if (args->has_ctw()) {
        ctw = CTWModel::parse(args->ctw_path);
        logger::info(logger::Color::GREEN, "Parsing CTW file: success.");
        casa = ctw->convert_to<CASA>(ctw_var_mapping);
        casa->set_strength(args->strength);
        cnf = casa->convert_to<CNF>();
        logger::infof(logger::Color::GREEN, "Converted CNF: {} variables and {} clauses.",
                      cnf->get_num_variables(), cnf->get_num_clauses());
    }
    if (args->has_init_ca()) {
        std::ifstream finit(args->init_ca_path);
        finit >> ca;
        ca.set_strength(conf->generation.init_strength);
        if (ca.get_num_testcases() == 0) {
            logger::infof(logger::Color::RED, "Error: read 0 line from {}, is this the right path?",
                          args->init_ca_path);
            exit(-1);
        }
        logger::infof(logger::Color::GREEN, "Read init ca from {}, size: {}", args->init_ca_path,
                      ca.get_num_testcases());
    } else {
        ca = SamplingCA::run(cnf.get(), conf->generation, args->seed);
        logger::infof(logger::Color::GREEN, "Init CA for {}-wise with {} testcases.",
                      ca.get_strength(), ca.get_num_testcases());
    }
    global_check_stop();

    const char *nnf_path = args->has_nnf() ? args->nnf_path.c_str() : nullptr;
    if (conf->generation.init_strength != args->strength) {
        if (ca.is_casa()) ca = ca.casa2cnf(casa.get());
        expandor::Expandor expandor(cnf.get(), ca, conf->expansion, args->seed, nnf_path);
        for (int i = conf->generation.init_strength; i < args->strength; i++) {
            global_check_stop();
            expandor.Expand();
            expandor.Optimize(expandor::OptMethod::ALL, conf->optimization);
        }
        ca = CoveringArray(args->strength, ca.get_num_vars(), expandor.GetFinalCA());
        logger::infof(logger::Color::GREEN, "Generate CA for {}-wise with {} testcases.",
                      ca.get_strength(), ca.get_num_testcases());
    }

    if (casa && !ca.is_casa()) ca = ca.cnf2casa(casa.get());

    const auto output = [&] {
        if (!args->has_output()) {
            logger::infof(logger::Color::GREEN, "size: {}", ca.get_num_testcases());
            return;
        }
        logger::infof(logger::Color::GREEN, "save ca to {}", args->output_ca_path);
        logger::infof(logger::Color::GREEN, "size: {}", ca.get_num_testcases());
        std::ofstream fout(args->output_ca_path);
        if (ctw == nullptr) {
            for (uint i = 0; i < ca.get_num_vars(); ++i) {
                fout << (i ? "," : "") << "P" << i;
            }
            fout << '\n' << ca;
        } else {
            for (bool first = true; auto &name : ctw->dump_param_names()) {
                fout << (first ? "" : ",") << name;
                first = false;
            }
            fout << '\n';
            for (auto &row : ca.get_ca()) {
                for (bool first = true; auto &val : row) {
                    fout << (first ? "" : ",") << ctw_var_mapping[val];
                    first = false;
                }
                fout << '\n';
            }
        }
    };

#ifndef NDEBUG
    if (casa)
        ca.assert_array_valid(casa.get(), cnf.get());
    else
        ca.assert_array_valid(cnf.get());
#endif
    output();
    return 0;
}
