#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/TargetSelect.h"
#include <iostream>
#include <string>

#include "AST.h"
#include "codegen.h"
#include "parser.h"
#include "JIT.h"

// References to stuff declared in flex and bison.
extern void setLexerInput(const std::string &line);
extern void clearLexerInput();
extern AST *yyparsetree;
int yyparse();

int main(int argc, char **argv) {
    // Have LLVM initialize itself.
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    // Create the top-level LLVM context.
    llvm::LLVMContext ctx;

    // Cache the LLVM type for 32-bit ints, as we use it a lot.
    llvm::Type *int32ty = llvm::Type::getInt32Ty(ctx);

    // Create the JIT engine.
    JIT jit;
    
    // Process command line arguments.
    while (argc > 1) {
        if (strcmp(argv[1], "--printIR") == 0) {
            jit.printIR = true;
        } else if (strcmp(argv[1], "--opt") == 0) {
            jit.optimize = true;
        } else {
            std::cout << "Unknown options: " << argv[1] << "\n";
            exit(1);
        }
        argc--, argv++;
    }

    // Infinite loop where we get a line of input, execute it, and print
    // the result.
    while (true) {
        // Create a module for holding our IR (once JITed, we can no longer
        // modify it, so we need to create new ones).
        auto module = llvm::make_unique<llvm::Module>("calc", ctx);
        llvm::Module *m = module.get();

        // Loop until we get a line that parses successfully.
        while (true) {
            std::string line;
            std::getline(std::cin, line);
            line += '\n';

            setLexerInput(line);
            int err = yyparse();
            clearLexerInput();

            // Note: on syntax error, any partially constructed parse tree
            // becomes leaked memory because the pointers exist only on an
            // internal bison stack--which is an array of a union type.
            if (!err && yyparsetree)
                break;
        }

        // We have a successfuly parse; now we do semantics.
        if (yyparsetree->lexeme == kw_fun) {
            // We have a function definition.
            Function *fun = static_cast<Function *>(yyparsetree);

            // Create an empty function in the module (the only function the
            // module will ever have)A  It has a single int32 argument, and
            // returns an int32.
            auto llvm_func = m->getOrInsertFunction(fun->name, int32ty, int32ty);
            llvm::Function *f = llvm::cast<llvm::Function>(llvm_func);

            // Lower the AST to IR.
            Codegen cg(ctx, jit, f);
            cg.setArgName(fun->arg);
            cg.translateFunction(fun->body.get());

            // Hand the module off to the JIT engine, which will immediately
            // lower it to machine code.  Note that the engine takes ownership
            // of the module--module will hold a null pointer after this
            // statement.
            jit.addOrReplaceFunction(fun->name, std::move(module));
        } else {
            // We have an expression.  Though technically not a function, it
            // still needs to be wrapped in one to add to a module.  This
            // function has no arguments, but still returns an int32.
            std::string name { "__dummy__" };
            auto llvm_func = m->getOrInsertFunction(name, int32ty);
            llvm::Function *f = llvm::cast<llvm::Function>(llvm_func);

            // Lower the AST to IR.
            Codegen cg(ctx, jit, f);
            cg.translateFunction(static_cast<Expr *>(yyparsetree));
            
            // Hand the module off to the JIT engine, which will pass a pointer
            // to the compiled function to the lambda; and when the lambda
            // returns, it'll immediately delete the module as it'll never be
            // executed again.
            jit.execute(std::move(module), name, [](JIT::cmd_t fp) {
                std::cout << "Result: " << fp() << "\n\n";
            });
        }

        // Free up the parse tree.
        delete yyparsetree;
        yyparsetree = nullptr;
    }

    // We never get here.
    return 0;
}
