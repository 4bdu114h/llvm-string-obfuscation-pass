#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

using namespace llvm;

namespace {

std::vector<uint8_t> encryptString(StringRef Str) {
  std::vector<uint8_t> Result;

  uint8_t Key[] = {0x12, 0x34, 0x56};

  for (size_t i = 0; i < Str.size(); i++) {
    Result.push_back(static_cast<uint8_t>(Str[i]) ^ Key[i % 3]);
  }

  return Result;
}

class StringObfuscationPass : public PassInfoMixin<StringObfuscationPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {

    errs() << "=== String Scan Started ===\n";

    LLVMContext &Ctx = M.getContext();

    Type *PtrTy = PointerType::get(Ctx, 0);

    FunctionType *DecodeTy =
        FunctionType::get(PtrTy, {PtrTy}, false);

    FunctionCallee DecodeFunc =
        M.getOrInsertFunction("decode", DecodeTy);

    for (GlobalVariable &GV : M.globals()) {

      if (!GV.hasInitializer())
        continue;

      Constant *Init = GV.getInitializer();

      auto *DataArray = dyn_cast<ConstantDataArray>(Init);

      if (!DataArray)
        continue;

      if (!DataArray->isString())
        continue;

      bool HasInstructionUser = false;

      for (User *U : GV.users()) {
        if (isa<Instruction>(U)) {
          HasInstructionUser = true;
          break;
        }
      }

      if (!HasInstructionUser) {
        errs() << "Skipping aggregate-only string: "
               << GV.getName() << "\n";
        continue;
      }

      StringRef Original = DataArray->getAsString();

      auto Encrypted = encryptString(Original);

      Constant *NewInitializer =
          ConstantDataArray::get(Ctx, Encrypted);

      GV.setInitializer(NewInitializer);

      errs() << "Replaced string: "
             << GV.getName() << "\n";

      for (User *U : GV.users()) {

        auto *Inst = dyn_cast<Instruction>(U);

        if (!Inst)
          continue;

        IRBuilder<> Builder(Inst);

        Value *StrPtr =
            Builder.CreateBitCast(&GV, PtrTy);

        Value *Decoded =
            Builder.CreateCall(
                DecodeFunc,
                {StrPtr},
                "decoded_str");

        if (auto *Call = dyn_cast<CallInst>(Inst)) {

          for (unsigned i = 0;
               i < Call->arg_size();
               i++) {

            if (Call->getArgOperand(i) == &GV) {

              Call->setArgOperand(i, Decoded);
            }
          }
        }
      }
    }

    return PreservedAnalyses::all();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK
PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION,
      "StringObfuscationPass",
      LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
            [](StringRef Name,
               ModulePassManager &MPM,
               ArrayRef<PassBuilder::PipelineElement>) {

              if (Name == "string-obfuscation") {
                MPM.addPass(StringObfuscationPass());
                return true;
              }

              return false;
            });
      }};
}