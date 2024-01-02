#include "include/jane_llvm.hpp"

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/InitializePasses.h>
#include <llvm/MC/SubtargetFeature.h>
#include <llvm/PassRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Scalar.h>

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
  std::string str = LLVMGetHostCPUName();
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

static void addAddDiscriminatorsPass(const PassManagerBuilder &builder,
                                     legacy::PassManagerBase &PM) {
  PM.add(createAddDiscriminatorsPass());
}

void LLVMJaneOptimizeModule(LLVMTargetMachineRef targ_machine_ref,
                            LLVMModuleRef module_ref) {
  TargetMachine *target_machine =
      reinterpret_cast<TargetMachine *>(targ_machine_ref);
  Module *module = unwrap(module_ref);
  TargetLibraryInfoImpl tlii(Triple(module->getTargetTriple()));

  PassManagerBuilder *PMBuilder = new PassManagerBuilder();
  PMBuilder->OptLevel = target_machine->getOptLevel();
  PMBuilder->SizeLevel = 0;
  PMBuilder->BBVectorize = true;
  PMBuilder->SLPVectorize = true;
  PMBuilder->LoopVectorize = true;

  PMBuilder->DisableUnitAtATime = false;
  PMBuilder->DisableUnrollLoops = false;
  PMBuilder->MergeFunctions = true;
  PMBuilder->PrepareForLTO = true;
  PMBuilder->RerollLoops = true;

  PMBuilder->addExtension(PassManagerBuilder::EP_EarlyAsPossible,
                          addAddDiscriminatorsPass);

  PMBuilder->LibraryInfo = &tlii;

  PMBuilder->Inliner =
      createFunctionInliningPass(PMBuilder->OptLevel, PMBuilder->SizeLevel);

  // Set up the per-function pass manager.
  legacy::FunctionPassManager *FPM = new legacy::FunctionPassManager(module);
  FPM->add(createTargetTransformInfoWrapperPass(
      target_machine->getTargetIRAnalysis()));
#ifndef NDEBUG
  bool verify_module = true;
#else
  bool verify_module = false;
#endif
  if (verify_module) {
    FPM->add(createVerifierPass());
  }
  PMBuilder->populateFunctionPassManager(*FPM);

  // Set up the per-module pass manager.
  legacy::PassManager *MPM = new legacy::PassManager();
  MPM->add(createTargetTransformInfoWrapperPass(
      target_machine->getTargetIRAnalysis()));

  PMBuilder->populateModulePassManager(*MPM);

  // run per function optimization passes
  FPM->doInitialization();
  for (Function &F : *module)
    if (!F.isDeclaration())
      FPM->run(F);
  FPM->doFinalization();

  // run per module optimization passes
  MPM->run(*module);
}

LLVMValueRef LLVMJaneBuildCall(LLVMBuilderRef B, LLVMValueRef Fn,
                               LLVMValueRef *Args, unsigned NumArgs,
                               unsigned CC, const char *Name) {
  CallInst *call_inst =
      CallInst::Create(unwrap(Fn), makeArrayRef(unwrap(Args), NumArgs), Name);
  call_inst->setCallingConv(CC);
  return wrap(unwrap(B)->Insert(call_inst));
}

LLVMJaneDIType *LLVMJaneCreateDebugPointerType(LLVMJaneDIBuilder *dibuilder,
                                               LLVMJaneDIType *pointee_type,
                                               uint64_t size_in_bits,
                                               uint64_t align_in_bits,
                                               const char *name) {
  DIType *di_type = reinterpret_cast<DIBuilder *>(dibuilder)->createPointerType(
      reinterpret_cast<DIType *>(pointee_type), size_in_bits, align_in_bits,
      name);
  return reinterpret_cast<LLVMJaneDIType *>(di_type);
}

LLVMJaneDIType *LLVMJaneCreateDebugBasicType(LLVMJaneDIBuilder *dibuilder,
                                             const char *name,
                                             uint64_t size_in_bits,
                                             uint64_t align_in_bits,
                                             unsigned encoding) {
  DIType *di_type = reinterpret_cast<DIBuilder *>(dibuilder)->createBasicType(
      name, size_in_bits, align_in_bits, encoding);
  return reinterpret_cast<LLVMJaneDIType *>(di_type);
}

unsigned LLVMJaneEncoding_DW_ATE_unsigned(void) {
  return dwarf::DW_ATE_unsigned;
}

unsigned LLVMJaneEncoding_DW_ATE_signed(void) { return dwarf::DW_ATE_signed; }

unsigned LLVMJaneLang_DW_LANG_C99(void) { return dwarf::DW_LANG_C99; }

LLVMJaneDIBuilder *LLVMJaneCreateDIBuilder(LLVMModuleRef module,
                                           bool allow_unresolved) {
  DIBuilder *di_builder =
      new DIBuilder(*llvm::unwrap(module), allow_unresolved);
  return reinterpret_cast<LLVMJaneDIBuilder *>(di_builder);
}

void LLVMJaneSetCurrentDebugLocation(LLVMBuilderRef builder, int line,
                                     int column, LLVMJaneDIScope *scope) {
  unwrap(builder)->SetCurrentDebugLocation(
      llvm::DebugLoc::get(line, column, reinterpret_cast<DIScope *>(scope)));
}

LLVMJaneDILexicalBlock *
LLVMJaneCreateLexicalBlock(LLVMJaneDIBuilder *dbuilder, LLVMJaneDIScope *scope,
                           LLVMJaneDIFile *file, unsigned line, unsigned col) {
  DILexicalBlock *result =
      reinterpret_cast<DIBuilder *>(dbuilder)->createLexicalBlock(
          reinterpret_cast<DIScope *>(scope), reinterpret_cast<DIFile *>(file),
          line, col);
  return reinterpret_cast<LLVMJaneDILexicalBlock *>(result);
}

LLVMJaneDIScope *
LLVMJaneLexicalBlockToScope(LLVMJaneDILexicalBlock *lexical_block) {
  DIScope *scope = reinterpret_cast<DILexicalBlock *>(lexical_block);
  return reinterpret_cast<LLVMJaneDIScope *>(scope);
}

LLVMJaneDIScope *
LLVMJaneCompileUnitToScope(LLVMJaneDICompileUnit *compile_unit) {
  DIScope *scope = reinterpret_cast<DICompileUnit *>(compile_unit);
  return reinterpret_cast<LLVMJaneDIScope *>(scope);
}

LLVMJaneDIScope *LLVMJaneFileToScope(LLVMJaneDIFile *difile) {
  DIScope *scope = reinterpret_cast<DIFile *>(difile);
  return reinterpret_cast<LLVMJaneDIScope *>(scope);
}

LLVMJaneDIScope *LLVMJaneSubprogramToScope(LLVMJaneDISubprogram *subprogram) {
  DIScope *scope = reinterpret_cast<DISubprogram *>(subprogram);
  return reinterpret_cast<LLVMJaneDIScope *>(scope);
}

LLVMJaneDICompileUnit *LLVMJaneCreateCompileUnit(
    LLVMJaneDIBuilder *dibuilder, unsigned lang, const char *file,
    const char *dir, const char *producer, bool is_optimized, const char *flags,
    unsigned runtime_version, const char *split_name, uint64_t dwo_id,
    bool emit_debug_info) {
  DICompileUnit *result =
      reinterpret_cast<DIBuilder *>(dibuilder)->createCompileUnit(
          lang, file, dir, producer, is_optimized, flags, runtime_version,
          split_name, DIBuilder::FullDebug, dwo_id, emit_debug_info);
  return reinterpret_cast<LLVMJaneDICompileUnit *>(result);
}

LLVMJaneDIFile *LLVMJaneCreateFile(LLVMJaneDIBuilder *dibuilder,
                                   const char *filename,
                                   const char *directory) {
  DIFile *result =
      reinterpret_cast<DIBuilder *>(dibuilder)->createFile(filename, directory);
  return reinterpret_cast<LLVMJaneDIFile *>(result);
}

LLVMJaneDISubprogram *
LLVMJaneCreateFunction(LLVMJaneDIBuilder *dibuilder, LLVMJaneDIScope *scope,
                       const char *name, const char *linkage_name,
                       LLVMJaneDIFile *file, unsigned lineno,
                       LLVMJaneDISubroutineType *ty, bool is_local_to_unit,
                       bool is_definition, unsigned scope_line, unsigned flags,
                       bool is_optimized, LLVMValueRef function) {
  llvm::Function *unwrapped_function =
      reinterpret_cast<llvm::Function *>(unwrap(function));
  DISubprogram *result =
      reinterpret_cast<DIBuilder *>(dibuilder)->createFunction(
          reinterpret_cast<DIScope *>(scope), name, linkage_name,
          reinterpret_cast<DIFile *>(file), lineno,
          reinterpret_cast<DISubroutineType *>(ty), is_local_to_unit,
          is_definition, scope_line, flags, is_optimized, unwrapped_function);
  return reinterpret_cast<LLVMJaneDISubprogram *>(result);
}

void LLVMJaneDIBuilderFinalize(LLVMJaneDIBuilder *dibuilder) {
  reinterpret_cast<DIBuilder *>(dibuilder)->finalize();
}