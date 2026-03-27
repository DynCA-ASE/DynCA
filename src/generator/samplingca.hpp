#pragma once

#include "generator/cdclcasampler.h"

#include "covering_array.hpp"
#include "io/cnf.hpp"

class SamplingCA {
 public:
    struct SimpArgument {
        int init_strength = 2;
        int candidate_set_size = 100;
    };

    struct Argument {
        bool use_pure_cdcl = false;
        bool use_pure_sls = false;
        bool use_cdcl_vsids = false;
        bool disable_var_shuffle = false;
        bool use_weighted_sampling = true;
        int init_strength = 2;
        int candidate_set_size = 100;
        int context_aware = 2;
        int64_t upper_limit = 0;
    };

    /// run samplingca and return a ca format in cnf
    static CoveringArray run(CNF *cnf, const Argument &arg, int seed) {
        samplingca::CDCLCASampler ca_sampler{cnf->get_cnf_file_path(), seed};
        ca_sampler.SetTWise(arg.init_strength);
        ca_sampler.SetCandidateSetSize(arg.candidate_set_size);
        ca_sampler.SetWeightedSamplingMethod(arg.use_weighted_sampling);
        if (arg.context_aware != 2) ca_sampler.SetContextAwareMethod(arg.context_aware);
        if (arg.upper_limit != 0) ca_sampler.SetUpperLimit(arg.upper_limit);
        if (arg.use_pure_cdcl) ca_sampler.SetPureCDCL();
        if (arg.use_pure_sls) ca_sampler.SetPureSLS();
        if (arg.disable_var_shuffle) ca_sampler.DisableVarShuffling();
        if (arg.use_cdcl_vsids) ca_sampler.EnableVSIDSForCDCL();
        ca_sampler.GenerateCoveringArray();
        return CoveringArray(arg.init_strength, cnf->get_num_variables(),
                             std::move(ca_sampler.GetTestcaseSet()));
    }

    static CoveringArray run(CNF *cnf, const SimpArgument &arg, int seed) {
        Argument _arg{};
        _arg.init_strength = arg.init_strength;
        _arg.candidate_set_size = arg.candidate_set_size;
        return run(cnf, _arg, seed);
    }
};
