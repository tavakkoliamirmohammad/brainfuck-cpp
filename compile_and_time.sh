#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <source_file.b>"
    exit 1
fi

# Get the input file and base name
INPUT_FILE="$1"
BASE_NAME=$(basename "$INPUT_FILE" .b)

echo "Making sure the binaries are up-to-date"
make 

# Generate Native (ARM64) assembly from the Brainf*ck source file
echo "Compiling $BASE_NAME to native code..."
./bfn_arm64.o $INPUT_FILE
clang -O3 -o ${BASE_NAME}_native.o output.s

# Generate LLVM IR from the Brainf*ck source file
echo "Generating LLVM IR from $INPUT_FILE..."
./bfllvm.o "$INPUT_FILE" &> "$BASE_NAME.ll"

# Compile the LLVM IR to native machine code without optimizations
echo "Compiling $BASE_NAME.ll to native code (unoptimized)..."
clang "$BASE_NAME.ll" -o "$BASE_NAME.o"

# Compile the LLVM IR to native machine code with -O3 optimizations
echo "Compiling $BASE_NAME.ll to native code with -O3 optimizations..."
clang "$BASE_NAME.ll" -O3 -o "${BASE_NAME}_O3.o"

# Time the execution of the interpreter
echo -e "\nTiming interpreter:"
time ./bfi.o $INPUT_FILE > /dev/null

# Time the execution of the interpreter
echo -e "\nTiming native complied code:"
time ./${BASE_NAME}_native.o > /dev/null

# Time the execution of the unoptimized binary
echo -e "\nTiming unoptimized binary ($BASE_NAME.o):"
time "./$BASE_NAME.o" > /dev/null

# Time the execution of the optimized binary
echo -e "\nTiming optimized binary (${BASE_NAME}_O3.o):"
time "./${BASE_NAME}_O3.o" > /dev/null

rm "$BASE_NAME.ll"
rm "$BASE_NAME.o"
rm "${BASE_NAME}_O3.o"
rm "output.s"
rm "${BASE_NAME}_native.o"