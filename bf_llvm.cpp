#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

class Instruction {
public:
  virtual ~Instruction() = default;
  virtual void generateCode(llvm::IRBuilder<> &builder, llvm::Value *tape_ptr,
                            llvm::Module *module,
                            llvm::LLVMContext &context) = 0;
};

class IncrementDataPointer : public Instruction {
public:
  void generateCode(llvm::IRBuilder<> &builder, llvm::Value *tape_ptr,
                    llvm::Module *, llvm::LLVMContext &context) override {
    llvm::Value *ptr = builder.CreateLoad(builder.getInt8Ty()->getPointerTo(),
                                          tape_ptr, "ptr");
    llvm::Value *new_ptr = builder.CreateInBoundsGEP(
        builder.getInt8Ty(), ptr, builder.getInt32(1), "ptr_inc");
    builder.CreateStore(new_ptr, tape_ptr);
  }
};

class DecrementDataPointer : public Instruction {
public:
  void generateCode(llvm::IRBuilder<> &builder, llvm::Value *tape_ptr,
                    llvm::Module *, llvm::LLVMContext &context) override {
    llvm::Value *ptr = builder.CreateLoad(builder.getInt8Ty()->getPointerTo(),
                                          tape_ptr, "ptr");
    llvm::Value *new_ptr = builder.CreateInBoundsGEP(
        builder.getInt8Ty(), ptr, builder.getInt32(-1), "ptr_dec");
    builder.CreateStore(new_ptr, tape_ptr);
  }
};

class IncrementByte : public Instruction {
public:
  void generateCode(llvm::IRBuilder<> &builder, llvm::Value *tape_ptr,
                    llvm::Module *, llvm::LLVMContext &context) override {
    llvm::Value *ptr = builder.CreateLoad(builder.getInt8Ty()->getPointerTo(),
                                          tape_ptr, "ptr");
    llvm::Value *val = builder.CreateLoad(builder.getInt8Ty(), ptr, "val");
    llvm::Value *inc = builder.CreateAdd(val, builder.getInt8(1), "inc");
    builder.CreateStore(inc, ptr);
  }
};

class DecrementByte : public Instruction {
public:
  void generateCode(llvm::IRBuilder<> &builder, llvm::Value *tape_ptr,
                    llvm::Module *, llvm::LLVMContext &context) override {
    llvm::Value *ptr = builder.CreateLoad(builder.getInt8Ty()->getPointerTo(),
                                          tape_ptr, "ptr");
    llvm::Value *val = builder.CreateLoad(builder.getInt8Ty(), ptr, "val");
    llvm::Value *dec = builder.CreateSub(val, builder.getInt8(1), "dec");
    builder.CreateStore(dec, ptr);
  }
};

class OutputByte : public Instruction {
public:
  void generateCode(llvm::IRBuilder<> &builder, llvm::Value *tape_ptr,
                    llvm::Module *module, llvm::LLVMContext &context) override {
    llvm::Function *putchar_func = module->getFunction("putchar");
    if (!putchar_func) {
      llvm::FunctionType *putchar_type = llvm::FunctionType::get(
          builder.getInt32Ty(), {builder.getInt32Ty()}, false);
      putchar_func = llvm::Function::Create(
          putchar_type, llvm::Function::ExternalLinkage, "putchar", module);
    }
    llvm::Value *ptr = builder.CreateLoad(builder.getInt8Ty()->getPointerTo(),
                                          tape_ptr, "ptr");
    llvm::Value *val = builder.CreateLoad(builder.getInt8Ty(), ptr, "val");
    llvm::Value *val_int32 =
        builder.CreateZExt(val, builder.getInt32Ty(), "val_int32");
    builder.CreateCall(putchar_func, val_int32);
  }
};

class InputByte : public Instruction {
public:
  void generateCode(llvm::IRBuilder<> &builder, llvm::Value *tape_ptr,
                    llvm::Module *module, llvm::LLVMContext &context) override {
    llvm::Function *getchar_func = module->getFunction("getchar");
    if (!getchar_func) {
      llvm::FunctionType *getchar_type =
          llvm::FunctionType::get(builder.getInt32Ty(), false);
      getchar_func = llvm::Function::Create(
          getchar_type, llvm::Function::ExternalLinkage, "getchar", module);
    }
    llvm::Value *ch = builder.CreateCall(getchar_func);
    llvm::Value *ch_int8 =
        builder.CreateTrunc(ch, builder.getInt8Ty(), "ch_int8");
    llvm::Value *ptr = builder.CreateLoad(builder.getInt8Ty()->getPointerTo(),
                                          tape_ptr, "ptr");
    builder.CreateStore(ch_int8, ptr);
  }
};

class Loop : public Instruction {
public:
  std::vector<std::unique_ptr<Instruction>> instructions;

  void generateCode(llvm::IRBuilder<> &builder, llvm::Value *tape_ptr,
                    llvm::Module *module, llvm::LLVMContext &context) override {
    llvm::Function *function = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock *loop_cond =
        llvm::BasicBlock::Create(context, "loop_cond", function);
    llvm::BasicBlock *loop_body =
        llvm::BasicBlock::Create(context, "loop_body", function);
    llvm::BasicBlock *loop_end =
        llvm::BasicBlock::Create(context, "loop_end", function);

    builder.CreateBr(loop_cond);

    // Loop condition
    builder.SetInsertPoint(loop_cond);
    llvm::Value *ptr = builder.CreateLoad(builder.getInt8Ty()->getPointerTo(),
                                          tape_ptr, "ptr");
    llvm::Value *val = builder.CreateLoad(builder.getInt8Ty(), ptr, "val");
    llvm::Value *cond =
        builder.CreateICmpNE(val, builder.getInt8(0), "loop_cond");
    builder.CreateCondBr(cond, loop_body, loop_end);

    // Loop body
    builder.SetInsertPoint(loop_body);
    for (const auto &instr : instructions) {
      instr->generateCode(builder, tape_ptr, module, context);
    }
    builder.CreateBr(loop_cond);

    // After loop
    builder.SetInsertPoint(loop_end);
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

  // Initialize LLVM
  llvm::LLVMContext context;
  llvm::Module module("brainfuck_module", context);
  llvm::IRBuilder<> builder(context);

  // Create main function
  llvm::FunctionType *main_type =
      llvm::FunctionType::get(builder.getInt32Ty(), false);
  llvm::Function *main_func = llvm::Function::Create(
      main_type, llvm::Function::ExternalLinkage, "main", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", main_func);
  builder.SetInsertPoint(entry);

  // Create tape
  llvm::ArrayType *tape_type = llvm::ArrayType::get(builder.getInt8Ty(), 30000);
  llvm::Value *tape = builder.CreateAlloca(tape_type, nullptr, "tape");
  builder.CreateMemSet(tape, builder.getInt8(0), 30000, llvm::MaybeAlign(1));

  // Create pointer to the tape (start at tape[0])
  llvm::Value *ptr = builder.CreateInBoundsGEP(
      tape_type, tape, {builder.getInt32(0), builder.getInt32(0)}, "ptr");
  llvm::Value *tape_ptr = builder.CreateAlloca(
      builder.getInt8Ty()->getPointerTo(), nullptr, "tape_ptr");
  builder.CreateStore(ptr, tape_ptr);

  for (const auto &instr : instructions) {
    instr->generateCode(builder, tape_ptr, &module, context);
  }

  // Return 0 at the end
  builder.CreateRet(builder.getInt32(0));

  // Verify the IR generated
  if (llvm::verifyModule(module, &llvm::errs())) {
    std::cerr << "Error: module verification failed\n";
    return 1;
  }

  module.print(llvm::outs(), nullptr);

  return 0;
}