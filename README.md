# DMLCS

DMLCS is a C++17 implementation of Dynamic Multiple Longest Common Subsequence computation.

The program first constructs a graph from a set of initial sequences. New sequences can then be added one by one to update the MLCS length and graph incrementally. It reports graph size, execution time, and memory usage, and can optionally backtrack through the graph to produce MLCS strings.

## Project Structure

```text
DMLCS/
├── include/        # Header files
├── src/            # C++ source files
├── Makefile        # GNU Make build configuration
├── .gitignore
├── README.md       # Chinese documentation
└── README_EN.md    # English documentation
```

## Requirements

- A compiler with C++17 support, such as GCC 7+ or a recent version of Clang
- GNU Make
- Linux, or Windows with MinGW/MSYS2

## Building

Build the optimized version:

```bash
make
```

Build the debug version:

```bash
make debug
```

Remove generated build files:

```bash
make clean
```

The default executable is named `dmlcs` on Linux and `dmlcs.exe` under MinGW/MSYS2 on Windows.

To select another compiler, run, for example:

```bash
make CXX=clang++
```

## Input Format

Both the initial-sequence file and the incremental-sequence file contain one sequence per line:

```text
ACGTACGT
GACTAGTA
ACTGACGT
```

Empty lines are ignored. Each non-empty line is treated as one complete sequence, so sequence lines should not contain spaces or comments.

## Usage

```text
dmlcs [options]
```

Available options:

| Option | Description |
| --- | --- |
| `--input FILE` | Read the initial sequences from `FILE` |
| `--add FILE` | Read sequences to be added incrementally from `FILE` |
| `--measure build` | Measure incremental graph construction only |
| `--measure include-backtrack` | Construct the graph and measure result backtracking |
| `--measure full` | Alias for `include-backtrack` |
| `--max-results N` | Return at most `N` MLCS results; the default is 10 |
| `--print-results` | Print the reconstructed MLCS strings |
| `--no-finalize` | Do not merge an expansion into the base graph |
| `--debug` | Enable diagnostic output |
| `--help`, `-h` | Display the help message |

If `--input` is omitted, the program uses the built-in sequences `ACGTACGT` and `GACTAGTA` as a demonstration.

## Examples

Construct the initial graph only:

```bash
./dmlcs --input initial.txt
```

Add new sequences and measure incremental graph construction:

```bash
./dmlcs --input initial.txt --add added.txt --measure build
```

Backtrack and print up to 20 MLCS results:

```bash
./dmlcs --input initial.txt --add added.txt \
  --measure include-backtrack \
  --max-results 20 \
  --print-results
```

In Windows PowerShell:

```powershell
.\dmlcs.exe --input initial.txt --add added.txt --measure full --print-results
```

## Output Metrics

The program reports the following metrics:

- Current MLCS length
- Number of graph nodes
- Incremental graph construction time
- Backtracking time
- Total execution time
- Peak process memory usage
- Working-set memory increase during the current stage
- Number of reconstructed results

## Notes

- `--print-results` must be used together with `--measure include-backtrack` or `--measure full`.
- `--max-results 0` disables the production of backtracked results.
- By default, each completed expansion is merged into the base graph. Later incremental sequences therefore build upon the results of earlier sequences.
- With `--no-finalize`, every sequence in the incremental input is expanded against the same initial graph; incremental sequences do not accumulate.
- The MLCS graph can grow rapidly as sequence lengths and the number of sequences increase. Monitor memory usage when processing large data sets.

## License

This project does not currently specify an open-source license. Add an appropriate `LICENSE` file before publishing the repository if you want others to use, modify, or distribute the code.
