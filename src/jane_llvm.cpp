#include "include/jane_llvm.hpp"
#include <llvm/InitializePasses.h>
#include <llvm/MC/SubtargetFeature.h>
#include <llvm/PassRegistry.h>

using namespace llvm;

void LLVMJaneInitializeLoopStrengthReducePass(LLVMPassRegistryRef R) {
  initializeLoopStrengthReducePass(*unwrap(R));
}

void LLVMJaneInitializeLowerIntrinsicsPass(LLVMPassRegistryRef R) {
  initializeLowerIntrinsicsPass(*unwrap(R));
}

void LLVMJaneInitializeUnreachableBlockElimPass(LLVMPassRegistryRef R) {
  initializeUnreachableMachineBlockElimPass(*unwrap(R));
}

char *LLVMJaneGetHostCPUName(void) {
  std::string str = sys::getHostCPUName();
  return strdup(str.c_str());
}

char *LLVMJaneGetNativeFeatures(void) {
  SubtargetFeatures features;
  StringMap<bool> host_features;
  if (sys::getHostCPUFeatures(host_features)) {
    for (auto &F : host_features) {
      features.AddFeature(F.first(), F.second);
    }
  }
  return strdup(features.getString().c_str());
}