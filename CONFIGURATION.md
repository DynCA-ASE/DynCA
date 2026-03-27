# The Configuration of *DynCA*

## The Default Configuration

The default configuration of *DynCA* is listed as follows:

```json
{
    "generation": {
        "init_strength": 2,
        "candidate_set_size": 120
    },
    "expansion": {
        "use_validator": "ddnnf",
        "use_solver": "minisat",
        "candidate_set_size": 80
    },
    "optimization": {
        "alt_length": 266,
        "opt_time_ms": 3300000,
        "configs": [
            {
                "use_weight": true,
                "cell_taboo_percent": 33,
                "forced_greedy_percent": 87,
                "cell_level_percent": 19,
                "taboo_size": 2
            }
        ]
    }
}
```

## Description of Hyper-parameters and Options

+ `generation.init_strength`: *DynCA* usually first employs an existing 2-wise CCAG algorithm (e.g., [SamplingCA](https://github.com/chuanluocs/SamplingCA)) to generate a 2-wise CA and then expands it to higher strength. The parameter `generation.init_strength` allows the user to let *DynCA* directly generate a CA with the specified strength.

+ `generation.candidate_set_size`: the hyper-parameter passed to *SamplingCA*.

+ `expansion.use_validator`: the validator used for tuple validation. The available values are `ddnnf`, `minisat`, and `cadical`. *DynCA* uses the **knowledge-compiled tuple validation technique**, and therefore the validator is `ddnnf`. For *Alt-1* (i.e., without the **knowledge-compiled tuple validation technique**), the validator is `minisat`.

+ `expansion.use_solver`: the sampling solver used in the generation phase. The available values are `minisat` and `cadical`. According to our training results, `minisat` achieves better performance in our experiments.

+ `expansion.candidate_set_size`: the hyper-parameter controlling the candidate set size in the generation phase.

+ `optimization.alt_length`: the hyper-parameter for the **alternating compression mechanism**. For *Alt-2*, set it to `0` to disable the **alternating compression mechanism**.

+ `optimization.opt_time_ms`: the maximum time for the optimization phase, in milliseconds.

+ `optimization.configs.use_weight`: whether to enable the `add_weight` strategy.

+ `optimization.configs.cell_taboo_percent`: the percentage of steps using cell-level tabu.

+ `optimization.configs.forced_greedy_percent`: the percentage of steps using the `forced_patching` strategy.

+ `optimization.configs.cell_level_percent`: the percentage of steps using cell-level neighborhood search. For *Alt-3*, set it to `0` to use only tuple-level neighborhood. For *Alt-4*, set it to `100` to use only cell-level neighborhood.

+ `optimization.configs.taboo_size`: the size of the tabu list (for both cell-level and assignment-level).
