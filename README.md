# IS+MPL

## 1. Introduction

IS+MPL is an incremental algorithm for the multiple longest common subsequence problem with a dynamically increasing number of sequences (D-MLCS-N).

The program first constructs a directed acyclic graph (DAG) for an initial sequence set. When a new sequence arrives, it reuses the immediate-successor relationships in the existing DAG and organizes the expanded states by their matched positions in the new sequence. The resulting graph can then be used as the input graph for the next update.

The implementation supports:

- incremental insertion of multiple sequences;
- automatic construction of the input symbol set;
- graph-construction time and Linux process-memory measurements;
- optional backtracking of distinct MLCS strings;
- both a command-line interface and a reusable C++ API.

## 2. Input Data

Two text files are used:

1. The initial-sequence file contains the sequences used to construct the initial DAG.
2. The addition file contains the new sequences in their arrival order.

Each nonempty line represents one sequence. For example:

```text
ACGTACGT
GACTAGTA
```

The program derives the symbol set directly from the sequences. Symbols are case-sensitive. DNA, protein, and custom finite character sets are supported.

Example files are provided in the `examples` directory.

## 3. Building Notes

The program is written in C++17 and targets Linux. GCC, Make, and Python 3 are sufficient for the standard build and correctness test.

Build with Make:

```shell
make
```

The command generates an executable named `is_mpl` in the project directory.

The experiment build used the following compiler options:

```shell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic
```

CMake can also be used:

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 4. Usage Notes

### 4.1 Incremental construction

```shell
./is_mpl \
  --mode auto \
  --input examples/initial.txt \
  --add examples/additions.txt \
  --max-results 0
```

The sequences in `additions.txt` are processed one by one. After every update, the newly generated DAG is promoted and reused by the next update.

### 4.2 Constructing only the initial DAG

```shell
./is_mpl \
  --mode initial \
  --input examples/initial.txt
```

### 4.3 Backtracking MLCS strings

```shell
./is_mpl \
  --mode auto \
  --input examples/initial.txt \
  --add examples/additions.txt \
  --measure include-backtrack \
  --max-results 1000 \
  --print-results
```

The available options are:

| Option | Description |
|---|---|
| `--input FILE` | File containing the initial sequences. |
| `--add FILE` | File containing the sequences to add. |
| `--mode auto` | Run incremental construction when additions are provided. |
| `--mode initial` | Construct only the initial DAG. |
| `--measure build-only` | Measure graph construction without MLCS backtracking. This is the default. |
| `--measure include-backtrack` | Backtrack MLCS strings after each update. |
| `--max-results N` | Return at most `N` distinct MLCS strings. |
| `--print-results` | Print the backtracked MLCS strings. |
| `--debug` | Print diagnostic messages for the initial-DAG traversal. |

## 5. Algorithm Outline

For each input sequence, a successor-position table stores the next occurrence of every symbol after every position. The initial DAG is constructed from matching-position vectors.

During an incremental update, a state is represented by:

```text
(node in the existing DAG, matched position in the new sequence)
```

The update performs the following operations:

1. Reuse the outgoing immediate-successor edges of the existing DAG node.
2. Query the successor-position table of the new sequence for the next matching position.
3. Place each newly discovered state in the corresponding matched-position layer.
4. Process the layers in increasing position order.
5. Retain all predecessors that produce the longest path to a state.
6. Promote the expanded graph for the next sequence insertion.

The character set and its character-to-column mapping are private implementation details in `src/solver.cpp`.

## 6. Output Metrics

The command-line program reports the MLCS length, graph size, elapsed time, and process memory after each stage.

- `Initial Build Time(s)` measures initial-DAG construction. The initial successor-position tables are prepared before this timer starts.
- `Incremental Build Time(s)` includes the successor-position table for the new sequence and the expanded-DAG construction.
- Graph promotion for the next update is performed after the current stage metrics are printed.
- `Peak Memory(GB)` is the absolute peak resident memory of the process up to the reporting point.
- `Snapshot Delta(GB)` is the nonnegative difference between resident memory at the beginning and end of the stage.
- Backtracking time is included only when `--measure include-backtrack` is selected.

The Linux process-memory values are read from `/proc/self/status` and `getrusage()` inside `src/solver.cpp`. The command-line output is produced by `src/main.cpp`.

## 7. Correctness Test

Run the test with:

```shell
make test
```

`tests/test_cli.py` independently enumerates the MLCS of several small sequence sets by brute force. It compares both the MLCS length and the complete set of MLCS strings with the output produced after every incremental update. The test code is not used by the solver executable.

With CMake:

```shell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 8. Library Interface

The solver can be used directly from C++:

```cpp
#include <is_mpl/solver.h>

is_mpl::Solver solver;
solver.set_initial_sequences({"ACGT", "AGCT"});

auto initial = solver.build_initial_dag();
auto update = solver.expand_with_new_sequence("ACT");
solver.finalize_expansion();
```

`finalize_expansion()` must be called before inserting the next sequence.

## 9. Project Structure

```text
include/is_mpl/metrics.h    Runtime metrics
include/is_mpl/solver.h     Public solver API
src/graph_types.h           DAG node and edge data structures
src/solver.cpp              Initial and incremental DAG construction
src/io.*                    Input-file reader
src/main.cpp                Command-line interface and output
tests/test_cli.py           Independent brute-force correctness test
```

## 10. License

A license has not yet been added to this repository. Add the selected license before public distribution.
