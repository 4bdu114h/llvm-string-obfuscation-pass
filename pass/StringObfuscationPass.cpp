#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class StringObfuscationPass : public PassInfoMixin<StringObfuscationPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {

    errs() << "=== String Scan Started ===\n";

    for (GlobalVariable &GV : M.globals()) {

      if (!GV.hasInitializer())
        continue;

      Constant *Init = GV.getInitializer();

      auto *DataArray = dyn_cast<ConstantDataArray>(Init);

      if (!DataArray)
        continue;

      if (!DataArray->isString())
        continue;

      errs() << "Found string: ";

      errs() << DataArray->getAsString() << "\n";
    }

    return PreservedAnalyses::all();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "StringObfuscationPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "string-obfuscation") {
                    MPM.addPass(StringObfuscationPass());
                    return true;
                  }

                  return false;
                });
          }};
}