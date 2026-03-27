# *DynCA*: An Effective Algorithm for 3-Wise Combinatorial Interaction Testing in Highly Configurable Systems

*DynCA* is a novel and effective algorithm for solving large-scale 3-wise CCAG problems.
This repository provides the implementation of *DynCA*, the testing instances used in the experiments, and the corresponding experimental results.

## How to Obtain *DynCA*

*DynCA* is [publicly available on GitHub](https://github.com/DynCA-ASE/DynCA). To obtain *DynCA*, use `git clone` to get a local copy of the GitHub repository:

```sh
git clone https://github.com/DynCA-ASE/DynCA.git
```

## Building *DynCA* from Source Code

*DynCA* uses C++20. The recommended version of g++ is 12.0 or later. Alternatively, you may use clang++ 16.0 or later. Other compilers are not tested and may fail to compile.

Moreover, *DynCA* depends on [jemalloc](https://github.com/jemalloc/jemalloc) for better performance. You can install `jemalloc` using `apt`:

```sh
sudo apt install libjemalloc-dev
```

Additionally, *DynCA* uses [coprocessor](https://github.com/nmanthey/riss-solver) to simplify the input CNF instance, and uses [d4v2](https://github.com/crillab/d4v2) and [FastFMC](https://github.com/ShuangyuLyu/FastFMC) to convert CNF to d-DNNF for the **knowledge-compiled tuple validation technique**. For ease of use, we provide copies of these executables in `bin/` (these executables are currently only expected to run on Ubuntu 24.04).

> Note: The [coprocessor](https://github.com/nmanthey/riss-solver) is under [LGPL license](https://github.com/nmanthey/riss-solver/blob/master/LICENSE); the [d4v2](https://github.com/crillab/d4v2) is also under [LGPL license](https://github.com/crillab/d4v2/blob/main/LICENSE).

*DynCA* uses [CMake](https://cmake.org) to configure the project, and requires CMake version 3.24 or later.  
To configure *DynCA*, use the following command:

```sh
cmake -DCMAKE_BUILD_TYPE=Release -B build .
```

If the version of your system default g++ is lower than required, you can manually specify the compiler with the following command:

```sh
# Do not forget to change `/usr/bin/gcc-12` and `/usr/bin/g++-12` to your real path for gcc and g++.
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/gcc-12 -DCMAKE_CXX_COMPILER=/usr/bin/g++-12 -B build .
```

After configuration, compile *DynCA* using the following command:

```sh
cmake --build build -t DynCA
```

The binary executable file will be at `build/DynCA`.

## Instructions for Running *DynCA*

The basic usage of *DynCA* is as follows:
```
./build/DynCA <CNF-PATH>
```

The required parameter is listed as follows:

| Parameter | Allowed Values | Description |
| - | - | - |
| `<CNF_PATH>` | path | the path of the cnf file of the input instance |

The optional parameters are listed as follows:

| Parameter | Allowed Values | Default Value | Description | 
| - | - | - | - |
| `-s` or `--seed` | integer | -- | the random seed |
| `-o` or `--output` | path | -- | the path of the output CA file |
| `--time` | positive integer | -- | the total cutoff time |
| `--conf` | path | `default.conf.json` | the configuration file for *DynCA* |

To learn more about the optional parameters of *DynCA*, call *DynCA* with the following command:

```sh
./build/DynCA -h
```

As an algorithm that combines multiple strategies, *DynCA* has many hyper-parameters and options. These are configured using a JSON configuration file. The default configuration is provided in [default.conf.json](default.conf.json).

To learn more about the hyper-parameters and options of *DynCA*, please read [CONFIGURATION.md](CONFIGURATION.md).

## Example Command for Running *DynCA*

An example command for running *DynCA* is:
```sh
./build/DynCA benchmarks/linux.cnf
```

The command above runs *DynCA* on the instance [benchmarks/linux.cnf](benchmarks/linux.cnf) with the default configuration.

> This command may yield different results across runs because the random seed of *DynCA* is not set by default.
> If you need a reproducible result, please set the random seed using `-s <seed>`.

> This command will not print the final CA because the output path of *DynCA* is not set by default.
> If you need the CA constructed by *DynCA*, please set the output path using `-o <path>`.

## Implementation of *DynCA*

The implementation of *DynCA* is in the `src/` directory. *DynCA* implements three core techniques: **knowledge-compiled tuple validation technique**, **alternating compression mechanism**, and **multi-strategy local search technique**.

## Testing Instances for Evaluating *DynCA*

The instances used in our experiments are placed in the `benchmarks/` directory. These instances are in CNF format (i.e., Boolean formulas in Conjunctive Normal Form).

## Experimental Results

The directory `experimental_results/` contains four `.csv` files that present the experimental results:

+ [Results_of_DynCA_and_its_SOTA_Competitors_on_Evaluation_Set.csv](experimental_results/Results_of_DynCA_and_its_SOTA_Competitors_on_Evaluation_Set.csv): Results of *DynCA*, *DynCA-short*, and their state-of-the-art competitors on the evaluation set for the large-scale 3-wise CCAG problem.

+ [Results_of_DynCA_and_its_SOTA_Competitors_on_Training_Set.csv](experimental_results/Results_of_DynCA_and_its_SOTA_Competitors_on_Training_Set.csv): Results of *DynCA*, *DynCA-short*, and their state-of-the-art competitors on the training set for the large-scale 3-wise CCAG problem.

+ [Results_of_DynCA_and_its_Alternative_Versions_on_Evaluation_Set.csv](experimental_results/Results_of_DynCA_and_its_Alternative_Versions_on_Evaluation_Set.csv): Results of *DynCA* and its alternative versions on the evaluation set for the large-scale 3-wise CCAG problem.

+ [Results_of_DynCA_and_its_Alternative_Versions_on_Training_Set.csv](experimental_results/Results_of_DynCA_and_its_Alternative_Versions_on_Training_Set.csv): Results of *DynCA* and its alternative versions on the training set for the large-scale 3-wise CCAG problem.
