// Copyright 2024 The XLS Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef XLS_DSLX_TYPE_SYSTEM_DEDUCE_INVOCATION_H_
#define XLS_DSLX_TYPE_SYSTEM_DEDUCE_INVOCATION_H_

#include <functional>
#include <memory>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "xls/dslx/frontend/ast.h"
#include "xls/dslx/interp_value.h"
#include "xls/dslx/type_system/concrete_type.h"
#include "xls/dslx/type_system/deduce_ctx.h"
#include "xls/dslx/type_system/parametric_constraint.h"
#include "xls/dslx/type_system/type_and_parametric_env.h"

namespace xls::dslx {

absl::StatusOr<std::unique_ptr<ConcreteType>> DeduceInvocation(
    const Invocation* node, DeduceCtx* ctx);

// Helper that deduces the concrete types of the arguments to a parametric
// function or proc and returns them to the caller.
absl::Status InstantiateParametricArgs(
    const Instantiation* inst, const Expr* callee, absl::Span<Expr* const> args,
    DeduceCtx* ctx, std::vector<InstantiateArg>* instantiate_args);

// Generic function to do the heavy lifting of deducing the type of an
// Invocation or Spawn's constituent functions.
absl::StatusOr<TypeAndParametricEnv> DeduceInstantiation(
    DeduceCtx* ctx, const Invocation* invocation,
    const std::vector<InstantiateArg>& args,
    const std::function<absl::StatusOr<Function*>(const Instantiation*,
                                                  DeduceCtx*)>& resolve_fn,
    const absl::flat_hash_map<std::variant<const Param*, const ProcMember*>,
                              InterpValue>& constexpr_env);

absl::StatusOr<std::unique_ptr<ConcreteType>> DeduceFormatMacro(
    const FormatMacro* node, DeduceCtx* ctx);

absl::StatusOr<std::unique_ptr<ConcreteType>> DeduceZeroMacro(
    const ZeroMacro* node, DeduceCtx* ctx);

}  // namespace xls::dslx

#endif  // XLS_DSLX_TYPE_SYSTEM_DEDUCE_INVOCATION_H_
