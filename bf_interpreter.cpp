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
  virtual void execute(std::vector<uint8_t> &data, size_t &data_ptr,
                       std::istream &input, std::ostream &output) = 0;
};

class IncrementDataPointer : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &) override {
    ++data_ptr;
    if (data_ptr >= data.size()) {
      data.push_back(0);
    }
  }
};

class DecrementDataPointer : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &) override {
    if (data_ptr == 0) {
      throw std::runtime_error("Data pointer moved before the start of data.");
    }
    --data_ptr;
  }
};

class IncrementByte : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &) override {
    ++data[data_ptr];
  }
};

class DecrementByte : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &) override {
    --data[data_ptr];
  }
};

class OutputByte : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &output) override {
    output.put(static_cast<char>(data[data_ptr]));
  }
};

class InputByte : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr,
               std::istream &input, std::ostream &) override {
    int ch = input.get();
    data[data_ptr] = (ch == EOF) ? 0 : static_cast<uint8_t>(ch);
  }
};

class Loop : public Instruction {
public:
  std::vector<std::unique_ptr<Instruction>> instructions;

  void execute(std::vector<uint8_t> &data, size_t &data_ptr,
               std::istream &input, std::ostream &output) override {
    while (data[data_ptr] != 0) {
      for (const auto &instr : instructions) {
        instr->execute(data, data_ptr, input, output);
      }
    }
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

  std::vector<uint8_t> data(1, 0);
  size_t data_ptr = 0;

  try {
    for (const auto &instr : instructions) {
      instr->execute(data, data_ptr, std::cin, std::cout);
    }
  } catch (const std::exception &e) {
    std::cerr << "Error during execution: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
