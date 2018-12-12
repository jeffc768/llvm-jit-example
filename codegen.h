#include "llvm/IR/Instructions.h"

#pragma once

struct Expr;
class JIT;

// Utility to translate AST to IR into the supplied LLVM function.
class Codegen {
public:
    Codegen(llvm::LLVMContext &ctx_, JIT &j, llvm::Function *f);

    // For functions, note the argument name.
    void setArgName(const std::string &name);

    // Do the translation.
    void translateFunction(Expr *e);

private:
    llvm::LLVMContext   &ctx;
    JIT                 &jit;
    llvm::BasicBlock    *bb;
    llvm::Function      *func;
    llvm::Type          *int32ty;

    std::string          argName;

    llvm::Value *translate(Expr *e);
    llvm::Value *doName(Expr *e);
    llvm::Value *doNumber(Expr *e);
    llvm::Value *doUnary(Expr *e);
    llvm::Value *doBinary(Expr *e, llvm::BinaryOperator::BinaryOps op);
    llvm::Value *doTernary(Expr *e);
    llvm::Value *doCmp(Expr *e, llvm::ICmpInst::Predicate op);
    llvm::Value *doCall(Expr *e);
    llvm::Value *doAssign(Expr *e);

    llvm::Value *getVarAddr(const std::string &name);
};
