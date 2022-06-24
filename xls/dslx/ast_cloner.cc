// Copyright 2022 The XLS Authors
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
#include "xls/dslx/ast_cloner.h"

#include "absl/status/status.h"
#include "xls/common/status/status_macros.h"
#include "xls/common/visitor.h"
#include "xls/dslx/ast.h"
#include "xls/dslx/ast_utils.h"

namespace xls::dslx {

// TODO(rspringer): 2022-06-06: Make sure all NameDef::definers are set
// appropriately.
// TODO(rspringer): 2022-06-06: Switch to AstNodeVisitor (without "WithDefault")
// once all nodes are supported.
class AstCloner : public AstNodeVisitorWithDefault {
 public:
  AstCloner(Module* module) : module_(module) {}

  absl::Status HandleArray(const Array* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<Expr*> new_members;
    for (const Expr* old_member : n->members()) {
      new_members.push_back(down_cast<Expr*>(old_to_new_.at(old_member)));
    }

    Array* array =
        module_->Make<Array>(n->span(), new_members, n->has_ellipsis());
    if (n->type_annotation() != nullptr) {
      array->set_type_annotation(
          down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation())));
    }
    old_to_new_[n] = array;
    return absl::OkStatus();
  }

  absl::Status HandleArrayTypeAnnotation(
      const ArrayTypeAnnotation* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    old_to_new_[n] = module_->Make<ArrayTypeAnnotation>(
        n->span(),
        down_cast<TypeAnnotation*>(old_to_new_.at(n->element_type())),
        down_cast<Expr*>(old_to_new_.at(n->dim())));
    return absl::OkStatus();
  }

  absl::Status HandleAttr(const Attr* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    // We must've seen the NameDef here, so we don't need to visit it.
    old_to_new_[n] = module_->Make<Attr>(
        n->span(), down_cast<Expr*>(old_to_new_.at(n->lhs())),
        down_cast<NameDef*>(old_to_new_.at(n->attr())));
    return absl::OkStatus();
  }

  absl::Status HandleBinop(const Binop* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<Binop>(
        n->span(), n->binop_kind(), down_cast<Expr*>(old_to_new_.at(n->lhs())),
        down_cast<Expr*>(old_to_new_.at(n->rhs())));
    return absl::OkStatus();
  }

  absl::Status HandleBuiltinNameDef(const BuiltinNameDef* n) override {
    if (!old_to_new_.contains(n)) {
      old_to_new_[n] = module_->Make<BuiltinNameDef>(n->identifier());
    }
    return absl::OkStatus();
  }

  absl::Status HandleBuiltinTypeAnnotation(
      const BuiltinTypeAnnotation* n) override {
    old_to_new_[n] =
        module_->Make<BuiltinTypeAnnotation>(n->span(), n->builtin_type());
    return absl::OkStatus();
  }

  absl::Status HandleCast(const Cast* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<Cast>(
        n->span(), down_cast<Expr*>(old_to_new_.at(n->expr())),
        down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation())));
    return absl::OkStatus();
  }

  absl::Status HandleChannelDecl(const ChannelDecl* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    absl::optional<std::vector<Expr*>> new_dims;
    if (n->dims().has_value()) {
      std::vector<Expr*> old_dims_vector = n->dims().value();
      std::vector<Expr*> new_dims_vector;
      new_dims_vector.reserve(old_dims_vector.size());
      for (const Expr* expr : old_dims_vector) {
        new_dims_vector.push_back(down_cast<Expr*>(old_to_new_.at(expr)));
      }
      new_dims = std::move(new_dims_vector);
    }

    old_to_new_[n] = module_->Make<ChannelDecl>(
        n->span(), down_cast<TypeAnnotation*>(old_to_new_.at(n->type())),
        new_dims);
    return absl::OkStatus();
  }

  absl::Status HandleChannelTypeAnnotation(
      const ChannelTypeAnnotation* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    absl::optional<std::vector<Expr*>> new_dims;
    if (n->dims().has_value()) {
      std::vector<Expr*> old_dims_vector = n->dims().value();
      std::vector<Expr*> new_dims_vector;
      new_dims_vector.reserve(old_dims_vector.size());
      for (const Expr* expr : old_dims_vector) {
        new_dims_vector.push_back(down_cast<Expr*>(old_to_new_.at(expr)));
      }
      new_dims = new_dims_vector;
    }

    old_to_new_[n] = module_->Make<ChannelTypeAnnotation>(
        n->span(), n->direction(),
        down_cast<TypeAnnotation*>(old_to_new_.at(n->payload())), new_dims);
    return absl::OkStatus();
  }

  absl::Status HandleColonRef(const ColonRef* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    ColonRef::Subject new_subject;
    if (std::holds_alternative<NameRef*>(n->subject())) {
      new_subject =
          down_cast<NameRef*>(old_to_new_.at(std::get<NameRef*>(n->subject())));
    } else {
      new_subject = down_cast<ColonRef*>(
          old_to_new_.at(std::get<ColonRef*>(n->subject())));
    }

    old_to_new_[n] = module_->Make<ColonRef>(n->span(), new_subject, n->attr());
    return absl::OkStatus();
  }

  absl::Status HandleConstantArray(const ConstantArray* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<Expr*> new_members;
    for (const Expr* old_member : n->members()) {
      new_members.push_back(down_cast<Expr*>(old_to_new_.at(old_member)));
    }
    ConstantArray* array =
        module_->Make<ConstantArray>(n->span(), new_members, n->has_ellipsis());
    if (n->type_annotation() != nullptr) {
      array->set_type_annotation(
          down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation())));
    }
    old_to_new_[n] = array;
    return absl::OkStatus();
  }

  absl::Status HandleConstantDef(const ConstantDef* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<ConstantDef>(
        n->span(), down_cast<NameDef*>(old_to_new_.at(n->name_def())),
        down_cast<Expr*>(old_to_new_.at(n->value())), n->is_public());
    return absl::OkStatus();
  }

  absl::Status HandleConstRef(const ConstRef* n) override {
    XLS_RETURN_IF_ERROR(n->name_def()->Accept(this));
    old_to_new_[n] = module_->Make<ConstRef>(
        n->span(), n->identifier(),
        down_cast<NameDef*>(old_to_new_.at(n->name_def())));
    return absl::OkStatus();
  }

  absl::Status HandleEnumDef(const EnumDef* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    NameDef* new_name_def = down_cast<NameDef*>(old_to_new_.at(n->name_def()));
    TypeAnnotation* new_type_annotation = nullptr;
    if (n->type_annotation() != nullptr) {
      new_type_annotation =
          down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation()));
    }

    std::vector<EnumMember> new_values;
    for (const auto& member : n->values()) {
      new_values.push_back(
          EnumMember{down_cast<NameDef*>(old_to_new_.at(member.name_def)),
                     down_cast<Expr*>(old_to_new_.at(member.value))});
    }

    EnumDef* new_enum_def =
        module_->Make<EnumDef>(n->span(), new_name_def, new_type_annotation,
                               new_values, n->is_public());
    new_name_def->set_definer(new_enum_def);
    old_to_new_[n] = new_enum_def;

    return absl::OkStatus();
  }

  absl::Status HandleFor(const For* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    old_to_new_[n] = module_->Make<For>(
        n->span(), down_cast<NameDefTree*>(old_to_new_.at(n->names())),
        down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation())),
        down_cast<Expr*>(old_to_new_.at(n->iterable())),
        down_cast<Expr*>(old_to_new_.at(n->body())),
        down_cast<Expr*>(old_to_new_.at(n->init())));
    return absl::OkStatus();
  }

  absl::Status HandleFormatMacro(const FormatMacro* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<Expr*> new_args;
    new_args.reserve(n->args().size());
    for (const Expr* arg : n->args()) {
      new_args.push_back(down_cast<Expr*>(old_to_new_.at(arg)));
    }

    old_to_new_[n] = module_->Make<FormatMacro>(n->span(), n->macro(),
                                                n->format(), new_args);
    return absl::OkStatus();
  }

  absl::Status HandleFunction(const Function* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    NameDef* new_name_def = down_cast<NameDef*>(old_to_new_.at(n->name_def()));

    std::vector<ParametricBinding*> new_parametric_bindings;
    new_parametric_bindings.reserve(n->parametric_bindings().size());
    for (const auto* pb : n->parametric_bindings()) {
      new_parametric_bindings.push_back(
          down_cast<ParametricBinding*>(old_to_new_.at(pb)));
    }

    std::vector<Param*> new_params;
    new_params.reserve(n->params().size());
    for (const auto* param : n->params()) {
      new_params.push_back(down_cast<Param*>(old_to_new_.at(param)));
    }

    TypeAnnotation* new_return_type = nullptr;
    if (n->return_type() != nullptr) {
      new_return_type =
          down_cast<TypeAnnotation*>(old_to_new_.at(n->return_type()));
    }
    old_to_new_[n] = module_->Make<Function>(
        n->span(), new_name_def, new_parametric_bindings, new_params,
        new_return_type, down_cast<Expr*>(old_to_new_.at(n->body())), n->tag(),
        n->is_public());
    new_name_def->set_definer(old_to_new_.at(n));
    return absl::OkStatus();
  }

  absl::Status HandleImport(const Import* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    old_to_new_[n] = module_->Make<Import>(
        n->span(), n->subject(),
        down_cast<NameDef*>(old_to_new_.at(n->name_def())), n->alias());
    return absl::OkStatus();
  }

  absl::Status HandleIndex(const Index* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    IndexRhs new_rhs = std::visit(
        Visitor{[&](Expr* expr) -> IndexRhs {
                  return down_cast<Expr*>(old_to_new_.at(expr));
                },
                [&](Slice* slice) -> IndexRhs {
                  return down_cast<Slice*>(old_to_new_.at(slice));
                },
                [&](WidthSlice* width_slice) -> IndexRhs {
                  return down_cast<WidthSlice*>(old_to_new_.at(width_slice));
                }},
        n->rhs());

    old_to_new_[n] = module_->Make<Index>(
        n->span(), down_cast<Expr*>(old_to_new_.at(n->lhs())), new_rhs);
    return absl::OkStatus();
  }

  absl::Status HandleInvocation(const Invocation* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<Expr*> new_args;
    new_args.reserve(n->args().size());
    for (const Expr* arg : n->args()) {
      new_args.push_back(down_cast<Expr*>(old_to_new_.at(arg)));
    }

    old_to_new_[n] = module_->Make<Invocation>(
        n->span(), down_cast<Expr*>(old_to_new_.at(n->callee())), new_args);
    return absl::OkStatus();
  }

  absl::Status HandleLet(const Let* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    TypeAnnotation* new_type = nullptr;
    if (n->type_annotation() != nullptr) {
      new_type =
          down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation()));
    }

    old_to_new_[n] = module_->Make<Let>(
        n->span(), down_cast<NameDefTree*>(old_to_new_.at(n->name_def_tree())),
        new_type, down_cast<Expr*>(old_to_new_.at(n->rhs())),
        down_cast<Expr*>(old_to_new_.at(n->body())), n->is_const());
    return absl::OkStatus();
  }

  absl::Status HandleMatch(const Match* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<MatchArm*> new_arms;
    new_arms.reserve(n->arms().size());
    for (const MatchArm* arm : n->arms()) {
      new_arms.push_back(down_cast<MatchArm*>(old_to_new_.at(arm)));
    }

    old_to_new_[n] = module_->Make<Match>(
        n->span(), down_cast<Expr*>(old_to_new_.at(n->matched())), new_arms);
    return absl::OkStatus();
  }

  absl::Status HandleMatchArm(const MatchArm* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<NameDefTree*> new_patterns;
    new_patterns.reserve(n->patterns().size());
    for (const NameDefTree* pattern : n->patterns()) {
      new_patterns.push_back(down_cast<NameDefTree*>(old_to_new_.at(pattern)));
    }

    old_to_new_[n] = module_->Make<MatchArm>(
        n->span(), new_patterns, down_cast<Expr*>(old_to_new_.at(n->expr())));

    return absl::OkStatus();
  }

  absl::Status HandleNameDef(const NameDef* n) override {
    if (!old_to_new_.contains(n)) {
      // We need to set the definer in the definer itself, not here, to avoid
      // looping (Function -> NameDef -> Function -> NameDef -> ...).
      old_to_new_[n] =
          module_->Make<NameDef>(n->span(), n->identifier(), nullptr);
    }

    return absl::OkStatus();
  }

  absl::Status HandleNameDefTree(const NameDefTree* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    if (n->is_leaf()) {
      NameDefTree::Leaf leaf;
      XLS_RETURN_IF_ERROR(std::visit(
          Visitor{
              [&](ColonRef* colon_ref) -> absl::Status {
                leaf = down_cast<ColonRef*>(old_to_new_.at(colon_ref));
                return absl::OkStatus();
              },
              [&](NameDef* name_def) -> absl::Status {
                leaf = down_cast<NameDef*>(old_to_new_.at(name_def));
                return absl::OkStatus();
              },
              [&](NameRef* name_ref) -> absl::Status {
                leaf = down_cast<NameRef*>(old_to_new_.at(name_ref));
                return absl::OkStatus();
              },
              [&](Number* number) -> absl::Status {
                leaf = down_cast<Number*>(old_to_new_.at(number));
                return absl::OkStatus();
              },
              [&](WildcardPattern* wp) -> absl::Status {
                leaf = down_cast<WildcardPattern*>(old_to_new_.at(wp));
                return absl::OkStatus();
              },
          },
          n->leaf()));
      old_to_new_[n] = module_->Make<NameDefTree>(n->span(), leaf);
      return absl::OkStatus();
    }

    NameDefTree::Nodes nodes;
    nodes.reserve(n->nodes().size());
    for (const auto& node : n->nodes()) {
      nodes.push_back(down_cast<NameDefTree*>(old_to_new_.at(node)));
    }
    old_to_new_[n] = module_->Make<NameDefTree>(n->span(), nodes);
    return absl::OkStatus();
  }

  absl::Status HandleNameRef(const NameRef* n) override {
    AnyNameDef any_name_def;
    if (std::holds_alternative<NameDef*>(n->name_def())) {
      auto* name_def = std::get<NameDef*>(n->name_def());
      XLS_RETURN_IF_ERROR(name_def->Accept(this));
      any_name_def = down_cast<NameDef*>(old_to_new_.at(name_def));
    } else {
      auto* builtin_name_def = std::get<BuiltinNameDef*>(n->name_def());
      XLS_RETURN_IF_ERROR(builtin_name_def->Accept(this));
      any_name_def =
          down_cast<BuiltinNameDef*>(old_to_new_.at(builtin_name_def));
    }

    old_to_new_[n] =
        module_->Make<NameRef>(n->span(), n->identifier(), any_name_def);
    return absl::OkStatus();
  }

  absl::Status HandleNumber(const Number* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    TypeAnnotation* new_type = nullptr;
    if (n->type_annotation() != nullptr) {
      new_type =
          down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation()));
    }
    old_to_new_[n] =
        module_->Make<Number>(n->span(), n->text(), n->number_kind(), new_type);
    return absl::OkStatus();
  }

  absl::Status HandleParam(const Param* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<Param>(
        down_cast<NameDef*>(old_to_new_.at(n->name_def())),
        down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation())));
    return absl::OkStatus();
  }

  absl::Status HandleParametricBinding(const ParametricBinding* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    Expr* new_expr = nullptr;
    if (n->expr() != nullptr) {
      new_expr = down_cast<Expr*>(old_to_new_.at(n->expr()));
    }
    old_to_new_[n] = module_->Make<ParametricBinding>(
        down_cast<NameDef*>(old_to_new_.at(n->name_def())),
        down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation())),
        new_expr);
    return absl::OkStatus();
  }

  absl::Status HandleProc(const Proc* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<ParametricBinding*> new_parametric_bindings;
    new_parametric_bindings.reserve(n->parametric_bindings().size());
    for (const ParametricBinding* pb : n->parametric_bindings()) {
      new_parametric_bindings.push_back(
          down_cast<ParametricBinding*>(old_to_new_.at(pb)));
    }

    std::vector<Param*> new_members;
    new_members.reserve(n->members().size());
    for (const Param* member : n->members()) {
      new_members.push_back(down_cast<Param*>(old_to_new_.at(member)));
    }

    NameDef* new_name_def = down_cast<NameDef*>(old_to_new_.at(n->name_def()));
    Proc* p = module_->Make<Proc>(
        n->span(), new_name_def,
        down_cast<NameDef*>(old_to_new_.at(n->config_name_def())),
        down_cast<NameDef*>(old_to_new_.at(n->next_name_def())),
        new_parametric_bindings, new_members,
        down_cast<Function*>(old_to_new_.at(n->config())),
        down_cast<Function*>(old_to_new_.at(n->next())), n->is_public());
    new_name_def->set_definer(p);
    old_to_new_[n] = p;
    return absl::OkStatus();
  }

  absl::Status HandleQuickCheck(const QuickCheck* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<QuickCheck>(
        n->GetSpan().value(), down_cast<Function*>(old_to_new_.at(n->f())),
        n->test_count());
    return absl::OkStatus();
  }

  absl::Status HandleRecv(const Recv* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<Recv>(
        n->span(), down_cast<NameRef*>(old_to_new_.at(n->token())),
        down_cast<Expr*>(old_to_new_.at(n->channel())));
    return absl::OkStatus();
  }

  absl::Status HandleRecvIf(const RecvIf* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<RecvIf>(
        n->span(), down_cast<NameRef*>(old_to_new_.at(n->token())),
        down_cast<Expr*>(old_to_new_.at(n->channel())),
        down_cast<Expr*>(old_to_new_.at(n->condition())));
    return absl::OkStatus();
  }

  absl::Status HandleSend(const Send* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<Send>(
        n->span(), down_cast<NameRef*>(old_to_new_.at(n->token())),
        down_cast<Expr*>(old_to_new_.at(n->channel())),
        down_cast<Expr*>(old_to_new_.at(n->payload())));
    return absl::OkStatus();
  }

  absl::Status HandleSendIf(const SendIf* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<SendIf>(
        n->span(), down_cast<NameRef*>(old_to_new_.at(n->token())),
        down_cast<Expr*>(old_to_new_.at(n->channel())),
        down_cast<Expr*>(old_to_new_.at(n->condition())),
        down_cast<Expr*>(old_to_new_.at(n->payload())));
    return absl::OkStatus();
  }

  absl::Status HandleSlice(const Slice* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<Slice>(
        n->GetSpan().value(), down_cast<Expr*>(old_to_new_.at(n->start())),
        down_cast<Expr*>(old_to_new_.at(n->limit())));
    return absl::OkStatus();
  }

  absl::Status HandleSpawn(const Spawn* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    XLS_RETURN_IF_ERROR(n->callee()->Accept(this));

    std::vector<Expr*> new_parametrics;
    new_parametrics.reserve(n->explicit_parametrics().size());
    for (const Expr* parametric : n->explicit_parametrics()) {
      new_parametrics.push_back(down_cast<Expr*>(old_to_new_.at(parametric)));
    }

    old_to_new_[n] = module_->Make<Spawn>(
        n->span(), down_cast<Expr*>(old_to_new_.at(n->callee())),
        down_cast<Invocation*>(old_to_new_.at(n->config())),
        down_cast<Invocation*>(old_to_new_.at(n->next())), new_parametrics,
        down_cast<Expr*>(old_to_new_.at(n->body())));
    return absl::OkStatus();
  }

  absl::Status HandleSplatStructInstance(
      const SplatStructInstance* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    // Have to explicitly visit struct def, since it's not a child.
    StructRef new_struct_ref;
    if (std::holds_alternative<StructDef*>(n->struct_ref())) {
      StructDef* old_struct_def = std::get<StructDef*>(n->struct_ref());
      XLS_RETURN_IF_ERROR(old_struct_def->Accept(this));
      new_struct_ref = down_cast<StructDef*>(old_to_new_.at(old_struct_def));
    } else {
      ColonRef* old_colon_ref = std::get<ColonRef*>(n->struct_ref());
      XLS_RETURN_IF_ERROR(old_colon_ref->Accept(this));
      new_struct_ref = down_cast<ColonRef*>(old_to_new_.at(old_colon_ref));
    }

    const std::vector<std::pair<std::string, Expr*>>& old_members =
        n->members();
    std::vector<std::pair<std::string, Expr*>> new_members;
    new_members.reserve(old_members.size());
    for (const auto& member : old_members) {
      new_members.push_back(std::make_pair(
          member.first, down_cast<Expr*>(old_to_new_.at(member.second))));
    }

    old_to_new_[n] = module_->Make<SplatStructInstance>(
        n->span(), new_struct_ref, new_members,
        down_cast<Expr*>(old_to_new_.at(n->splatted())));
    return absl::OkStatus();
  }

  absl::Status HandleString(const String* n) override {
    old_to_new_[n] = module_->Make<String>(n->span(), n->text());
    return absl::OkStatus();
  }

  absl::Status HandleStructDef(const StructDef* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<ParametricBinding*> new_parametric_bindings;
    new_parametric_bindings.reserve(n->parametric_bindings().size());
    for (const auto* pb : n->parametric_bindings()) {
      new_parametric_bindings.push_back(
          down_cast<ParametricBinding*>(old_to_new_.at(pb)));
    }

    std::vector<std::pair<NameDef*, TypeAnnotation*>> new_members;
    for (const std::pair<NameDef*, TypeAnnotation*>& member : n->members()) {
      new_members.push_back(std::make_pair(
          down_cast<NameDef*>(old_to_new_.at(member.first)),
          down_cast<TypeAnnotation*>(old_to_new_.at(member.second))));
    }

    old_to_new_[n] = module_->Make<StructDef>(
        n->span(), down_cast<NameDef*>(old_to_new_.at(n->name_def())),
        new_parametric_bindings, new_members, n->is_public());
    return absl::OkStatus();
  }

  absl::Status HandleStructInstance(const StructInstance* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    // Have to explicitly visit struct def, since it's not a child.
    StructRef new_struct_ref;
    if (std::holds_alternative<StructDef*>(n->struct_def())) {
      StructDef* old_struct_def = std::get<StructDef*>(n->struct_def());
      XLS_RETURN_IF_ERROR(old_struct_def->Accept(this));
      new_struct_ref = down_cast<StructDef*>(old_to_new_.at(old_struct_def));
    } else {
      ColonRef* old_colon_ref = std::get<ColonRef*>(n->struct_def());
      XLS_RETURN_IF_ERROR(old_colon_ref->Accept(this));
      new_struct_ref = down_cast<ColonRef*>(old_to_new_.at(old_colon_ref));
    }

    absl::Span<const std::pair<std::string, Expr*>> old_members =
        n->GetUnorderedMembers();
    std::vector<std::pair<std::string, Expr*>> new_members;
    new_members.reserve(old_members.size());
    for (const auto& member : old_members) {
      new_members.push_back(std::make_pair(
          member.first, down_cast<Expr*>(old_to_new_.at(member.second))));
    }

    old_to_new_[n] =
        module_->Make<StructInstance>(n->span(), new_struct_ref, new_members);
    return absl::OkStatus();
  }

  absl::Status HandleTernary(const Ternary* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    old_to_new_[n] = module_->Make<Ternary>(
        n->span(), down_cast<Expr*>(old_to_new_.at(n->test())),
        down_cast<Expr*>(old_to_new_.at(n->consequent())),
        down_cast<Expr*>(old_to_new_.at(n->alternate())));
    return absl::OkStatus();
  }

  absl::Status HandleTestFunction(const TestFunction* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    XLS_RETURN_IF_ERROR(n->fn()->Accept(this));
    old_to_new_[n] = module_->Make<TestFunction>(
        down_cast<Function*>(old_to_new_.at(n->fn())));
    return absl::OkStatus();
  }

  absl::Status HandleTestProc(const TestProc* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<Expr*> new_next_args;
    new_next_args.reserve(n->next_args().size());
    for (const Expr* next_arg : n->next_args()) {
      new_next_args.push_back(down_cast<Expr*>(old_to_new_.at(next_arg)));
    }

    old_to_new_[n] = module_->Make<TestProc>(
        down_cast<Proc*>(old_to_new_.at(n->proc())), new_next_args);
    return absl::OkStatus();
  }

  absl::Status HandleTupleTypeAnnotation(
      const TupleTypeAnnotation* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<TypeAnnotation*> new_members;
    new_members.reserve(n->members().size());
    for (const auto* member : n->members()) {
      new_members.push_back(down_cast<TypeAnnotation*>(old_to_new_.at(member)));
    }
    old_to_new_[n] = module_->Make<TupleTypeAnnotation>(n->span(), new_members);
    return absl::OkStatus();
  }

  absl::Status HandleTypeDef(const TypeDef* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    NameDef* new_name_def = down_cast<NameDef*>(old_to_new_.at(n->name_def()));
    TypeDef* new_td = module_->Make<TypeDef>(
        n->span(), new_name_def,
        down_cast<TypeAnnotation*>(old_to_new_.at(n->type_annotation())),
        n->is_public());
    new_name_def->set_definer(new_td);
    old_to_new_[n] = new_td;
    return absl::OkStatus();
  }

  absl::Status HandleTypeRef(const TypeRef* n) override {
    TypeDefinition new_type_definition;

    // A TypeRef doesn't own its referenced type definition, so we have to
    // explicitly visit it.
    XLS_RETURN_IF_ERROR(std::visit(
        Visitor{[&](ColonRef* colon_ref) -> absl::Status {
                  XLS_RETURN_IF_ERROR(colon_ref->Accept(this));
                  new_type_definition =
                      down_cast<ColonRef*>(old_to_new_.at(colon_ref));
                  return absl::OkStatus();
                },
                [&](EnumDef* enum_def) -> absl::Status {
                  XLS_RETURN_IF_ERROR(enum_def->Accept(this));
                  new_type_definition =
                      down_cast<EnumDef*>(old_to_new_.at(enum_def));
                  return absl::OkStatus();
                },
                [&](StructDef* struct_def) -> absl::Status {
                  XLS_RETURN_IF_ERROR(struct_def->Accept(this));
                  new_type_definition =
                      down_cast<StructDef*>(old_to_new_.at(struct_def));
                  return absl::OkStatus();
                },
                [&](TypeDef* type_def) -> absl::Status {
                  XLS_RETURN_IF_ERROR(type_def->Accept(this));
                  new_type_definition =
                      down_cast<TypeDef*>(old_to_new_.at(type_def));
                  return absl::OkStatus();
                }},
        n->type_definition()));

    old_to_new_[n] =
        module_->Make<TypeRef>(n->span(), n->text(), new_type_definition);
    return absl::OkStatus();
  }

  absl::Status HandleTypeRefTypeAnnotation(
      const TypeRefTypeAnnotation* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<Expr*> new_parametrics;
    new_parametrics.reserve(n->parametrics().size());
    for (const auto* parametric : n->parametrics()) {
      new_parametrics.push_back(down_cast<Expr*>(old_to_new_.at(parametric)));
    }

    old_to_new_[n] = module_->Make<TypeRefTypeAnnotation>(
        n->span(), down_cast<TypeRef*>(old_to_new_.at(n->type_ref())),
        new_parametrics);
    return absl::OkStatus();
  }

  absl::Status HandleUnop(const Unop* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] =
        module_->Make<Unop>(n->span(), n->unop_kind(),
                            down_cast<Expr*>(old_to_new_.at(n->operand())));
    return absl::OkStatus();
  }

  absl::Status HandleWidthSlice(const WidthSlice* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));
    old_to_new_[n] = module_->Make<WidthSlice>(
        n->GetSpan().value(), down_cast<Expr*>(old_to_new_.at(n->start())),
        down_cast<TypeAnnotation*>(old_to_new_.at(n->width())));
    return absl::OkStatus();
  }

  absl::Status HandleWildcardPattern(const WildcardPattern* n) {
    old_to_new_[n] = module_->Make<WildcardPattern>(n->span());
    return absl::OkStatus();
  }

  absl::Status HandleXlsTuple(const XlsTuple* n) override {
    XLS_RETURN_IF_ERROR(VisitChildren(n));

    std::vector<Expr*> members;
    members.reserve(n->members().size());
    for (const auto* member : n->members()) {
      members.push_back(down_cast<Expr*>(old_to_new_.at(member)));
    }

    old_to_new_[n] = module_->Make<XlsTuple>(n->span(), members);
    return absl::OkStatus();
  }

  const absl::flat_hash_map<const AstNode*, AstNode*>& old_to_new() {
    return old_to_new_;
  }

 private:
  // Visits all the children of the given node, skipping those that have
  // already been processed.
  absl::Status VisitChildren(const AstNode* node) {
    for (const auto& child : node->GetChildren(/*want_types=*/true)) {
      if (!old_to_new_.contains(child)) {
        XLS_RETURN_IF_ERROR(child->Accept(this));
      }
    }
    return absl::OkStatus();
  }

  Module* module_;
  absl::flat_hash_map<const AstNode*, AstNode*> old_to_new_;
};

absl::StatusOr<AstNode*> CloneAst(AstNode* root) {
  AstCloner cloner(root->owner());
  XLS_RETURN_IF_ERROR(root->Accept(&cloner));
  return cloner.old_to_new().at(root);
}

absl::StatusOr<std::unique_ptr<Module>> CloneModule(Module* module) {
  auto new_module = std::make_unique<Module>(module->name());
  AstCloner cloner(new_module.get());
  for (const ModuleMember member : module->top()) {
    ModuleMember new_member;
    XLS_RETURN_IF_ERROR(std::visit(
        Visitor{
            [&](Function* f) -> absl::Status {
              XLS_RETURN_IF_ERROR(f->Accept(&cloner));
              new_member = down_cast<Function*>(cloner.old_to_new().at(f));
              return absl::OkStatus();
            },
            [&](Proc* p) -> absl::Status {
              XLS_RETURN_IF_ERROR(p->Accept(&cloner));
              new_member = down_cast<Proc*>(cloner.old_to_new().at(p));
              return absl::OkStatus();
            },
            [&](TestFunction* tf) -> absl::Status {
              XLS_RETURN_IF_ERROR(tf->Accept(&cloner));
              new_member = down_cast<TestFunction*>(cloner.old_to_new().at(tf));
              return absl::OkStatus();
            },
            [&](TestProc* tp) -> absl::Status {
              XLS_RETURN_IF_ERROR(tp->Accept(&cloner));
              new_member = down_cast<TestProc*>(cloner.old_to_new().at(tp));
              return absl::OkStatus();
            },
            [&](QuickCheck* qc) -> absl::Status {
              XLS_RETURN_IF_ERROR(qc->Accept(&cloner));
              new_member = down_cast<QuickCheck*>(cloner.old_to_new().at(qc));
              return absl::OkStatus();
            },
            [&](TypeDef* td) -> absl::Status {
              XLS_RETURN_IF_ERROR(td->Accept(&cloner));
              new_member = down_cast<TypeDef*>(cloner.old_to_new().at(td));
              return absl::OkStatus();
            },
            [&](StructDef* sd) -> absl::Status {
              XLS_RETURN_IF_ERROR(sd->Accept(&cloner));
              new_member = down_cast<StructDef*>(cloner.old_to_new().at(sd));
              return absl::OkStatus();
            },
            [&](ConstantDef* cd) -> absl::Status {
              XLS_RETURN_IF_ERROR(cd->Accept(&cloner));
              new_member = down_cast<ConstantDef*>(cloner.old_to_new().at(cd));
              return absl::OkStatus();
            },
            [&](EnumDef* ed) -> absl::Status {
              XLS_RETURN_IF_ERROR(ed->Accept(&cloner));
              new_member = down_cast<EnumDef*>(cloner.old_to_new().at(ed));
              return absl::OkStatus();
            },
            [&](Import* i) -> absl::Status {
              XLS_RETURN_IF_ERROR(i->Accept(&cloner));
              new_member = down_cast<Import*>(cloner.old_to_new().at(i));
              return absl::OkStatus();
            },
        },
        member));
    XLS_RETURN_IF_ERROR(new_module->AddTop(new_member));
  }
  return new_module;
}

// Verifies that `node` consists solely of "new" AST nodes and none that are
// in the "old" set.
absl::Status VerifyClone(const AstNode* old_root, const AstNode* new_root) {
  absl::flat_hash_set<const AstNode*> old_nodes = FlattenToSet(old_root);
  absl::flat_hash_set<const AstNode*> new_nodes = FlattenToSet(new_root);
  for (const AstNode* new_node : new_nodes) {
    if (old_nodes.contains(new_node)) {
      absl::optional<Span> span = new_node->GetSpan();
      return absl::InvalidArgumentError(absl::StrFormat(
          "Node \"%s\" (%s; %s) was found in both the old set and "
          "new translation!",
          new_node->ToString(),
          span.has_value() ? span.value().ToString() : "<no span>",
          new_node->GetNodeTypeName()));
    }
  }
  return absl::OkStatus();
}

}  // namespace xls::dslx
