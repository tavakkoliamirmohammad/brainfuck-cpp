#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct ExecutionContext {
  std::vector<size_t> instruction_counts;
  std::map<size_t, size_t> loop_counts;
  bool profiler_enabled;
};

class Instruction {
public:
  size_t id; // Unique ID for the instruction
  char cmd;  // The code character
  virtual ~Instruction() = default;
  virtual void execute(std::vector<uint8_t> &data, size_t &data_ptr,
                       std::istream &input, std::ostream &output,
                       ExecutionContext &context) = 0;
};

class IncrementDataPointer : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &, ExecutionContext &context) override {
    if (context.profiler_enabled) {
      context.instruction_counts[id]++;
    }
    ++data_ptr;
    if (data_ptr >= data.size()) {
      data.push_back(0);
    }
  }
};

class DecrementDataPointer : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &, ExecutionContext &context) override {
    if (context.profiler_enabled) {
      context.instruction_counts[id]++;
    }
    if (data_ptr == 0) {
      throw std::runtime_error("Data pointer moved before the start of data.");
    }
    --data_ptr;
  }
};

class IncrementByte : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &, ExecutionContext &context) override {
    if (context.profiler_enabled) {
      context.instruction_counts[id]++;
    }
    ++data[data_ptr];
  }
};

class DecrementByte : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &, ExecutionContext &context) override {
    if (context.profiler_enabled) {
      context.instruction_counts[id]++;
    }
    --data[data_ptr];
  }
};

class OutputByte : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr, std::istream &,
               std::ostream &output, ExecutionContext &context) override {
    if (context.profiler_enabled) {
      context.instruction_counts[id]++;
    }
    output.put(static_cast<char>(data[data_ptr]));
  }
};

class InputByte : public Instruction {
public:
  void execute(std::vector<uint8_t> &data, size_t &data_ptr,
               std::istream &input, std::ostream &,
               ExecutionContext &context) override {
    if (context.profiler_enabled) {
      context.instruction_counts[id]++;
    }
    int ch = input.get();
    data[data_ptr] = (ch == EOF) ? 0 : static_cast<uint8_t>(ch);
  }
};

class Loop : public Instruction {
public:
  std::vector<std::unique_ptr<Instruction>> instructions;
  bool is_simple;
  bool is_innermost;

  void execute(std::vector<uint8_t> &data, size_t &data_ptr,
               std::istream &input, std::ostream &output,
               ExecutionContext &context) override {
    if (context.profiler_enabled) {
      context.instruction_counts[id]++;
    }
    while (data[data_ptr] != 0) {
      if (context.profiler_enabled) {
        context.loop_counts[id]++;
      }
      for (const auto &instr : instructions) {
        instr->execute(data, data_ptr, input, output, context);
      }
    }
  }
};

bool isLoopSimple(
    const std::vector<std::unique_ptr<Instruction>> &instructions) {
  // Check if any instruction is a Loop
  for (const auto &instr : instructions) {
    if (dynamic_cast<Loop *>(instr.get())) {
      return false; // Not innermost
    }
  }
  // Check if any instruction is I/O
  for (const auto &instr : instructions) {
    if (dynamic_cast<InputByte *>(instr.get()) ||
        dynamic_cast<OutputByte *>(instr.get())) {
      return false;
    }
  }

  // Compute net data pointer change
  int data_ptr_change = 0;
  for (const auto &instr : instructions) {
    if (dynamic_cast<IncrementDataPointer *>(instr.get())) {
      data_ptr_change++;
    } else if (dynamic_cast<DecrementDataPointer *>(instr.get())) {
      data_ptr_change--;
    }
  }
  if (data_ptr_change != 0) {
    return false;
  }
  // Compute net change to p[0] per iteration
  int p0_change = 0;
  for (const auto &instr : instructions) {
    if (dynamic_cast<IncrementByte *>(instr.get())) {
      p0_change++;
    } else if (dynamic_cast<DecrementByte *>(instr.get())) {
      p0_change--;
    }
  }

  if (p0_change != 1 && p0_change != -1) {
    return false;
  }
  return true;
}

std::vector<std::unique_ptr<Instruction>>
parse(const std::string &code, size_t &index, size_t &instruction_id,
      std::vector<char> &instruction_cmds, std::vector<Loop *> &loops,
      Loop *parent_loop) {
  std::vector<std::unique_ptr<Instruction>> instructions;

  while (index < code.size()) {
    char cmd = code[index++];
    switch (cmd) {
    case '>': {
      auto instr = std::make_unique<IncrementDataPointer>();
      instr->id = instruction_id++;
      instr->cmd = '>';
      instruction_cmds.push_back('>');
      instructions.push_back(std::move(instr));
      break;
    }
    case '<': {
      auto instr = std::make_unique<DecrementDataPointer>();
      instr->id = instruction_id++;
      instr->cmd = '<';
      instruction_cmds.push_back('<');
      instructions.push_back(std::move(instr));
      break;
    }
    case '+': {
      auto instr = std::make_unique<IncrementByte>();
      instr->id = instruction_id++;
      instr->cmd = '+';
      instruction_cmds.push_back('+');
      instructions.push_back(std::move(instr));
      break;
    }
    case '-': {
      auto instr = std::make_unique<DecrementByte>();
      instr->id = instruction_id++;
      instr->cmd = '-';
      instruction_cmds.push_back('-');
      instructions.push_back(std::move(instr));
      break;
    }
    case '.': {
      auto instr = std::make_unique<OutputByte>();
      instr->id = instruction_id++;
      instr->cmd = '.';
      instruction_cmds.push_back('.');
      instructions.push_back(std::move(instr));
      break;
    }
    case ',': {
      auto instr = std::make_unique<InputByte>();
      instr->id = instruction_id++;
      instr->cmd = ',';
      instruction_cmds.push_back(',');
      instructions.push_back(std::move(instr));
      break;
    }
    case '[': {
      if (parent_loop) {
        parent_loop->is_innermost = false;
      }
      auto loop = std::make_unique<Loop>();
      loop->id = instruction_id++;
      loop->cmd = '[';
      loop->is_innermost = true;
      instruction_cmds.push_back('[');
      loops.push_back(loop.get());

      loop->instructions = parse(code, index, instruction_id, instruction_cmds,
                                 loops, loop.get());

      loop->is_simple = isLoopSimple(loop->instructions);

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
  bool profiler_enabled = false;
  std::string filename;

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-p") {
      profiler_enabled = true;
    } else {
      filename = arg;
    }
  }

  // Read Brainfuck code from a file or standard input
  std::string code;
  std::ifstream file;

  if (!filename.empty()) {
    file.open(filename);
    if (!file) {
      std::cerr << "Failed to open file: " << filename << '\n';
      return 1;
    }
  }

  std::istream &input = (!filename.empty()) ? file : std::cin;

  std::ostringstream oss;
  oss << input.rdbuf();
  code = oss.str();

  size_t index = 0;
  size_t instruction_id = 0;
  std::vector<char> instruction_cmds;
  std::vector<Loop *> loops;
  std::vector<std::unique_ptr<Instruction>> instructions;

  try {
    instructions =
        parse(code, index, instruction_id, instruction_cmds, loops, nullptr);
  } catch (const std::exception &e) {
    std::cerr << "Error while parsing: " << e.what() << '\n';
    return 1;
  }

  std::vector<uint8_t> data(1, 0);
  size_t data_ptr = 0;

  ExecutionContext context;
  context.instruction_counts.resize(instruction_id, 0);
  context.profiler_enabled = profiler_enabled;

  try {
    for (const auto &instr : instructions) {
      instr->execute(data, data_ptr, std::cin, std::cout, context);
    }
  } catch (const std::exception &e) {
    std::cerr << "Error during execution: " << e.what() << '\n';
    return 1;
  }

  if (profiler_enabled) {
    // Print instruction counts
    std::cout << "\nInstruction execution counts:\n";
    for (size_t i = 0; i < context.instruction_counts.size(); ++i) {
      char cmd = instruction_cmds[i];
      size_t count = context.instruction_counts[i];
      if (count > 0) {
        std::cout << cmd << " " << count << "\n";
      }
    }

    // Process loops
    std::vector<std::pair<Loop *, size_t>> simple_innermost_loops;
    std::vector<std::pair<Loop *, size_t>> non_simple_innermost_loops;

    for (Loop *loop : loops) {
      if (loop->is_innermost) {
        size_t count = context.loop_counts[loop->id];
        if (count > 0) {
          if (loop->is_simple) {
            simple_innermost_loops.emplace_back(loop, count);
          } else {
            non_simple_innermost_loops.emplace_back(loop, count);
          }
        }
      }
    }

    // Sort the loops in decreasing order of execution counts
    auto loop_compare = [](const std::pair<Loop *, size_t> &a,
                           const std::pair<Loop *, size_t> &b) {
      return a.second > b.second;
    };
    std::sort(simple_innermost_loops.begin(), simple_innermost_loops.end(),
              loop_compare);
    std::sort(non_simple_innermost_loops.begin(),
              non_simple_innermost_loops.end(), loop_compare);

    // Print the simple innermost loops
    std::cout << "\nSimple innermost loops:\n";
    for (const auto &pair : simple_innermost_loops) {
      Loop *loop = pair.first;
      size_t count = pair.second;
      std::cout << "Loop at instruction id " << loop->id << " executed "
                << count << " times\n";
    }

    // Print the non-simple innermost loops
    std::cout << "\nNon-simple innermost loops:\n";
    for (const auto &pair : non_simple_innermost_loops) {
      Loop *loop = pair.first;
      size_t count = pair.second;
      std::cout << "Loop at instruction id " << loop->id << " executed "
                << count << " times\n";
    }
  }

  return 0;
}
