#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"
#include <map>
#include <memory>
#include <string>

#pragma once

class JIT {
    using ObjLayerT = llvm::orc::RTDyldObjectLinkingLayer;
    using CompileLayerT = llvm::orc::IRCompileLayer<ObjLayerT, llvm::orc::SimpleCompiler>;

public:
    using func_t = int (*)(int);    // Signature of calculator function
    using cmd_t = int (*)();        // Signature of calculator command

    JIT();
    ~JIT();

    // Return the address of the named calculator variable's value.
    int *getOrAddVariable(const std::string &name);

    // Add or replace a calculator function to the JIT engine.
    void addOrReplaceFunction(const std::string &name,
                              std::unique_ptr<llvm::Module> module);

    // Retrieve the address of where the named calculator function's address
    // is located.  This indirection allows functions to always use the latest
    // version of other functions.
    func_t *getFunction(const std::string &name);

    // Do a one-time execution of a function.  Pass the JITed code to the
    // lambda, and delete the JITed module afterwards.
    void execute(std::unique_ptr<llvm::Module> module,
                 const std::string &name,
                 void (*lambda)(cmd_t cmd));

    // If true, print the IR of each compiled function or command.
    bool    printIR = false;

    // If true, run a few optimization passes on the IR.
    bool    optimize = false;

private:
    intptr_t findSymbol(llvm::orc::VModuleKey modkey, const std::string &name);

    // Declare the layers of the ORC JIT engine.  Based off the Kaleidoscope
    // tutorial.
    llvm::orc::ExecutionSession                   session;
    std::shared_ptr<llvm::orc::SymbolResolver>    resolver;
    std::unique_ptr<llvm::TargetMachine>          target;
    const llvm::DataLayout                        layout;
    ObjLayerT                                     objectLayer;
    CompileLayerT                                 compileLayer;

    // The map that tracks the values of all the variables defined within
    // the calculator.
    std::map<const std::string, int>              variableMap;

    // The map that tracks all the functions defined within the calculator.
    using funcdesc_t = std::pair<llvm::orc::VModuleKey, func_t>;
    std::map<const std::string, funcdesc_t> functionMap;
};
