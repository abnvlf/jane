#ifndef JANE_LLVM
#define JANE_LLVM

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

struct LLVMJaneDIType;
struct LLVMJaneDIBuilder;
struct LLVMJaneDICompileUnit;
struct LLVMJaneDIScope;
struct LLVMJaneDIFile;
struct LLVMJaneDILexicalBlock;
struct LLVMJaneDISubprogram;
struct LLVMJaneDISubroutineType;

void LLVMJaneInitializeLoopStrengthReducePass(LLVMPassRegistryRef R);
void LLVMJaneInitializeLowerIntrinsicsPass(LLVMPassRegistryRef R);
void LLVMJaneInitializeUnreachableBlockElimPass(LLVMPassRegistryRef R);

char *LLVMJaneGetHostCPUName(void);
char *LLVMJaneGetNativeFeatures(void);

void LLVMJaneOptimizeModule(LLVMTargetMachineRef targ_machine_ref,
                            LLVMModuleRef module_ref);

LLVMValueRef LLVMJaneBuildCall(LLVMBuilderRef B, LLVMValueRef Fn,
                               LLVMValueRef *Args, unsigned NumArgs,
                               unsigned CC, const char *Name);

LLVMJaneDIType *LLVMJaneCreateDebugPointerType(LLVMJaneDIBuilder *dibuilder,
                                               LLVMJaneDIType *pointee_type,
                                               uint64_t size_in_bits,
                                               uint64_t align_in_bits,
                                               const char *name);

LLVMJaneDIType *LLVMJaneCreateDebugPointerType(LLVMJaneDIBuilder *dibuilder,
                                               LLVMJaneDIType *pointee_type,
                                               uint64_t size_in_bits,
                                               uint64_t align_in_bits,
                                               const char *name);

LLVMJaneDIType *LLVMJaneCreateDebugBasicType(LLVMJaneDIBuilder *dibuilder,
                                             const char *name,
                                             uint64_t size_in_bits,
                                             uint64_t align_in_bits,
                                             unsigned encoding);

unsigned LLVMJaneEncoding_DW_ATE_unsigned(void);
unsigned LLVMJaneEncoding_DW_ATE_signed(void);
unsigned LLVMJaneLang_DW_LANG_C99(void);

LLVMJaneDIBuilder *LLVMJaneCreateDIBuilder(LLVMModuleRef module,
                                           bool allow_unresolved);

void LLVMJaneSetCurrentDebugLocation(LLVMBuilderRef builder, int line,
                                     int column, LLVMJaneDIScope *scope);

LLVMJaneDIScope *
LLVMJaneLexicalBlockToScope(LLVMJaneDILexicalBlock *lexical_block);
LLVMJaneDIScope *
LLVMJaneCompileUnitToScope(LLVMJaneDICompileUnit *compile_unit);
LLVMJaneDIScope *LLVMJaneFileToScope(LLVMJaneDIFile *difile);
LLVMJaneDIScope *LLVMJaneSubprogramToScope(LLVMJaneDISubprogram *subprogram);

LLVMJaneDILexicalBlock *LLVMJaneCreateLexicalBlock(LLVMJaneDIBuilder *dbuilder,
                                                   LLVMJaneDIScope *scope,
                                                   LLVMJaneDIFile *file,
                                                   unsigned line, unsigned col);

LLVMJaneDICompileUnit *LLVMJaneCreateCompileUnit(
    LLVMJaneDIBuilder *dibuilder, unsigned lang, const char *file,
    const char *dir, const char *producer, bool is_optimized, const char *flags,
    unsigned runtime_version, const char *split_name, uint64_t dwo_id,
    bool emit_debug_info);

LLVMJaneDIFile *LLVMJaneCreateFile(LLVMJaneDIBuilder *dibuilder,
                                   const char *filename, const char *directory);

LLVMJaneDISubprogram *
LLVMJaneCreateFunction(LLVMJaneDIBuilder *dibuilder, LLVMJaneDIScope *scope,
                       const char *name, const char *linkage_name,
                       LLVMJaneDIFile *file, unsigned lineno,
                       LLVMJaneDISubroutineType *ty, bool is_local_to_unit,
                       bool is_definition, unsigned scope_line, unsigned flags,
                       bool is_optimized, LLVMValueRef function);

void LLVMJaneDIBuilderFinalize(LLVMJaneDIBuilder *dibuilder);

#endif // JANE_LLVM