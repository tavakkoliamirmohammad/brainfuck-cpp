# Compiler and flags
CXX := clang++
CXXFLAGS := -std=c++14 -O3

# LLVM configuration
LLVM_CXXFLAGS := $(shell llvm-config --cxxflags)
LLVM_LDFLAGS := $(shell llvm-config --ldflags --system-libs --libs core)

# Targets
all: bfi bfn_arm64 bfllvm

# Interpreter
bfi: bf_interpreter.cpp
	$(CXX) $(CXXFLAGS) -o bfi.o bf_interpreter.cpp

# ARM64 Compiler
bfn_arm64: bf_native_arm64.cpp
	$(CXX) $(CXXFLAGS) -o bfn_arm64.o bf_native_arm64.cpp

# LLVM IR Compiler
bfllvm: bf_llvm.cpp
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) -lunwind bf_llvm.cpp $(LLVM_LDFLAGS) -o bfllvm.o

# Clean up build artifacts
clean:
	rm -f bfi.o bfn_arm64.o bfllvm.o
