# Brainfuck Interpreter and LLVM Compiler in C++

This repository contains two implementations of a Brainfuck language processor in C++:

1. **Brainfuck Interpreter**: A simple interpreter that executes Brainfuck code and displays the memory content after execution.
2. **Brainfuck to LLVM IR Compiler**: A compiler that translates Brainfuck code into LLVM Intermediate Representation (IR), which can then be compiled into machine code using LLVM tools.

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
  - [Brainfuck Interpreter](#brainfuck-interpreter)
  - [Brainfuck to LLVM IR Compiler](#brainfuck-to-llvm-ir-compiler)
- [Examples](#examples)
- [License](#license)

## Features

- **Full Brainfuck Support**: Implements all standard Brainfuck commands:
  - `>`: Increment the data pointer.
  - `<`: Decrement the data pointer.
  - `+`: Increment the byte at the data pointer.
  - `-`: Decrement the byte at the data pointer.
  - `.`: Output the byte at the data pointer.
  - `,`: Input a byte and store it at the data pointer.
  - `[` and `]`: Loop control commands.
- **LLVM IR Generation**: The compiler emits LLVM IR code, allowing further optimization and compilation to native code.

## Requirements

- **C++ Compiler**: A C++14 compliant compiler.
- **LLVM**: LLVM development libraries and tools installed (version 9.0 or later is recommended).

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/tavakkoliamirmohammad/brainfuck-cpp.git
cd brainfuck-cpp
```

### 2. Install LLVM

Ensure LLVM is installed on your system and that `llvm-config` is available in your `PATH`.

- **macOS with Homebrew**:

  ```bash
  brew install llvm
  export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
  ```

- **Linux (Ubuntu)**:

  ```bash
  sudo apt-get install llvm llvm-dev
  ```

### 3. Build the Interpreter and Compiler

#### Manual Compilation

- **Brainfuck Interpreter**

  ```bash
  clang++ -std=c++14 -O3 -o bfi.o bf_interpreter.cpp
  ```

- **Brainfuck to ARM64 Compiler**

```bash
clang++ -std=c++14 -O3 -o bfn_arm64.o bf_native_arm64.cpp
```

- **Brainfuck to LLVM IR Compiler**

```bash
clang++ -std=c++14 -O3 bf_llvm.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -lunwind -o bfllvm.o
```

## Usage

### Brainfuck Interpreter

Run the interpreter with a Brainfuck source file:

```bash
./bf_interpreter your_program.b
```

Or pipe Brainfuck code into the interpreter:

```bash
echo "++>+++[<+>-]." | ./bf_interpreter
```

### Brainfuck to LLVM IR Compiler

#### 1. Compile Brainfuck to LLVM IR

```bash
./bf_to_llvm.o your_program.b > your_program.ll
```

#### 2. Compile LLVM IR to Object Code

```bash
llc -filetype=obj your_program.ll -o your_program.o
```

#### 3. Link and Create Executable

```bash
clang your_program.o -o your_program
```

#### 4. Run the Executable

```bash
./your_program
```

## Examples

### Hello World

**Brainfuck Code (`hello_world.bf`):**

```brainfuck
++++++++++[>+++++++>++++++++++>+++>+<<<<-]
>++.>+.+++++++..+++.>++.<<+++++++++++++++.
>.+++.------.--------.>+.>.
```

**Run with Interpreter:**

```bash
./bf_interpreter.o hello_world.bf
```

**Compile and Run with LLVM Compiler:**

```bash
./bf_to_llvm hello_world.b > hello_world.ll
clang hello_world.ll -o hello_world.o
./hello_world.o
```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---
