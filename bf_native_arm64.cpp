#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class Instruction {
public:
  virtual ~Instruction() = default;
  virtual void execute(std::ostream &output, int &label_counter) = 0;
};

class IncrementDataPointer : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tADD X19, X19, #1\n";
  }
};

class DecrementDataPointer : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tSUB X19, X19, #1\n";
  }
};

class IncrementByte : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tLDRB W1, [X19]\n";
    output << "\tADD W1, W1, #1\n";
    output << "\tSTRB W1, [X19]\n";
  }
};

class DecrementByte : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tLDRB W1, [X19]\n";
    output << "\tSUB W1, W1, #1\n";
    output << "\tSTRB W1, [X19]\n";
  }
};

class OutputByte : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tLDRB W0, [X19]\n";
    output << "\tBL _putchar\n";
  }
};

class InputByte : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tBL _getchar\n";
    output << "\tSTRB W0, [X19]\n";
  }
};

class Loop : public Instruction {
public:
  std::vector<std::unique_ptr<Instruction>> instructions;

  void execute(std::ostream &output, int &label_counter) override {
    int start_label = label_counter++;
    int end_label = label_counter++;

    output << "L" << start_label << ":\n";
    output << "\tLDRB W1, [X19]\n";
    output << "\tCBZ W1, L" << end_label << "\n";

    for (const auto &instr : instructions) {
      instr->execute(output, label_counter);
    }

    output << "\tB L" << start_label << "\n";
    output << "L" << end_label << ":\n";
  }
};

std::vector<std::unique_ptr<Instruction>> parse(const std::string &code,
                                                size_t &index) {
  std::vector<std::unique_ptr<Instruction>> instructions;

  while (index < code.size()) {
    char cmd = code[index++];
    switch (cmd) {
    case '>':
      instructions.push_back(std::make_unique<IncrementDataPointer>());
      break;
    case '<':
      instructions.push_back(std::make_unique<DecrementDataPointer>());
      break;
    case '+':
      instructions.push_back(std::make_unique<IncrementByte>());
      break;
    case '-':
      instructions.push_back(std::make_unique<DecrementByte>());
      break;
    case '.':
      instructions.push_back(std::make_unique<OutputByte>());
      break;
    case ',':
      instructions.push_back(std::make_unique<InputByte>());
      break;
    case '[': {
      auto loop = std::make_unique<Loop>();
      loop->instructions = parse(code, index);
      instructions.push_back(std::move(loop));
      break;
    }
    case ']':
      return instructions;
    default:
      // Ignore non-command characters (comments)
      break;
    }
  }
  return instructions;
}

int main(int argc, char *argv[]) {
  // Read Brainfuck code from a file or standard input
  std::string code;
  std::ifstream file;

  if (argc > 1) {
    file.open(argv[1]);
    if (!file) {
      std::cerr << "Failed to open file: " << argv[1] << '\n';
      return 1;
    }
  }

  std::istream &input = (argc > 1) ? file : std::cin;

  std::ostringstream oss;
  oss << input.rdbuf();
  code = oss.str();

  size_t index = 0;
  std::vector<std::unique_ptr<Instruction>> instructions;
  try {
    instructions = parse(code, index);
  } catch (const std::exception &e) {
    std::cerr << "Error while parsing: " << e.what() << '\n';
    return 1;
  }

  // Generate ARM64 assembly code
  int label_counter = 0;
  std::ofstream output_file("output.s");
  if (!output_file) {
    std::cerr << "Failed to open output file.\n";
    return 1;
  }

  // Define all functions used frome external sources
  output_file << "\t.text\n";
  output_file << "\t.global _main\n";
  output_file << "\t.extern _putchar, _getchar, _malloc, _free, _memset\n";
  output_file << "_main:\n";

  // Save frame pointer and link register onto stack
  output_file << "\tSTP X29, X30, [SP, #-16]!\n"; //
  output_file << "\tMOV X29, SP\n";

  // Save X19 and X20
  output_file << "\tSTP X19, X20, [SP, #-16]!\n";

  // Allocate data array using malloc
  output_file << "\tMOV X0, #30000\n"; // Allocate 30,000 bytes
  output_file << "\tBL _malloc\n";

  // Store data pointer for tape in X19
  output_file << "\tMOV X19, X0\n";

  // Store original pointer allocated in X20
  output_file << "\tMOV X20, X0\n";

  // Zero out the allocated memory (optional but recommended)
  output_file << "\tMOV X1, X19\n";    // Destination pointer
  output_file << "\tMOV W2, #0\n";     // Value to set (zero)
  output_file << "\tMOV X3, #30000\n"; // Number of bytes
  output_file << "\tBL _memset\n";

  try {
    for (const auto &instr : instructions) {
      instr->execute(output_file, label_counter);
    }
  } catch (const std::exception &e) {
    std::cerr << "Error during code generation: " << e.what() << '\n';
    return 1;
  }

  // Free the allocated memory
  output_file << "\tMOV X0, X20\n";
  output_file << "\tBL _free\n";

  // Restore callee-saved registers
  output_file << "\tLDP X19, X20, [SP], #16\n"; // Restore X19 and X20
  output_file << "\tLDP X29, X30, [SP], #16\n"; // Restore frame pointer and
                                                // link register

  // Return from main
  output_file << "\tMOV W0, #0\n";
  output_file << "\tRET\n";

  output_file.close();

  return 0;
}
