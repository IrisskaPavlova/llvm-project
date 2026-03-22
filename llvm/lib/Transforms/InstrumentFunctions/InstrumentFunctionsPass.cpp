#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"

using namespace llvm;

// Option 1: Function Instrumentation
class InstrumentFunctionsPass : public PassInfoMixin<InstrumentFunctionsPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        if (F.isDeclaration()) return PreservedAnalyses::all();
        Module *M = F.getParent();
        LLVMContext &Ctx = F.getContext();
        IRBuilder<> Builder(Ctx);
        FunctionType *VoidTy = FunctionType::get(Type::getVoidTy(Ctx), false);
        FunctionCallee StartFunc = M->getOrInsertFunction("instrument_start", VoidTy);
        FunctionCallee EndFunc = M->getOrInsertFunction("instrument_end", VoidTy);
        Builder.SetInsertPoint(&F.getEntryBlock(), F.getEntryBlock().getFirstInsertionPt());
        Builder.CreateCall(StartFunc);
        for (BasicBlock &BB : F) {
            if (auto *Ret = dyn_cast<ReturnInst>(BB.getTerminator())) {
                Builder.SetInsertPoint(Ret);
                Builder.CreateCall(EndFunc);
            }
        }
        errs() << "[Option 1] Instrumented: " << F.getName() << "\n";
        return PreservedAnalyses::all();
    }
};

// Option 2: Inlining Candidates
class InlineCandidatesPass : public PassInfoMixin<InlineCandidatesPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        bool Found = false;
        for (Function &F : M) {
            for (BasicBlock &BB : F) {
                for (Instruction &I : BB) {
                    if (auto *Call = dyn_cast<CallInst>(&I)) {
                        Function *Callee = Call->getCalledFunction();
                        if (Callee && !Callee->isDeclaration()) {
                            if (Callee->arg_size() == 0 && Callee->getReturnType()->isVoidTy()) {
                                errs() << "[Option 2] Inline candidate: " << Callee->getName()
                                       << " in " << F.getName() << "\n";
                                Found = true;
                            }
                        }
                    }
                }
            }
        }
        if (!Found) errs() << "[Option 2] No suitable functions\n";
        return PreservedAnalyses::all();
    }
};

// Option 3: Loop Wrapping
class LoopWrapperPass : public PassInfoMixin<LoopWrapperPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        if (F.isDeclaration()) return PreservedAnalyses::all();
        Module *M = F.getParent();
        LLVMContext &Ctx = F.getContext();
        LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
        if (LI.empty()) return PreservedAnalyses::all();
        FunctionType *VoidTy = FunctionType::get(Type::getVoidTy(Ctx), false);
        FunctionCallee LoopStart = M->getOrInsertFunction("loop_start", VoidTy);
        FunctionCallee LoopEnd = M->getOrInsertFunction("loop_end", VoidTy);
        for (Loop *L : LI) {
            errs() << "[Option 3] Loop in " << F.getName() << "\n";
            BasicBlock *Header = L->getHeader();
            IRBuilder<> Builder(Ctx);
            Builder.SetInsertPoint(Header->getFirstNonPHIIt());
            Builder.CreateCall(LoopStart);
            SmallVector<BasicBlock*, 8> ExitingBlocks;
            L->getExitingBlocks(ExitingBlocks);
            for (BasicBlock *Exiting : ExitingBlocks) {
                Builder.SetInsertPoint(Exiting->getTerminator());
                Builder.CreateCall(LoopEnd);
            }
        }
        return PreservedAnalyses::all();
    }
};

// Option 4: Mul to Shift Replacement
class MulToShiftPass : public PassInfoMixin<MulToShiftPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        if (F.isDeclaration()) return PreservedAnalyses::all();
        bool Changed = false;
        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *Mul = dyn_cast<BinaryOperator>(&I)) {
                    if (Mul->getOpcode() == Instruction::Mul) {
                        Value *LHS = Mul->getOperand(0);
                        Value *RHS = Mul->getOperand(1);
                        if (ConstantInt *ConstRHS = dyn_cast<ConstantInt>(RHS)) {
                            uint64_t Val = ConstRHS->getZExtValue();
                            if (Val > 0 && (Val & (Val - 1)) == 0) {
                                int ShiftAmount = 0;
                                uint64_t Temp = Val;
                                while (Temp >>= 1) ShiftAmount++;
                                IRBuilder<> Builder(Mul);
                                Value *Shift = Builder.CreateShl(LHS, ShiftAmount, "shift");
                                Mul->replaceAllUsesWith(Shift);
                                errs() << "[Option 4] Replaced: " << *Mul << " with shift by " << ShiftAmount << "\n";
                                Changed = true;
                            }
                        }
                        else if (ConstantInt *ConstLHS = dyn_cast<ConstantInt>(LHS)) {
                            uint64_t Val = ConstLHS->getZExtValue();
                            if (Val > 0 && (Val & (Val - 1)) == 0) {
                                int ShiftAmount = 0;
                                uint64_t Temp = Val;
                                while (Temp >>= 1) ShiftAmount++;
                                IRBuilder<> Builder(Mul);
                                Value *Shift = Builder.CreateShl(RHS, ShiftAmount, "shift");
                                Mul->replaceAllUsesWith(Shift);
                                errs() << "[Option 4] Replaced: " << *Mul << " with shift by " << ShiftAmount << "\n";
                                Changed = true;
                            }
                        }
                    }
                }
            }
        }
        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "InstrumentFunctionsPass", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "instrument-functions") {
                        FPM.addPass(InstrumentFunctionsPass());
                        return true;
                    }
                    if (Name == "loop-wrapper") {
                        FPM.addPass(LoopWrapperPass());
                        return true;
                    }
                    if (Name == "mul-to-shift") {
                        FPM.addPass(MulToShiftPass());
                        return true;
                    }
                    return false;
                });
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "inline-candidates") {
                        MPM.addPass(InlineCandidatesPass());
                        return true;
                    }
                    return false;
                });
        }
    };
}
