#ifndef JANE_ANALYZE
#define JANE_ANALYZE

#include "semantic_info.hpp"

struct CodeGen;
struct ImportTableEntry;
void semantic_analyze(CodeGen *g, ImportTableEntry *entry);

#endif // JANE_ANALYZE