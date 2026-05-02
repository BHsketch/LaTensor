#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h" // Added for PreservedAnalyses & PassInfoMixin
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "polly/RegisterPasses.h"

// Polly headers
#include "polly/ScopInfo.h"

using namespace llvm;

namespace {

// 1. Inherit from PassInfoMixin instead of FunctionPass
struct MyPollyScopPass : public PassInfoMixin<MyPollyScopPass> {
  
  // 2. Use the NPM run signature
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {    
    // 3. Get ScopInfo using the New Pass Manager Analysis Manager
    polly::ScopInfo &SI = FAM.getResult<polly::ScopInfoAnalysis>(F);

    if (SI.empty()) {
        errs() << "No SCoPs in function: " << F.getName() << "\n";
        return PreservedAnalyses::all();
    }

    errs() << "Function: " << F.getName() << "\n";

    for (auto &Entry : SI) {
      polly::Scop *S = Entry.second.get();
      errs() << "  SCoP: " << S->getNameStr() << "\n";

      for (polly::ScopStmt &Stmt : *S) {
        errs() << "    Stmt: " << Stmt.getBaseName() << "\n";

        isl::set Domain = Stmt.getDomain();
        char *DomainStr = isl_set_to_str(Domain.get());
        errs() << "      Loop Bounds (Domain): " << DomainStr << "\n";
        free(DomainStr); // You must free the C-string!
        
        for (Instruction *I : Stmt.getInstructions()) {
          errs() << "      Inst: " << *I << "\n";
        }

        for (polly::MemoryAccess *MA : Stmt) {
          errs() << "      MemAccess: ";
          MA->print(errs());
          errs() << "\n";

          if (MA->isWrite()) {
            Instruction *StoreI = MA->getAccessInstruction();
            errs() << "        Write: " << *StoreI << "\n";

            if (auto *StoreOp = dyn_cast<StoreInst>(StoreI)) {
              Value *RHS = StoreOp->getValueOperand();
              errs() << "        RHS: " << *RHS << "\n";
            }
          }
        }
      }
    }

    // 4. Return the analyses preserved (all of them, since we only read)
    return PreservedAnalyses::all();
  }
};

} // end anonymous namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "MyPollyScopPass", "0.1",
    [](llvm::PassBuilder &PB) {
      
      // THE MAGIC LINE: Manually register Polly's analyses with the PassBuilder
      polly::registerPollyPasses(PB);

      // Keep your existing explicit parsing setup
      PB.registerPipelineParsingCallback(
        [](llvm::StringRef Name,
           llvm::FunctionPassManager &FPM,
           llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
          if (Name == "my-polly-scop-pass") {
            FPM.addPass(MyPollyScopPass());
            return true;
          }
          return false;
        });
    }
  };
}