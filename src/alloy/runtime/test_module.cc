/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <alloy/runtime/test_module.h>

#include <alloy/compiler/compiler_passes.h>
#include <alloy/reset_scope.h>
#include <alloy/runtime/runtime.h>
#include <poly/platform.h>
#include <poly/string.h>

namespace alloy {
namespace runtime {

using alloy::backend::Backend;
using alloy::compiler::Compiler;
using alloy::hir::HIRBuilder;
using alloy::runtime::Function;
using alloy::runtime::FunctionInfo;
namespace passes = alloy::compiler::passes;

TestModule::TestModule(Runtime* runtime, const std::string& name,
                       std::function<bool(uint64_t)> contains_address,
                       std::function<bool(hir::HIRBuilder&)> generate)
    : Module(runtime),
      name_(name),
      contains_address_(contains_address),
      generate_(generate) {
  builder_.reset(new HIRBuilder());
  compiler_.reset(new Compiler(runtime));
  assembler_ = std::move(runtime->backend()->CreateAssembler());
  assembler_->Initialize();

  // Merge blocks early. This will let us use more context in other passes.
  // The CFG is required for simplification and dirtied by it.
  compiler_->AddPass(std::make_unique<passes::ControlFlowAnalysisPass>());
  compiler_->AddPass(std::make_unique<passes::ControlFlowSimplificationPass>());
  compiler_->AddPass(std::make_unique<passes::ControlFlowAnalysisPass>());

  // Passes are executed in the order they are added. Multiple of the same
  // pass type may be used.
  compiler_->AddPass(std::make_unique<passes::ContextPromotionPass>());
  compiler_->AddPass(std::make_unique<passes::SimplificationPass>());
  compiler_->AddPass(std::make_unique<passes::ConstantPropagationPass>());
  compiler_->AddPass(std::make_unique<passes::SimplificationPass>());
  // compiler_->AddPass(std::make_unique<passes::DeadStoreEliminationPass>());
  compiler_->AddPass(std::make_unique<passes::DeadCodeEliminationPass>());

  //// Removes all unneeded variables. Try not to add new ones after this.
  // compiler_->AddPass(new passes::ValueReductionPass());

  // Register allocation for the target backend.
  // Will modify the HIR to add loads/stores.
  // This should be the last pass before finalization, as after this all
  // registers are assigned and ready to be emitted.
  compiler_->AddPass(std::make_unique<passes::RegisterAllocationPass>(
      runtime->backend()->machine_info()));

  // Must come last. The HIR is not really HIR after this.
  compiler_->AddPass(std::make_unique<passes::FinalizationPass>());
}

TestModule::~TestModule() = default;

bool TestModule::ContainsAddress(uint64_t address) {
  return contains_address_(address);
}

SymbolInfo::Status TestModule::DeclareFunction(uint64_t address,
                                               FunctionInfo** out_symbol_info) {
  SymbolInfo::Status status = Module::DeclareFunction(address, out_symbol_info);
  if (status == SymbolInfo::STATUS_NEW) {
    auto symbol_info = *out_symbol_info;

    // Reset() all caching when we leave.
    make_reset_scope(compiler_);
    make_reset_scope(assembler_);

    if (!generate_(*builder_.get())) {
      symbol_info->set_status(SymbolInfo::STATUS_FAILED);
      return SymbolInfo::STATUS_FAILED;
    }

    compiler_->Compile(builder_.get());

    Function* fn = nullptr;
    assembler_->Assemble(symbol_info, builder_.get(), 0, nullptr, 0, &fn);

    symbol_info->set_function(fn);
    status = SymbolInfo::STATUS_DEFINED;
    symbol_info->set_status(status);
  }
  return status;
}

}  // namespace runtime
}  // namespace alloy
