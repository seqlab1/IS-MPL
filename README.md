# IS-MPL

## 1. Introduction

IS-MPL is an algorithm for the multiple longest common subsequence problem with a dynamically increasing number of sequences (D-MLCS-N).

## 2. Test Data

The examples use real human DNA and protein sequences obtained from the National Center for Biotechnology Information (NCBI). The downloaded sequences were converted to a one-sequence-per-line text format. Sequences shorter than the target length were removed, and the remaining sequences were truncated to the target length.

- `examples/dna`: 2,000 initial DNA sequences of length 70 and 6 added sequences.
- `examples/protein`: 2,000 initial protein sequences of length 185 and 6 added sequences.

Each directory contains:

- `initial.txt`: sequences used for initial DAG construction.
- `additions.txt`: added sequences used for dynamic DAG extension.

## 3. Building Notes

IS-MPL requires Linux, a C++17 compiler, and Make.

```shell
make
```

## 4. Usage Notes

Run the DNA example:

```shell
./is_mpl \
  --mode auto \
  --input examples/dna/initial.txt \
  --add examples/dna/additions.txt \
  --max-results 0
```

Run the protein example:

```shell
./is_mpl \
  --mode auto \
  --input examples/protein/initial.txt \
  --add examples/protein/additions.txt \
  --max-results 0
```

For MLCS backtracking, run:

```shell
./is_mpl \
  --mode auto \
  --input examples/dna/initial.txt \
  --add examples/dna/additions.txt \
  --measure include-backtrack \
  --max-results 1000 \
  --print-results
```

## 5. Correctness Test

Run the correctness test with:

```shell
make test
```

The test compares the MLCS length and MLCS strings produced by IS-MPL with independently computed results.

## 6. Project Structure

```text
include/is_mpl/metrics.h    
include/is_mpl/solver.h     
src/graph_types.h           
src/solver.cpp              
src/io.*                    
src/main.cpp                
tests/test_cli.py           
```

## 7. License

A license has not yet been added to this repository. Add the selected license before public distribution.