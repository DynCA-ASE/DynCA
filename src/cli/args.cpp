#include "cli/args.hpp"
#include <clipp.h>
#include <fmt/format.h>
#include <rfl.hpp>
#include <rfl/json.hpp>
#include "util/logger.hpp"

std::unique_ptr<Argument> parse_args(int argc, char *argv[]) noexcept {
    using namespace clipp;
    auto arg = std::make_unique<Argument>();
    std::vector<std::string> files;
    clipp::group cli;
    auto print_help = [&cli, pname = argv[0]](int code = 0) {
        auto dfmt = doc_formatting{}.first_column(5).doc_column(25).indent_size(0);
        std::cerr << fmt::format("Usage: {} [options] <input files>...\n\n", pname);
        std::cerr << "Options:\n" << documentation(cli, dfmt) << std::endl;
        exit(code);
    };
    // clang-format off
    cli = (
        (   option("-h", "--help") % "Print help" >> print_help,
            option("-t", "--strength") % "Strength [required]"
                & value("t", arg->strength),
            option("-s", "--seed") % "Random seed [default=1]"
                & value("s", arg->seed),
            option("-o", "--output") % "Output CA file path"
                & value("path", arg->output_ca_path),
            option("--cnf") % "CNF file path [--cnf <.cnf>]"
                & value("path", arg->cnf_path),
            option("--nnf") % "CNF file path [--nnf <.nnf>]"
                & value("path", arg->nnf_path),
            option("--casa") % "CASA file paths [--casa <.model> <.constr>]"
                & value("model", arg->casa_model_path)
                & value("constr", arg->casa_constr_path),
            option("--ctw") % "CTW file path [--ctw <.ctw>]"
                & value("path", arg->ctw_path),
            option("--time") % "Global time limit (in seconds)"
                & value("time-in-seconds", arg->max_time),
            any_other(files).label("input files")
                % "Input files with extension `.cnf`, `.model` or `.constraint` "
                  "will be automatically parsed to cnf or casa format. Use `--cnf` "
                  "or `--casa` instead if you want to use files with other extensions."
        ),
        "Advance Options:" %
        (   option("--init-ca") % "Init CA path (support cnf/casa format)"
                & value("path", arg->init_ca_path),
            option("--conf") % "Configuration file path"
                & value("path", arg->conf_path),
            (   option("--verbose") >> set(arg->verbose_level, logger::Verbosity::VERBOSE) |
                option("--quiet") >> set(arg->verbose_level, logger::Verbosity::QUIET)
            ) % "Verbosity control",
            option("--log") % "Log file path (same effect as redirecting stdout)"
                & value("path", arg->log_path),
            option("--coprocessor") % "coprocessor path"
                & value("path", arg->coprocessor_path),
            option("--no-optimize") % "Do not campact ca after generated"
                >> set(arg->no_optimize)
        )
    ).repeatable(false);
    // clang-format on
    if (auto result = parse(argc, argv, cli)) {
        for (auto &file : files) {
            arg->parse_path_extension(file);
        }
        if (int cnt = arg->has_cnf() + arg->has_casa() + arg->has_ctw(); cnt != 1) {
            if (cnt == 0)
                std::cerr << "Error: no input file\n";
            else
                std::cerr << "Error: specified multiple input formats\n";
            exit(1);
        }
        if (!arg->strength && !arg->has_casa()) {
            arg->strength = 3;
        } else if (arg->strength && (arg->strength < 2 || arg->strength > 6)) {
            std::cerr << "Error: strength must between [2, 6]\n";
            exit(1);
        }
        if (arg->seed == 0) {
            arg->seed = time(0) & 0x7fff'ffff;
        }
        if (arg->verbose_level == logger::Verbosity::QUIET && !arg->has_log() &&
            !arg->has_output()) {
            std::cerr << "Warning: verbose_level is set to quiet without neither log file nor "
                         "output file set. The program will run without outputing anything!"
                      << std::endl;
        }
        return arg;
    } else {
        if (result.begin() == result.end()) {
            print_help(1);
        }
        std::cerr << "Error when parsing command line arguments.\n"
                  << "Use `--help` to show man page." << std::endl;
        exit(1);
    }
}

std::unique_ptr<Configuration> parse_conf(const Argument &args) noexcept {
    auto conf = std::make_unique<Configuration>();
    std::ifstream fin(args.has_conf() ? args.conf_path : "default.conf.json");
    auto result = rfl::json::read<Configuration>(fin);
    *conf = result.value();
#ifndef NDEBUG
    logger::debugf("Configuration: {}", rfl::json::write(result));
#endif
    return conf;
}
