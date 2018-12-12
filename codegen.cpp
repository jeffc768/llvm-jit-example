#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/BasicBlock.h"
#include <string>

#include "codegen.h"
#include "AST.h"
#include "parser.h"
#include "JIT.h"

using namespace llvm;

Codegen::Codegen(LLVMContext &ctx_, JIT &j, llvm::Function *f)
      : ctx(ctx_), jit(j), func(f) {
    // Create function's entry block.
    bb = BasicBlock::Create(ctx, "", f);

    // Cache heavily used LLVM type for int32.
    int32ty = Type::getInt32Ty(ctx);
}

void Codegen::setArgName(const std::string &name) {
    argName = name;
}

void Codegen::translateFunction(Expr *body) {
    // Start translation at the root of the AST.
    Value *code = translate(body);

    // If the value is a bool, cast it to an int32 to make the return
    // instruction happy.
    if (code->getType() != int32ty)
        code = CastInst::Create(Instruction::ZExt, code,
                int32ty, "", bb);

    // Append return to the current (and final) basic block.
    ReturnInst::Create(ctx, code, bb);
}

Value *Codegen::translate(Expr *e) {
    switch (e->lexeme) {
        case t_name:
            return doName(e);
        case t_number:
            return doNumber(e);
        case '+':
            return doBinary(e, BinaryOperator::Instruction::Add);
        case '-':
            return doBinary(e, BinaryOperator::Instruction::Sub);
        case '*':
            return doBinary(e, BinaryOperator::Instruction::Mul);
        case '/':
            return doBinary(e, BinaryOperator::Instruction::SDiv);
        case '%':
            return doBinary(e, BinaryOperator::Instruction::SRem);
        case '<':
            return doCmp(e, ICmpInst::ICMP_SLT);
        case '>':
            return doCmp(e, ICmpInst::ICMP_SGT);
        case op_eq:
            return doCmp(e, ICmpInst::ICMP_EQ);
        case op_ne:
            return doCmp(e, ICmpInst::ICMP_NE);
        case op_le:
            return doCmp(e, ICmpInst::ICMP_SLE);
        case op_ge:
            return doCmp(e, ICmpInst::ICMP_SGE);
        case '|':
            return doBinary(e, BinaryOperator::Instruction::Or);
        case '&':
            return doBinary(e, BinaryOperator::Instruction::And);
        case '^':
            return doBinary(e, BinaryOperator::Instruction::Xor);
        case '!':
        case '~':
        case op_neg:
            return doUnary(e);
        case '?':
            return doTernary(e);
        case '=':
            return doAssign(e);
        case '(':
            return doCall(e);
    }

    // Should never reach here.
    return nullptr;
}

Value *Codegen::doName(Expr *e) {
    // If the name is the same as the function argument, return that as
    // the value.
    Name *n = static_cast<Name *>(e);
    if (n->value == argName)
        return &*func->arg_begin();

    // Otherwise, get the global's address from the JIT and deference it.
    return new LoadInst(getVarAddr(n->value), "" ,bb);
}

Value *Codegen::doNumber(Expr *e) {
    return ConstantInt::get(int32ty, static_cast<Number *>(e)->value);
}

Value *Codegen::doUnary(Expr *e) {
    Operator *op = static_cast<Operator *>(e);
    Value *arg1 = translate(op->arg1.get());
    if (op->lexeme == '~' || op->lexeme == '!')
        return BinaryOperator::CreateNot(arg1, "", bb);
    if (op->lexeme == op_neg)
        return BinaryOperator::CreateNeg(arg1, "", bb);
    return nullptr;
}

Value *Codegen::doBinary(Expr *e, BinaryOperator::BinaryOps binop) {
    Operator *op = static_cast<Operator *>(e);
    Value *arg1 = translate(op->arg1.get());
    Value *arg2 = translate(op->arg2.get());
    return BinaryOperator::Create(binop, arg1, arg2, "", bb);
}

Value *Codegen::doTernary(Expr *e) {
    // The ? : operator does conditional execution, requiring multiple
    // basic blocks: one for the true case, one for the false case, and
    // one to merge flow of control back together.
    Operator *op = static_cast<Operator *>(e);
    BasicBlock *bbTrue = BasicBlock::Create(ctx, "", func);
    BasicBlock *bbFalse = BasicBlock::Create(ctx, "", func);
    BasicBlock *bbMerge = BasicBlock::Create(ctx, "", func);

    // Translate the condition in the current basic block.  End the block
    // with a condition branch to the true and false cases.
    Value *cond = translate(op->arg1.get());
    BranchInst::Create(bbTrue, bbFalse, cond, bb);

    // Translate the true case in the true block.  End the block with an
    // unconditional branch to the merge block.
    bb = bbTrue;
    Value *ifTrue = translate(op->arg2.get());
    BranchInst::Create(bbMerge, bb);

    // Translate the false case in the true block.  End the block with an
    // unconditional branch to the merge block.
    bb = bbFalse;
    Value *ifFalse = translate(op->arg3.get());
    BranchInst::Create(bbMerge, bb);

    // The merge block becomes the current basic block going forwards.  A
    // phi node merges the values produced by the true and false cases into
    // a single value; that value is returned as the value of the ? :
    // operator.
    //
    // Note that the type of value being merged comes from the true case; it
    // could be either int32 or int1 (bool).  LLVM will assert if the false
    // case has a different type; a proper compiler would do type checking here
    // and not rely on LLVM to catch it--since it WON'T handle it gracefully,
    // assuming LLVM asserts are enabled at all.  (This is also true for
    // binary operators.)
    bb = bbMerge;
    PHINode *pn = PHINode::Create(ifTrue->getType(), 2, "", bb);
    pn->addIncoming(ifTrue, bbTrue);
    pn->addIncoming(ifFalse, bbFalse);
    return pn;
}

Value *Codegen::doCmp(Expr *e, ICmpInst::Predicate binop) {
    Operator *op = static_cast<Operator *>(e);
    Value *arg1 = translate(op->arg1.get());
    Value *arg2 = translate(op->arg2.get());

    // Note that this instruction producesd an int1 (bool) value.
    return CmpInst::Create(Instruction::ICmp, binop, arg1, arg2, "", bb);
}

Value *Codegen::doCall(Expr *e) {
    Operator *op = static_cast<Operator *>(e);
    auto& name = static_cast<Name *>(op->arg1.get())->value;

    Value *addr;

    if (name == func->getName()) {
        // Special case recursion.  An LLVM function is also a value, so it
        // can be the target of a call.
        addr = func;
    } else {
        // Form the LLVM types of the pointer to function, the equivalent of
        // int (*)(int).
        Type * formals[1] { int32ty };
        auto ft = FunctionType::get(int32ty, formals, false);
        Type *pft = PointerType::get(ft, 0);

        // and int (**)(int).
        Type *ppft = PointerType::get(pft, 0);

        // Get the address of the function address from the JIT.  Note that
        // its type, equivalent to intptr_t, is not portable!
        Type *addrTy = sizeof(void *) == 4 ? int32ty : Type::getInt64Ty(ctx);
        auto funcaddr = jit.getFunction(name);
        addr = ConstantInt::get(addrTy, intptr_t(funcaddr));

        // Now cast that intptr_t to int (**)(int).
        addr = new BitCastInst(addr, ppft, "", bb);

        // And dereference, yielding the pointer to function.
        addr = new LoadInst(addr, "", bb);
    }

    // Build the function argument list and call.
    Value *args[1] { translate(op->arg2.get()) };
    Value *v = CallInst::Create(addr, args, "", bb);

    return v;
}

Value *Codegen::doAssign(Expr *e) {
    Operator *op = static_cast<Operator *>(e);
    Name *lhs = static_cast<Name *>(op->arg1.get());
    Value *rhs = translate(op->arg2.get());
    Value *addr = getVarAddr(lhs->value);

    // Note that a store produces no value.
    new StoreInst(rhs, addr, false, bb);

    // The value of the assignment is the value just assigned.
    return rhs;
}

Value *Codegen::getVarAddr(const std::string &name) {
    // Get the LLVM type for intptr_t.
    Type *addrTy = sizeof(void *) == 4 ? int32ty
                                       : Type::getInt64Ty(ctx);

    // And the LLVM type for intptr_t*.
    Type *pint32ty = PointerType::get(int32ty, 0);

    // Get the address of the global variable.
    int *varaddr = jit.getOrAddVariable(name);
    
    // Construct the intptr_t LLVM value for it.
    Value *addr = ConstantInt::get(addrTy, intptr_t(varaddr));

    // Cast to int* and return.
    return new BitCastInst(addr, pint32ty, "", bb);
}
