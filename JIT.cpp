#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "JIT.h"
#include <iostream>

using namespace llvm;
using namespace llvm::orc;

// The constructor mostly initializes the ORC JIT engine.  It doesn't look
// pretty, but you don't really have to understand what it's doing unless
// you want to customize the layers.  Based off the Kaleidoscope tutorial.
JIT::JIT()
    : resolver(createLegacyLookupResolver(
          session,
          [this](const std::string &name) {
              return objectLayer.findSymbol(name, true);
          },
          [](Error err) {
              cantFail(std::move(err), "lookupFlags failed");
          })),
      target(EngineBuilder().selectTarget()),
      layout(target->createDataLayout()),
      objectLayer(session,
          [this](VModuleKey) {
              return ObjLayerT::Resources{
                  std::make_shared<SectionMemoryManager>(), resolver};
          }),
      compileLayer(objectLayer, SimpleCompiler(*target)) {
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

    // Also define two built-in functions, to show how easy it is.

    functionMap["pow2"] =
        std::make_pair(VModuleKey(), [](int arg) {
            return 1 << arg;
        });

    functionMap["abs"] =
        std::make_pair(VModuleKey(), [](int arg) {
            return arg < 0 ? -arg : arg;
        });
}

JIT::~JIT() {
    // This is probably done by the compile layer's destructor, but it doesn't
    // hurt to do this here: delete all the function modules and free up the
    // resources they consume.
    for (auto &kv : functionMap) {
        funcdesc_t &fd = kv.second;
        if (fd.first)
            cantFail(compileLayer.removeModule(fd.first));
    }
}

int *JIT::getOrAddVariable(const std::string &name) {
    return &variableMap[name];
}

static void AddOptimizations(Module *module) {
    // Pick a few optimization passes.  PassManagerBuilder can be used to
    // select the set of (many!) passes used by clang's -O1/-O2/-O3.
    legacy::FunctionPassManager fpm(module);
    fpm.add(createInstructionCombiningPass());
    fpm.add(createReassociatePass());
    fpm.add(createGVNPass());
    fpm.add(createCFGSimplificationPass());

    // Run passes on all functions (there's really only one) in the module.
    fpm.doInitialization();
    for (auto &f : *module)
        fpm.run(f);
    fpm.doFinalization();
}

void JIT::addOrReplaceFunction(const std::string &name,
                               std::unique_ptr<Module> module) {
    if (optimize)
        AddOptimizations(module.get());

    if (printIR)
        errs() << *module;

    // This JITs the module and its contents.
    auto key = session.allocateVModule();
    cantFail(compileLayer.addModule(key, std::move(module)));

    // If redefining the function, first delete the old JITed module before
    // re-adding it.
    funcdesc_t &fd = functionMap[name];
    if (fd.first) {
        cantFail(compileLayer.removeModule(fd.first));
    } else if (fd.second) {
        std::cout << "Can't replace built-in function " << name << "!\n";
        return;
    }

    // Query the function's entry point and add it to the map.
    fd.first = key;
    fd.second = (func_t)findSymbol(key, name);
}

JIT::func_t *JIT::getFunction(const std::string &name) {
    return &functionMap[name].second;
}

void JIT::execute(std::unique_ptr<Module> module,
                  const std::string &name,
                  void (*lambda)(cmd_t cmd)) {
    if (optimize)
        AddOptimizations(module.get());

    if (printIR)
        errs() << *module;

    // JIT the command.
    auto key = session.allocateVModule();
    cantFail(compileLayer.addModule(key, std::move(module)));

    // Pass it to the lambda.
    auto f = (cmd_t)findSymbol(key, name);
    lambda(f);

    // Delete the JITed module.
    cantFail(compileLayer.removeModule(key));
}

intptr_t JIT::findSymbol(VModuleKey modkey, const std::string &name) {
    auto sym = compileLayer.findSymbolIn(modkey, name, true);
    return (intptr_t)llvm::cantFail(sym.getAddress());
}
