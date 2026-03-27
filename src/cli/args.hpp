#pragma once

#include <filesystem>
#include "expandor/expandor.hpp"
#include "generator/samplingca.hpp"
#include "util/logger.hpp"

struct Argument {
    int strength = 0, seed = 0;
    std::string cnf_path, nnf_path;
    std::string casa_model_path, casa_constr_path;
    std::string ctw_path;
    std::string output_ca_path;
    uint32_t max_time = static_cast<uint32_t>(-1);

    bool no_optimize = false;
    logger::Verbosity verbose_level = logger::Verbosity::NORMAL;
    std::string init_ca_path;
    std::string conf_path;
    std::string log_path;
    std::string coprocessor_path;

    void parse_path_extension(std::filesystem::path filename) {
        auto ext = filename.extension();
        if (ext == ".cnf") {
            if (!cnf_path.empty()) {
                std::cerr << "Warning: specified cnf file more than once.\n";
            }
            cnf_path = std::move(filename);
        } else if (ext == ".nnf") {
            if (!nnf_path.empty()) {
                std::cerr << "Warning: specified nnf file more than once.\n";
            }
            nnf_path = std::move(filename);
        } else if (ext == ".model") {
            if (!casa_model_path.empty()) {
                std::cerr << "Warning: specified casa model file more than once.\n";
            }
            casa_model_path = std::move(filename);
        } else if (ext == ".constr" || ext == ".constraint" || ext == ".constraints") {
            if (!casa_constr_path.empty()) {
                std::cerr << "Warning: specified casa constraint file more than once.\n";
            }
            casa_constr_path = std::move(filename);
        } else if (ext == ".ctw") {
            if (!ctw_path.empty()) {
                std::cerr << "Warning: specified ctw file more than once.\n";
            }
            ctw_path = std::move(filename);
        } else {
            std::cerr << "Error: unrecognized file extension: " << ext << std::endl;
            exit(1);
        }
    }

    bool has_cnf() const { return !cnf_path.empty(); }
    bool has_nnf() const { return !nnf_path.empty(); }
    bool has_casa() const { return !casa_model_path.empty() && !casa_constr_path.empty(); }
    bool has_ctw() const { return !ctw_path.empty(); }
    bool has_init_ca() const { return !init_ca_path.empty(); }
    bool has_conf() const { return !conf_path.empty(); }
    bool has_log() const { return !log_path.empty(); }
    bool has_output() const { return !output_ca_path.empty(); }
    bool has_coprocessor() const { return !coprocessor_path.empty(); }
};

std::unique_ptr<Argument> parse_args(int argc, char *argv[]) noexcept;

struct Configuration {
    //! Genration Phase
    SamplingCA::SimpArgument generation;

    //! Expansion Phase
    expandor::Argument expansion;

    //! Optimization Phase
    expandor::OptArgument optimization;
};

std::unique_ptr<Configuration> parse_conf(const Argument &args) noexcept;
