#ifndef JANE_LLVM
#define JANE_LLVM

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

void LLVMJaneInitializeLoopStrengthReducePass(LLVMPassRegistryRef R);
void LLVMJaneInitializeLowerIntrinsicsPass(LLVMPassRegistryRef R);
void LLVMJaneInitializeUnreachableBlockElimPass(LLVMPassRegistryRef R);

char *LLVMJaneGetHostCPUName(void);
char *LLVMJaneGetNativeFeatures(void);

#endif // JANE_LLVM