# Robust ECDSA Threshold Signing at Scale

The following folder contains the code for the experiments in the paper *Robust
ECDSA Threshold Signing at Scale*. This code implements the Sign, Mul and Rand
protocol, as well as makes it possible to perform a comparison against a
threshold ECDSA protocol currently being used in the Partisia Blockchain
(PBCTS).

The structure of this folder is as follows

```
data: Data used to generate figures in our paper
 +- traces.zip: Execution traces of running our different protocols
scripts: Scripts used to run the different experiments
common: Source code common to all experiments
libs: Contains pbcts.so
experiments: Source code for our implementation
 |- mul: The Mul protocol
 |- rand: The Rand protocol
 |- sign: The Sign protocol
 +- pbcts: A wrapper around pbcts.so
```

## Building the experiments

The code depends on
[SCL](https://github.com/anderspkd/secure-computation-library), which requires
GMP and C++20, but otherwise have no external dependencies. We ran our
experiments using `g++ 13.2.1` as the compiler.

To build the experiments:

```
cmake . -B build -DCMAKE_BUILD_TYPE=Release
cd build && make
```

After compilation a number of different executables are created:
- `sign.x` used to run the sign protocol.
- `mul.x` used to run the mul protocol.
- `rand.x` used to run the rand protocol.
- `pbcts.x` used to run the PBCTS protocol.
- `benchmark_apocm.x` benchmark for the APOCM protocol.

## Generating the figures in our paper

The `data/figures.ipynb` notebook contains the code used to generate the figures
in our paper. The `data/utils.py` code is used to parse the JSON output of the
C++ code.

