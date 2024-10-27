#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Optimization flags
bool optimize_simple_loops = false;
bool optimize_memory_scans = false;

// Data structures for partial evaluation
struct DataCell {
  int value = 0;
  bool tainted = false;
};

using DataTape = std::unordered_map<int, DataCell>;

class Instruction {
public:
  virtual ~Instruction() = default;
  virtual void execute(std::ostream &output, int &label_counter) = 0;
  virtual bool isLoop() const { return false; }
  virtual bool isIO() const { return false; }
  virtual std::unique_ptr<Instruction> optimize() { return nullptr; }
  virtual bool partialEvaluate(DataTape &data_tape, int &data_ptr,
                               std::vector<char> &output) {
    return false;
  }
};

class IncrementDataPointer : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tADD X19, X19, #1\n";
  }

  bool partialEvaluate(DataTape & /*data_tape*/, int &data_ptr,
                       std::vector<char> & /*output*/) override {
    data_ptr += 1;
    return true;
  }
};

class DecrementDataPointer : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tSUB X19, X19, #1\n";
  }

  bool partialEvaluate(DataTape & /*data_tape*/, int &data_ptr,
                       std::vector<char> & /*output*/) override {
    data_ptr -= 1;
    return true;
  }
};

class IncrementByte : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tLDRB W1, [X19]\n";
    output << "\tADD W1, W1, #1\n";
    output << "\tSTRB W1, [X19]\n";
  }

  bool partialEvaluate(DataTape &data_tape, int &data_ptr,
                       std::vector<char> & /*output*/) override {
    auto &cell = data_tape[data_ptr];
    if (!cell.tainted) {
      cell.value = (cell.value + 1) % 256;
      return true;
    }
    return false;
  }
};

class DecrementByte : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tLDRB W1, [X19]\n";
    output << "\tSUB W1, W1, #1\n";
    output << "\tSTRB W1, [X19]\n";
  }

  bool partialEvaluate(DataTape &data_tape, int &data_ptr,
                       std::vector<char> & /*output*/) override {
    auto &cell = data_tape[data_ptr];
    if (!cell.tainted) {
      cell.value = (cell.value + 255) % 256;
      return true;
    }
    return false;
  }
};

class OutputByte : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tLDRB W0, [X19]\n";
    output << "\tBL _putchar\n";
  }
  bool isIO() const override { return true; }

  bool partialEvaluate(DataTape &data_tape, int &data_ptr,
                       std::vector<char> &output) override {
    auto &cell = data_tape[data_ptr];
    if (!cell.tainted) {
      output.push_back(static_cast<char>(cell.value));
      return true;
    }
    return false;
  }
};

class InputByte : public Instruction {
public:
  void execute(std::ostream &output, int & /*label_counter*/) override {
    output << "\tBL _getchar\n";
    output << "\tSTRB W0, [X19]\n";
  }
  bool isIO() const override { return true; }

  bool partialEvaluate(DataTape &data_tape, int &data_ptr,
                       std::vector<char> & /*output*/) override {
    data_tape[data_ptr].tainted = true;
    return false;
  }
};

class OptimizedSimpleLoop : public Instruction {
public:
  std::unordered_map<int, int> cell_changes; // cell offset to change
  OptimizedSimpleLoop(const std::unordered_map<int, int> &changes)
      : cell_changes(changes) {}
  void execute(std::ostream &output, int & /*label_counter*/) override {
    // Load p[0] into W0
    output << "\tLDRB W0, [X19]\n";
    // For each cell offset, apply the changes
    for (const auto &pair : cell_changes) {
      int offset = pair.first;
      int change = pair.second;
      if (offset == 0)
        continue; // Skip p[0], we'll set it to 0 at the end
      if (change == 0)
        continue; // No change to this cell
      // Load p[offset] into W1
      output << "\tLDRB W1, [X19, #" << offset << "]\n";
      // Apply the change: W1 += W0 * change
      if (change == 1) {
        output << "\tADD W1, W1, W0\n";
      } else if (change == -1) {
        output << "\tSUB W1, W1, W0\n";
      } else {
        // For changes greater than 1 or less than -1
        output << "\tMOV W2, #" << std::abs(change) << "\n";
        output << "\tMUL W2, W0, W2\n";
        if (change > 0) {
          output << "\tADD W1, W1, W2\n";
        } else {
          output << "\tSUB W1, W1, W2\n";
        }
      }
      // Store back to p[offset]
      output << "\tSTRB W1, [X19, #" << offset << "]\n";
    }
    // Set p[0] to 0
    output << "\tMOV W1, #0\n";
    output << "\tSTRB W1, [X19]\n";
  }
};

class OptimizedMemoryScan : public Instruction {
public:
  int direction; // +1 for '>', -1 for '<'
  OptimizedMemoryScan(int dir) : direction(dir) {}
  void execute(std::ostream &output, int &label_counter) override {
    int loop_label = label_counter++;
    int found_label = label_counter++;
    int not_found_label = label_counter++;

    // Set the scan direction
    std::string direction_instr = (direction == 1) ? "ADD" : "SUB";
    std::string offset = (direction == 1) ? "#16" : "#-16";

    output << "\t// Optimized Memory Scan\n";
    output << "L" << loop_label << ":\n";
    output << "\t// Load 16 bytes into vector register V0\n";
    output << "\tLD1 {V0.16B}, [X19]\n";
    output << "\t// Compare bytes in V0 with zero\n";
    output << "\tCMEQ V1.16B, V0.16B, #0\n";
    output << "\t// Find first non-zero byte\n";
    output << "\tUMOV W2, V1.B[0]\n";
    output << "\tCBNZ W2, L" << found_label << "\n";
    output << "\t" << direction_instr << " X19, X19, " << offset << "\n";
    output << "\tB L" << loop_label << "\n";
    output << "L" << found_label << ":\n";
  }
};

void optimizeInstructions(
    std::vector<std::unique_ptr<Instruction>> &instructions);

class Loop : public Instruction {
public:
  std::vector<std::unique_ptr<Instruction>> instructions;
  bool isLoop() const override { return true; }

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

  std::unique_ptr<Instruction> optimize() override {
    // First, optimize inner loops recursively
    optimizeInstructions(instructions);

    // Apply optimizations based on flags
    if (optimize_simple_loops && canOptimizeSimpleLoop()) {
      // Create an OptimizedSimpleLoop
      std::unordered_map<int, int> cell_changes = getCellChanges();
      return std::make_unique<OptimizedSimpleLoop>(cell_changes);
    } else if (optimize_memory_scans && canOptimizeMemoryScan()) {
      int direction = getMemoryScanDirection();
      return std::make_unique<OptimizedMemoryScan>(direction);
    }
    // No optimization possible; return nullptr
    return nullptr;
  }

  bool partialEvaluate(DataTape &data_tape, int &data_ptr,
                       std::vector<char> &output) override {
    auto &cell = data_tape[data_ptr];
    if (cell.tainted) {
      return false; // Cannot evaluate at compile time
    }
    if (cell.value == 0) {
      return true; // Loop does not execute
    }

    int loop_counter = 0;
    const int MAX_LOOP_ITERATIONS = 10000000; // Prevent infinite loops

    while (cell.value != 0) {
      loop_counter++;
      if (loop_counter > MAX_LOOP_ITERATIONS) {
        throw std::runtime_error(
            "Possible infinite loop during partial evaluation");
      }
      for (auto &instr : instructions) {
        if (!instr->partialEvaluate(data_tape, data_ptr, output)) {
          return false; // Cannot evaluate loop at compile time
        }
      }
      cell = data_tape[data_ptr]; // Update the cell after loop body
    }
    return true;
  }

private:
  bool canOptimizeSimpleLoop() const {
    int pointer = 0;
    std::unordered_map<int, int> cell_changes;
    for (const auto &instr : instructions) {
      if (instr->isLoop() || instr->isIO()) {
        return false; // Contains loops or I/O
      }
      if (dynamic_cast<const IncrementDataPointer *>(instr.get())) {
        pointer += 1;
      } else if (dynamic_cast<const DecrementDataPointer *>(instr.get())) {
        pointer -= 1;
      } else if (dynamic_cast<const IncrementByte *>(instr.get())) {
        cell_changes[pointer] += 1;
      } else if (dynamic_cast<const DecrementByte *>(instr.get())) {
        cell_changes[pointer] -= 1;
      } else {
        return false; // Unknown instruction
      }
    }
    if (pointer != 0) {
      return false; // Net pointer movement is not zero
    }
    if (cell_changes[0] != -1 && cell_changes[0] != 1) {
      return false; // p[0] not changed by +1 or -1
    }
    return true;
  }

  std::unordered_map<int, int> getCellChanges() const {
    int pointer = 0;
    std::unordered_map<int, int> cell_changes;
    for (const auto &instr : instructions) {
      if (dynamic_cast<const IncrementDataPointer *>(instr.get())) {
        pointer += 1;
      } else if (dynamic_cast<const DecrementDataPointer *>(instr.get())) {
        pointer -= 1;
      } else if (dynamic_cast<const IncrementByte *>(instr.get())) {
        cell_changes[pointer] += 1;
      } else if (dynamic_cast<const DecrementByte *>(instr.get())) {
        cell_changes[pointer] -= 1;
      }
    }
    return cell_changes;
  }

  bool canOptimizeMemoryScan() const {
    int pointer = 0;
    for (const auto &instr : instructions) {
      if (instr->isLoop() || instr->isIO()) {
        return false; // Contains loops or I/O
      }
      if (dynamic_cast<const IncrementDataPointer *>(instr.get())) {
        pointer += 1;
      } else if (dynamic_cast<const DecrementDataPointer *>(instr.get())) {
        pointer -= 1;
      } else {
        return false; // Contains instructions other than '<' or '>'
      }
    }
    if (pointer == 0) {
      return false; // Net pointer movement is zero
    }
    // Check if net pointer movement is a power of 2
    int abs_pointer = std::abs(pointer);
    return (abs_pointer & (abs_pointer - 1)) == 0;
  }

  int getMemoryScanDirection() const {
    int pointer = 0;
    for (const auto &instr : instructions) {
      if (dynamic_cast<const IncrementDataPointer *>(instr.get())) {
        pointer += 1;
      } else if (dynamic_cast<const DecrementDataPointer *>(instr.get())) {
        pointer -= 1;
      }
    }
    return (pointer > 0) ? 1 : -1;
  }
};

void optimizeInstructions(
    std::vector<std::unique_ptr<Instruction>> &instructions) {
  for (size_t i = 0; i < instructions.size(); ++i) {
    auto &instr = instructions[i];
    if (auto optimized_instr = instr->optimize()) {
      // Replace the instruction with the optimized instruction
      instructions[i] = std::move(optimized_instr);
    } else {
      // If the instruction is a Loop, we need to optimize its inner
      // instructions
      if (auto loop = dynamic_cast<Loop *>(instr.get())) {
        optimizeInstructions(loop->instructions);
      }
    }
  }
}

void partialEvaluateInstructions(
    std::vector<std::unique_ptr<Instruction>> &instructions,
    DataTape &data_tape, int &data_ptr, std::vector<char> &output,
    std::vector<std::unique_ptr<Instruction>> &new_instructions) {
  for (auto &instr : instructions) {
    if (instr->partialEvaluate(data_tape, data_ptr, output)) {
      // Instruction fully evaluated at compile time; do not keep it
    } else {
      // If it's a loop, recursively process inner instructions
      if (auto loop = dynamic_cast<Loop *>(instr.get())) {
        DataTape loop_data_tape = data_tape; // Copy current data tape
        int loop_data_ptr = data_ptr;
        std::vector<char> loop_output;
        std::vector<std::unique_ptr<Instruction>> new_loop_instructions;

        partialEvaluateInstructions(loop->instructions, loop_data_tape,
                                    loop_data_ptr, loop_output,
                                    new_loop_instructions);

        // Replace the loop instructions with the new instructions
        loop->instructions = std::move(new_loop_instructions);
      }
      new_instructions.push_back(std::move(instr));
    }
  }
}

// Parsing function
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

bool parseArguments(int argc, char *argv[], std::string &filename) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " [options] <filename>\n";
    std::cerr << "Options:\n";
    std::cerr
        << "  --no-optimizations          Disable all loop optimizations\n";
    std::cerr << "  --optimize-simple-loops     Optimize simple loops only\n";
    std::cerr << "  --optimize-memory-scans     Optimize memory scans only\n";
    std::cerr << "  --optimize-all              Optimize both simple loops and "
                 "memory scans (default)\n";
    return false;
  }

  // Default is to optimize both simple loops and memory scans
  optimize_simple_loops = true;
  optimize_memory_scans = true;

  std::vector<std::string> args(argv + 1, argv + argc);

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--no-optimizations") {
      optimize_simple_loops = false;
      optimize_memory_scans = false;
    } else if (args[i] == "--optimize-simple-loops") {
      optimize_simple_loops = true;
      optimize_memory_scans = false;
    } else if (args[i] == "--optimize-memory-scans") {
      optimize_simple_loops = false;
      optimize_memory_scans = true;
    } else if (args[i] == "--optimize-all") {
      optimize_simple_loops = true;
      optimize_memory_scans = true;
    } else if (args[i][0] != '-') {
      filename = args[i];
    } else {
      std::cerr << "Unknown option: " << args[i] << "\n";
      return false;
    }
  }

  if (filename.empty()) {
    std::cerr << "Error: No input file specified.\n";
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  std::string filename;
  if (!parseArguments(argc, argv, filename)) {
    return 1;
  }

  std::string code;
  std::ifstream file(filename);

  if (!file) {
    std::cerr << "Failed to open file: " << filename << '\n';
    return 1;
  }

  std::ostringstream oss;
  oss << file.rdbuf();
  code = oss.str();

  size_t index = 0;
  std::vector<std::unique_ptr<Instruction>> instructions;
  try {
    instructions = parse(code, index);
  } catch (const std::exception &e) {
    std::cerr << "Error while parsing: " << e.what() << '\n';
    return 1;
  }

  // Partial evaluation
  DataTape data_tape;
  data_tape[0] = DataCell{0, false};
  int data_ptr = 0;
  std::vector<char> compile_time_output;
  std::vector<std::unique_ptr<Instruction>> new_instructions;

  try {
    partialEvaluateInstructions(instructions, data_tape, data_ptr,
                                compile_time_output, new_instructions);
  } catch (const std::exception &e) {
    std::cerr << "Error during partial evaluation: " << e.what() << '\n';
    return 1;
  }

  instructions = std::move(new_instructions);

  if (optimize_simple_loops || optimize_memory_scans) {
    optimizeInstructions(instructions);
  }

  // Generate ARM64 assembly code
  int label_counter = 0;
  std::ofstream output_file("output.s");
  if (!output_file) {
    std::cerr << "Failed to open output file.\n";
    return 1;
  }

  // Define all functions used from external sources
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

  // Output compile-time generated outputs
  for (char c : compile_time_output) {
    output_file << "\tMOV W0, #" << static_cast<int>(static_cast<unsigned char>(c)) << "\n";
    output_file << "\tBL _putchar\n";
  }

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
