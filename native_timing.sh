#!/bin/bash

INPUT_FILE="$1"
BASE_NAME=$(basename "$INPUT_FILE" .b)
CSV_FILE="${BASE_NAME}_timing.csv"

mkdir -p res
echo "Optimization,RealTime" > "res/$CSV_FILE"

time_command() {
    { time "$@" > /dev/null; } 2>&1 | awk '/real/ {print $2}'
}

# Compilation with no optimizations
echo "Compiling $BASE_NAME with no optimizations..."
./bfn_arm64.o --no-optimizations "$INPUT_FILE"
clang -O3 -o ${BASE_NAME}_no_optimizations.o output.s
echo -e "\nTiming no optimizations:"
time_data=$(time_command ./${BASE_NAME}_no_optimizations.o < ./test.txt)
echo "no-optimizations,$time_data" >> "res/$CSV_FILE"

# # Compilation with simple loop optimizations
# echo "Compiling $BASE_NAME with simple loop optimizations..."
# ./bfn_arm64.o --optimize-simple-loops "$INPUT_FILE"
# clang -O3 -o ${BASE_NAME}_optimize_simple_loops.o output.s
# echo -e "\nTiming simple loop optimizations:"
# time_data=$(time_command ./${BASE_NAME}_optimize_simple_loops.o)
# echo "optimize-simple-loops,$time_data" >> "res/$CSV_FILE"

# # Compilation with memory scan optimizations
# echo "Compiling $BASE_NAME with memory scan optimizations..."
# ./bfn_arm64.o --optimize-memory-scans "$INPUT_FILE"
# clang -O3 -o ${BASE_NAME}_optimize_memory_scans.o output.s
# echo -e "\nTiming memory scan optimizations:"
# time_data=$(time_command ./${BASE_NAME}_optimize_memory_scans.o)
# echo "optimize-memory-scans,$time_data" >> "res/$CSV_FILE"

# # Compilation with all optimizations
# echo "Compiling $BASE_NAME with all optimizations..."
# ./bfn_arm64.o --optimize-all "$INPUT_FILE"
# clang -O3 -o ${BASE_NAME}_optimize_all.o output.s
# echo -e "\nTiming all optimizations:"
# time_data=$(time_command ./${BASE_NAME}_optimize_all.o)
# echo "optimize-all,$time_data" >> "res/$CSV_FILE"

# Compilation with partial evaluation
echo "Compiling $BASE_NAME with partial evaluation..."
./bfn_pe_arm64.o --no-optimizations "$INPUT_FILE"
clang -O3 -o ${BASE_NAME}_pe.o output.s
echo -e "\nTiming artial evaluation:"
time_data=$(time_command ./${BASE_NAME}_pe.o < ./test.txt)
echo "pe,$time_data" >> "res/$CSV_FILE"

# Clean up intermediate files
# rm "${BASE_NAME}_optimize_all.o"
# rm "${BASE_NAME}_optimize_simple_loops.o"
# rm "${BASE_NAME}_optimize_memory_scans.o"
rm "${BASE_NAME}_no_optimizations.o"
rm "${BASE_NAME}_pe.o"
rm "output.s"

echo "Timing results saved to $CSV_FILE"
