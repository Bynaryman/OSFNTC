#include <ATen/core/Dict.h>
#include <ATen/core/Tensor.h>
#include <ATen/core/function_schema.h>
#include <ATen/core/jit_type.h>
#include <c10/macros/Macros.h>
#include <c10/util/irange.h>
#include <ATen/core/grad_mode.h>
#include <ATen/core/function.h>
#include <iostream>

namespace c10 {

static_assert(
    std::is_same<decltype(getTypePtr<std::tuple<int64_t, int64_t>>()), const TupleTypePtr&>::value,
    "getTypePtr<std::tuple<int64_t, int64_t>> not returning const ref!");

namespace {
inline bool is_contiguous_strides(
    const IntArrayRef sizes,
    const IntArrayRef strides) {
  int n_dim = static_cast<int>(sizes.size());

  if (n_dim == 0 || strides[n_dim-1] != 1) {
    return false;
  }

  for (int i = n_dim - 2; i >= 0; i--) {
    if (strides[i] != strides[i+1] * sizes[i+1]) {
      return false;
    }
  }
  return true;
}

} // namespace

TypeVerbosity type_verbosity() {
  static const char* c_verbosity = std::getenv("PYTORCH_JIT_TYPE_VERBOSITY");
  static TypeVerbosity verbosity = c_verbosity ?
    static_cast<TypeVerbosity>(c10::stoi(c_verbosity)) : TypeVerbosity::Default;
  return verbosity;
}

std::ostream& operator<<(std::ostream & out, const Type & t) {
  if (auto value = t.cast<TensorType>()) {
    if  (value->scalarType().has_value()) {
      out << toString(*value->scalarType());
      if (!value->sizes().size().has_value()) {
        out << "Tensor";
      }
    } else {
      out << "Tensor";
    }
    if (auto ndim = value->sizes().size()) {
      bool has_valid_strides_info = *ndim > 0 &&
          value->strides().isComplete() && value->strides().size() == ndim;

      out << "(";
      size_t i = 0;
      bool symbolic = type_verbosity() == TypeVerbosity::Symbolic;
      for (i = 0; i < *ndim; ++i) {
        if (i > 0) {
          out << ", ";
        }
        if (auto s = value->sizes()[i]) {
          out << *s;
        } else if (symbolic) {
          out << value->symbolic_sizes().at(i);
        } else {
          out << "*";
        }
      }
      if (has_valid_strides_info &&
          type_verbosity() >= TypeVerbosity::TypeAndStride) {
        out << ", strides=[";
        for (size_t i = 0; i < *ndim; ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << *value->strides()[i];
        }
        out << "]";
      }
      if (type_verbosity() >= TypeVerbosity::Full) {
        if (value->requiresGrad()) {
          if (i++ > 0) {
            out << ", ";
          }
          out << "requires_grad=" << *value->requiresGrad();
        }
        if (value->device()) {
          if (i++ > 0) {
            out << ", ";
          }
          out << "device=" << *value->device();
        }
      }
      out << ")";
    } else {
      if (type_verbosity() >= TypeVerbosity::Full) {
        size_t i = 0;
        if (value->requiresGrad()) {
          out << "("
              << "requires_grad=" << *value->requiresGrad();
          i++;
        }
        if (value->device()) {
          out << ((i++ > 0) ? ", " : "(") << "device=" << *value->device();
        }
        if (i > 0) {
          out << ")";
        }
      }
    }

    if (value->undefined() && *value->undefined()) {
      out << "[Undefined]";
    }
  } else if(t.kind() == TypeKind::ListType) {
    auto prim = t.castRaw<ListType>()->getElementType();
    out << *prim << "[]";
  } else if (t.kind() == TypeKind::OptionalType) {
    auto prim = t.castRaw<OptionalType>()->getElementType();
    out << *prim << "?";
  } else if(t.kind() == TypeKind::FutureType) {
    auto elem = t.castRaw<FutureType>()->getElementType();
    out << "Future[" << *elem << "]";
  } else if(t.kind() == TypeKind::RRefType) {
    auto elem = t.castRaw<RRefType>()->getElementType();
    out << "RRef[" << *elem << "]";
  } else if(auto tup = t.cast<TupleType>()) {
    if (tup->schema()) {
      out << "NamedTuple";
    }
    out << "(";
    for(size_t i = 0; i < tup->elements().size(); ++i) {
      if(i > 0)
        out << ", ";
      if (tup->schema()) {
        auto arg = tup->schema()->arguments()[i];
        out << arg.name() << " : ";
        out << *(tup->elements()[i]);
        if (arg.default_value()) {
          out << " = " << *arg.default_value();
        }
      }
      else {
        out << *(tup->elements()[i]);
      }
    }
    out << ")";
  } else if (t.kind() == TypeKind::FunctionType) {
    out << "Function";
  } else {
     out << t.str();
  }
  return out;
}

const AnyTypePtr& AnyType::get() {
  static AnyTypePtr value(new AnyType());
  return value;
}

const TensorTypePtr& TensorType::get() {
  static auto value = TensorType::create(
      {}, {}, SymbolicShape(), VaryingShape<Stride>{}, {});
  return value;
}

const NumberTypePtr& NumberType::get() {
  static NumberTypePtr value(new NumberType());
  return value;
}
const IntTypePtr& IntType::get() {
  static IntTypePtr value(new IntType());
  return value;
}
const FloatTypePtr& FloatType::get() {
  static FloatTypePtr value(new FloatType());
  return value;
}
const ComplexTypePtr& ComplexType::get() {
  static ComplexTypePtr value(new ComplexType());
  return value;
}
const BoolTypePtr& BoolType::get() {
  static BoolTypePtr value(new BoolType());
  return value;
}
const StorageTypePtr& StorageType::get() {
  static StorageTypePtr value(new StorageType());
  return value;
}
const NoneTypePtr& NoneType::get() {
  static NoneTypePtr value(new NoneType());
  return value;
}
const GeneratorTypePtr& GeneratorType::get() {
  static GeneratorTypePtr value(new GeneratorType());
  return value;
}
const QuantizerTypePtr& QuantizerType::get() {
  static QuantizerTypePtr value(new QuantizerType());
  return value;
}
const QSchemeTypePtr& QSchemeType::get() {
  static QSchemeTypePtr value(new QSchemeType());
  return value;
}
const StringTypePtr& StringType::get() {
  static StringTypePtr value(new StringType());
  return value;
}
const DeviceObjTypePtr& DeviceObjType::get() {
  static DeviceObjTypePtr value(new DeviceObjType());
  return value;
}
const StreamObjTypePtr& StreamObjType::get() {
  static StreamObjTypePtr value(new StreamObjType());
  return value;
}
const ScalarTypeTypePtr& ScalarTypeType::get() {
static ScalarTypeTypePtr value(new ScalarTypeType());
return value;
}
const LayoutTypePtr& LayoutType::get() {
static LayoutTypePtr value(new LayoutType());
return value;
}
OptionalTypePtr OptionalType::ofTensor() {
  static auto value = OptionalType::create(TensorType::get());
  return value;
}
const PyObjectTypePtr& PyObjectType::get() {
  static PyObjectTypePtr value(new PyObjectType());
  return value;
}
const CapsuleTypePtr& CapsuleType::get() {
  static CapsuleTypePtr value(new CapsuleType());
  return value;
}
ListTypePtr ListType::ofTensors() {
  static auto value = ListType::create(TensorType::get());
  return value;
}
ListTypePtr ListType::ofOptionalTensors() {
  static auto value = ListType::create(OptionalType::ofTensor());
  return value;
}
ListTypePtr ListType::ofInts() {
  static auto value = ListType::create(IntType::get());
  return value;
}
ListTypePtr ListType::ofComplexDoubles() {
  static auto value = ListType::create(ComplexType::get());
  return value;
}
ListTypePtr ListType::ofFloats() {
  static auto value = ListType::create(FloatType::get());
  return value;
}
ListTypePtr ListType::ofBools() {
  static auto value = ListType::create(BoolType::get());
  return value;
}
ListTypePtr ListType::ofStrings() {
  static auto value = ListType::create(StringType::get());
  return value;
}

const AnyListTypePtr& AnyListType::get() {
  static AnyListTypePtr value(new AnyListType());
  return value;
}

const AnyTupleTypePtr& AnyTupleType::get() {
  static AnyTupleTypePtr value(new AnyTupleType());
  return value;
}

const AnyClassTypePtr& AnyClassType::get() {
  static AnyClassTypePtr value(new AnyClassType());
  return value;
}

const AnyEnumTypePtr& AnyEnumType::get() {
  static AnyEnumTypePtr value(new AnyEnumType());
  return value;
}

c10::optional<TypePtr> unifyTypesImpl(const TypePtr& t1, const TypePtr& t2, bool default_to_union=false, TypePtr type_hint=nullptr) {
  // check direct subtyping relation
  if (t1->isSubtypeOf(*t2)) {
    return t2;
  } else if (t2->isSubtypeOf(*t1)) {
    return t1;
  }

  // Handle non-container types which do not subtype each other and unify
  if (t1->kind() == TensorType::Kind && t2->kind() == TensorType::Kind) {
    return t1->expectRef<TensorType>().merge(t2->expectRef<TensorType>());
  }

  if (t1->isSubtypeOf(*NoneType::get()) && !t2->isSubtypeOf(*NoneType::get())) {
    return OptionalType::create(t2);
  } else if (t2->isSubtypeOf(*NoneType::get()) && !t1->isSubtypeOf(*NoneType::get())) {
    return OptionalType::create(t1);
  }

  // NB: we do not return NumberType because there is not currently enough
  // operator support for it

  // Attempt to unify Complete Tensor Types for immutable type containers

  // unify(Optional[t1], t2) => Optional[unify(t1, t2)]
  if (auto opt_t1 = t1->cast<OptionalType>()) {
    if (auto elem = unifyTypes(opt_t1->getElementType(), t2)) {
      return OptionalType::create(*std::move(elem));
    }
  } else if (auto opt_t2 = t2->cast<OptionalType>()) {
    if (auto elem = unifyTypes(opt_t2->getElementType(), t1)) {
      return OptionalType::create(*std::move(elem));
    }
  }

  if (t1->castRaw<TupleType>() && t2->castRaw<TupleType>()) {
    auto tuple1 = t1->castRaw<TupleType>();
    auto tuple2 = t2->castRaw<TupleType>();
    if (tuple1->elements().size() != tuple2->elements().size()) {
      return c10::nullopt;
    }
    std::vector<TypePtr> elements;
    for (size_t i = 0; i < tuple1->elements().size(); i++) {
      if (auto elem = unifyTypes(tuple1->elements().at(i), tuple2->elements().at(i), default_to_union)) {
        elements.push_back(*std::move(elem));
      } else {
        return c10::nullopt;
      }
    }
    return static_cast<TypePtr>(TupleType::create(std::move(elements)));
  }

  if (t1->castRaw<FutureType>() && t2->castRaw<FutureType>()) {
    if (auto elem = unifyTypes(
            t1->castRaw<FutureType>()->getElementType(),
            t2->castRaw<FutureType>()->getElementType())) {
      return FutureType::create(*elem);
    }
  }

  // Check direct subtyping relations again with Unshaped Types,
  // to handle unification of mutable container types which might contain two different
  // specialized tensors (ListType / DictType)
  auto t1_unshaped = unshapedType(t1);
  auto t2_unshaped = unshapedType(t2);

  if (t1_unshaped->isSubtypeOf(*t2_unshaped)) {
    return t2_unshaped;
  } else if (t2_unshaped->isSubtypeOf(*t1_unshaped)) {
    return t1_unshaped;
  }

  // Check whether or not `type_hint` is a common parent. This case
  // could occur if we had two class types that had been annotated with
  // a common interface
  if (type_hint && t1->isSubtypeOf(*type_hint) && t2->isSubtypeOf(*type_hint)) {
    return type_hint;
  }

  return c10::nullopt;
}

c10::optional<TypePtr> unifyTypes(const TypePtr& t1, const TypePtr& t2, bool default_to_union, TypePtr type_hint) {
  auto unified = unifyTypesImpl(t1, t2, default_to_union, std::move(type_hint));

  if (default_to_union && !unified) {
    return UnionType::create({t1, t2});
  }

  return unified;
}

c10::optional<TypePtr> unifyTypeList(
    at::ArrayRef<TypePtr> elements,
    std::ostream& why_not,
    bool default_to_union,
    TypePtr type_hint) {
  if (elements.size() == 0) {
    why_not << "Cannot get unified type from empty list";
    return c10::nullopt;
  }

  TypePtr ret_type = elements.at(0);
  for (size_t i = 1; i < elements.size() && ret_type; ++i) {
    c10::optional<TypePtr> maybe_unified = unifyTypes(ret_type, elements.at(i), default_to_union, type_hint);
    if (!maybe_unified) {
      why_not << "Could not unify type list since element " << i << " of type "
              << elements.at(i)->repr_str()
              << " did not match the types before it ("
              << ret_type->repr_str() << ")";
      return c10::nullopt;
    }
    ret_type = *maybe_unified;
  }

  return ret_type;
}

// NOTE: This function actually does need to take const TypePtr&
// because it sometimes calls unifyTypes, which needs const TypePtr&.
MatchTypeReturn matchTypeVariables(
    const TypePtr& formal,
    const TypePtr& actual,
    TypeEnv& type_env) {
  if (!formal->hasFreeVariables()) {
    return MatchTypeReturn::Success();
  }

  if (auto vt = formal->castRaw<VarType>()) {
    auto it = type_env.find(vt->name());
    if (it == type_env.end()) {
      type_env[vt->name()] = actual;
      return MatchTypeReturn::Success();
    } else if (auto unified = unifyTypes(it->second, actual)) {
      // note: unifyTypes allows subtyping in either direction, so actual
      // may be a supertype of the current binding. we're not responsible
      // for reporting the error, only for keeping type_env stable
      return MatchTypeReturn::Success();
    }
    std::stringstream ss;
    ss << "Type variable '" << vt->name() << "' previously matched to type "
       << it->second->repr_str() << " is matched to type "
       << actual->repr_str();
    return ss.str();
  } else if (auto lt_formal = formal->castRaw<ListType>()) {
    if (auto lt_actual = actual->castRaw<ListType>()) {
      auto innerMatch = matchTypeVariables(
          lt_formal->getElementType(), lt_actual->getElementType(), type_env);
      if (!innerMatch.success()) {
        // propagate the errMsg onward
        return innerMatch;
      }
      return MatchTypeReturn::Success();
    } else if (auto tup_type = actual->castRaw<TupleType>()) {
      std::stringstream ss;
      auto maybe_tuple_unified = unifyTypeList(tup_type->elements(), ss);
      if (maybe_tuple_unified) {
        return matchTypeVariables(
            lt_formal->getElementType(), *maybe_tuple_unified, type_env);
      }
    }

    std::stringstream ss;
    ss << "Cannot match " << lt_formal->repr_str() << " to "
       << actual->repr_str();
    return ss.str();
  } else if (auto tp_formal = formal->castRaw<TupleType>()) {
    if (auto tp_actual = actual->castRaw<TupleType>()) {
      if (tp_formal->elements().size() != tp_actual->elements().size()) {
        return MatchTypeReturn("Cannot match tuples of mismatched size");
      }
      for (size_t i = 0; i < tp_formal->elements().size(); ++i) {
        auto result = matchTypeVariables(
            tp_formal->elements()[i], tp_actual->elements()[i], type_env);
        if (!result.success()) {
          return result;
        }
      }
      return MatchTypeReturn::Success();
    } else {
      std::stringstream ss;
      ss << "Cannot match a tuple to " << actual->repr_str();
      return MatchTypeReturn(ss.str());
    }
  } else if (auto lt_formal = formal->castRaw<FutureType>()) {
    if (auto lt_actual = actual->castRaw<FutureType>()) {
      auto innerMatch = matchTypeVariables(
          lt_formal->getElementType(), lt_actual->getElementType(), type_env);
      if (!innerMatch.success()) {
        return innerMatch;
      }
      return MatchTypeReturn::Success();
    } else {
      std::stringstream ss;
      ss << "Cannot match a future to " << actual->repr_str();
      return ss.str();
    }
  } else if (auto lt_formal = formal->castRaw<RRefType>()) {
    if (auto lt_actual = actual->castRaw<RRefType>()) {
      auto innerMatch = matchTypeVariables(
          lt_formal->getElementType(), lt_actual->getElementType(), type_env);
      if (!innerMatch.success()) {
        return innerMatch;
      }
      return MatchTypeReturn::Success();
    } else {
      std::stringstream ss;
      ss << "Cannot match a rref to " << actual->repr_str();
      return ss.str();
    }
  } else if (auto opt_formal = formal->castRaw<OptionalType>()) {
    if (auto opt_actual = actual->castRaw<OptionalType>()) {
      auto optionedMatch = matchTypeVariables(
          opt_formal->getElementType(), opt_actual->getElementType(), type_env);
      if (!optionedMatch.success()) {
        return optionedMatch;
      }
    } else if (!actual->isSubtypeOf(*NoneType::get())) {
      // If the actual type is a non-optional, allow matching to the formal if
      // its element type matches the actual.
      // Don't match None because it is already an optional (but one of
      // unknown type).
      return matchTypeVariables(opt_formal->getElementType(), actual, type_env);
    }
    // note: if actual was None here we potentially did not fill in the type
    // variables contained in the formal. It is still a valid match because None
    // matches Optional[T] later error checking on tryEvalTypeVariables will
    // report the problem if we never match variables in type T
    return MatchTypeReturn::Success();
  } else if (auto dict_formal = formal->castRaw<DictType>()) {
    if (auto dict_actual = actual->castRaw<DictType>()) {
      auto key_match = matchTypeVariables(
          dict_formal->getKeyType(), dict_actual->getKeyType(), type_env);
      if (!key_match.success()) {
        return key_match;
      }
      auto value_match = matchTypeVariables(
          dict_formal->getValueType(), dict_actual->getValueType(), type_env);
      if (!value_match.success()) {
        return value_match;
      }
      return MatchTypeReturn::Success();
    } else {
      std::stringstream ss;
      ss << "Cannot match a dict to " << actual->repr_str();
      return ss.str();
    }
  }

  AT_ERROR("Unhandled free variable container: ", formal->repr_str());
}

// change return types like List[List[t]] into List[List[int]]
TORCH_API TypePtr tryEvalTypeVariables(const TypePtr& type, std::unordered_map<std::string, TypePtr>& type_env) {
  if (!type->hasFreeVariables()) {
    return type;
  }

  if (auto vt = type->castRaw<VarType>()) {
    auto it = type_env.find(vt->name());
    if (it == type_env.end()) {
      return nullptr;
    }
    return it->second;
  } else {
    std::vector<TypePtr> new_contained;
    new_contained.reserve(type->containedTypes().size());
    for (const TypePtr& t : type->containedTypes()) {
      TypePtr r = tryEvalTypeVariables(t, type_env);
      if (!r) {
        return nullptr;
      }
      new_contained.push_back(std::move(r));
    }
    return type->withContained(std::move(new_contained));
  }
}

TORCH_API bool elementTypeCanBeInferredFromMembers(const TypePtr& elem_type) {
  if (elem_type->kind() == UnionType::Kind
      || elem_type->kind() == OptionalType::Kind
      || elem_type->kind() == NumberType::Kind) {
    // Builtin Union types
    return false;
  }
  if (elem_type->kind() == InterfaceType::Kind) {
    // since classes can be members of multiple interfaces, we cannot
    // construct which interface the list holds from the members alone
    return false;
  }
  if (elem_type->kind() == AnyType::Kind) {
    // List of Any can contains heterogenous types
    return false;
  }
  return true;
}

const char * typeKindToString(TypeKind kind) {
#define CASE_TYPE(T) case TypeKind::T: return #T;
  switch(kind) {
    C10_FORALL_TYPES(CASE_TYPE)
  }
#undef CASE_TYPE
  return "";
}

bool Type::isSubtypeOfExt(const Type& rhs, std::ostream* why_not) const {
  if (rhs.kind() == TypeKind::AnyType || *this == rhs) {
    return true;
  }
  if (auto opt_rhs = rhs.castRaw<OptionalType>()) {
    return this->isSubtypeOfExt(*opt_rhs->getElementType(), why_not);
  }
  if (auto union_rhs = rhs.castRaw<UnionType>()) {
    // Check if `this` is a subtype of any of the types within the Union
    return std::any_of(union_rhs->containedTypes().begin(),
                       union_rhs->containedTypes().end(),
                       [&](const TypePtr& inner) {
                         return this->isSubtypeOfExt(*inner, why_not);
                       });
  }
  return false;
}

bool Type::is_module() const {
  return false;
}

std::string TensorType::str() const {
  return "Tensor";
}

template <typename T>
VaryingShape<T> VaryingShape<T>::merge(const VaryingShape<T>& other) const {
  if (!dims_ || !other.dims_ || dims_->size() != other.dims_->size()) {
    return VaryingShape<T>();
  }
  ListOfOptionalElements dims;
  for (size_t i = 0, n = dims_->size(); i < n; i++) {
    dims.push_back(merge_primitive((*dims_)[i], (*other.dims_)[i]));
  }
  return VaryingShape<T>(std::move(dims));
}

VaryingShape<int64_t> TensorType::sizes() const {
  if (!sizes_.rank()) {
    return VaryingShape<int64_t>();
  }
  return VaryingShape<int64_t>(
      fmap(*sizes_.sizes(), [](ShapeSymbol ss) {
        // we turn symbolic shapes into unknowns
        return ss.is_static()
            ? c10::optional<int64_t>(ss.static_size())
            : c10::nullopt;
      }));
}

TensorTypePtr TensorType::merge(const TensorType& other, bool merge_sizes) const {
  auto scalar_type = merge_primitive(scalarType(), other.scalarType());
  auto dev = merge_primitive(device(), other.device());
  auto sprops = stride_properties().merge(other.stride_properties());
  auto gr = merge_primitive(requiresGrad(), other.requiresGrad());
  auto undef = merge_primitive(undefined(), other.undefined());
  return TensorType::create(
      scalar_type,
      dev,
      merge_sizes ? symbolic_sizes().merge(other.symbolic_sizes())
                  : symbolic_sizes(),
      sprops,
      gr,
      undef);
}

template <typename T>
bool is_null_or_equal(c10::optional<T> a, c10::IntArrayRef b) {
  return !a.has_value() || a.value() == b;
}

bool TensorType::matchTensor(const at::Tensor& t) {
  bool undef = undefined().value_or(!t.defined());
  if (undef != !t.defined()) {
    // When the followings are true, we consider it's not a match:
    // - undefined().has_value() == true
    // - undefined().value() != !t.defined()
    return false;
  } else if (!t.defined()) {
    // When the followings are true, we consider it's a match:
    // - t is not defined
    // - undefined() == null or undefined().value() == true
    return true;
  }
  // Here we know t.defined() == true and compare all other properties.
  bool rg = at::GradMode::is_enabled() && t.requires_grad();
  bool matched_strides = (!stride_properties().size()) ||
      (!t.has_storage() && !stride_properties().isComplete()) ||
      stride_properties() ==
          computeStrideProps(t.sizes(), t.strides(), t.is_contiguous());
  return scalarType().value_or(t.scalar_type()) == t.scalar_type()
    && device().value_or(t.device()) == t.device()
    && requiresGrad().value_or(rg) == rg
    && matched_strides
    && is_null_or_equal(sizes().concrete_sizes(), t.sizes());
}

bool TensorType::operator==(const c10::Type& rhs) const {
  if (rhs.kind() != kind()) {
    return false;
  }
  auto rt = rhs.expect<TensorType>();

  return scalar_type_ == rt->scalarType() && sizes() == rt->sizes() &&
      stride_properties() == rt->stride_properties() &&
      device() == rt->device() && requiresGrad() == rt->requiresGrad() &&
      undefined() == rt->undefined();
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const VaryingShape<T>& vs) {
  out << "(";
  if (!vs.size()) {
    out << "*)";
    return out;
  }

  for (size_t i = 0; i < vs.size(); i++) {
    if (i > 0) {
      out << ", ";
    }
    if (vs[i].has_value()) {
      out << vs[i].value();
    } else {
      out << "*";
    }
  }
  out << ")";
  return out;
}

template std::ostream& operator<<(
    std::ostream& out,
    const VaryingShape<int64_t>& vs);
template std::ostream& operator<<(
    std::ostream& out,
    const VaryingShape<Stride>& vs);

std::ostream& operator<<(
    std::ostream& os,
    const SymbolicShape& ss) {
  // TODO: Unranked SymbolicShape printing is ambiguous with that of
  // dynamic-shaped vector.
  if(!ss.rank()) {
    os << "(*)";
    return os;
  }

  auto sizes = ss.sizes().value();

  os << "(";
  for (size_t i = 0; i < ss.rank().value(); i++) {
    if (i > 0) {
      os << ", ";
    }
    if(sizes[i].is_static()) {
      os << sizes[i];
    } else {
      os << "*";
    }
  }
  os << ")";

  return os;
}

std::ostream& operator<<(std::ostream& os, const ShapeSymbol& s) {
  if (s.value_ >= 0) {
    os << s.value_;
  } else {
    os << "SS(" << s.value_ << ')';
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const Stride& s) {
  os << "{";
  if (s.stride_index_.has_value()) {
    os << *s.stride_index_;
  } else {
    os << "*";
  }
  os << ":";
  if (s.stride_.has_value()) {
    os << *s.stride_;
  } else {
    os << "*";
  }
  os << '}';
  return os;
}

TupleTypePtr TupleType::createNamed(
    const c10::optional<c10::QualifiedName>& qualName,
    const std::vector<std::string>& field_names,
    const std::vector<TypePtr>& field_types) {
      std::vector<IValue> empty_defaults;
      return TupleType::createNamed(qualName, field_names, field_types, empty_defaults);
    }


TupleTypePtr TupleType::createNamed(const c10::optional<c10::QualifiedName>& qualName,
    const std::vector<std::string>& field_names,
    const std::vector<TypePtr>& field_types,
    std::vector<IValue>& field_defaults) {
  TORCH_INTERNAL_ASSERT(field_names.size() == field_types.size());

  std::vector<Argument> arguments;
  arguments.reserve(field_names.size());
  auto min_default_idx = field_names.size() - field_defaults.size();
  for (size_t i = 0; i < field_names.size(); ++i) {
    if (i < min_default_idx) {
      Argument arg{
          /*name=*/field_names[i],
          /*type=*/field_types[i],
          /*N=*/i};
      arguments.emplace_back(std::move(arg));
    }
    else {
      size_t j = i - min_default_idx;
      TORCH_CHECK(field_defaults[j].tagKind() != "Tensor", "Tensors are "
                  "not supported as default NamedTuple fields. Their "
                  "mutability could lead to potential memory aliasing "
                  "problems");
      Argument arg{
          /*name=*/field_names[i],
          /*type=*/field_types[i],
          /*N=*/i,
          /*default_value=*/field_defaults[j]};
      arguments.emplace_back(std::move(arg));
    }
  }

  auto schema = std::make_shared<FunctionSchema>(
      /*name=*/qualName.value_or(c10::QualifiedName()).name(),
      /*overload_name=*/std::string(""),
      /*arguments=*/std::move(arguments),
      /*returns=*/std::vector<Argument>{});
  return std::shared_ptr<TupleType>(new TupleType(
      field_types, qualName, schema)); // NOLINT(modernize-make-shared)
}

bool NoneType::isSubtypeOfExt(const Type& rhs, std::ostream *why_not) const {
  if (rhs.kind() == OptionalType::Kind) {
    return true;
  }
  return Type::isSubtypeOfExt(rhs, why_not);
}

// Remove nested Optionals/Unions during the instantiation of a Union or
// an Optional. This populates `types` with all the types found during
// flattening. At the end of `flattenUnion`, `types` may have
// duplicates, but it will not have nested Optionals/Unions
void flattenUnion(TypePtr& type, std::vector<TypePtr>* to_fill) {
  if (auto union_type = type->cast<UnionType>()) {
    for (auto inner : union_type->containedTypes()) {
      flattenUnion(inner, to_fill);
    }
  } else if (auto opt_type = type->cast<OptionalType>()) {
    auto inner = opt_type->getElementType();
    flattenUnion(inner, to_fill);
    to_fill->emplace_back(NoneType::get());
  } else if (type->kind() == NumberType::Kind) {
    to_fill->emplace_back(IntType::get());
    to_fill->emplace_back(FloatType::get());
    to_fill->emplace_back(ComplexType::get());
  } else {
    to_fill->emplace_back(type);
  }
}

// Helper function for `standardizeUnion`
//
// NB: If we have types `T1`, `T2`, `T3`, and `PARENT_T` such that `T1`,
// `T2`, and `T2` are children of `PARENT_T`, then `unifyTypes(T1, T2)`
// will return `PARENT_T`. This could be a problem if we didn't want our
// Union to also be able to take `T3 `. In our current type hierarchy,
// this isn't an issue--most types SHOULD be unified even if the parent
// type wasn't in the original vector. However, later additions to the
// type system might necessitate reworking `get_supertype`
void filterDuplicateSubtypes(std::vector<TypePtr>* types) {
  if (types->empty()) {
    return;
  }
  auto get_supertype = [](const TypePtr t1, const TypePtr t2) -> c10::optional<TypePtr> {
    // We don't want nested Optionals. Also, prematurely unifying to
    // `Optional` could prevent us from coalescing other types
    if ((t1->isSubtypeOf(*NoneType::get()) && !t2->isSubtypeOf(*NoneType::get()))
        || (!t1->isSubtypeOf(*NoneType::get()) && t2->isSubtypeOf(*NoneType::get()))) {
          return c10::nullopt;
    } else {
      return unifyTypes(t1, t2, /*default_to_union=*/false);
    }
  };

  // Coalesce types and delete all duplicates. Moving from right to left
  // through the vector, we try to unify the current element (`i`) with
  // each element (`j`) before the "new" end of the vector (`end`).
  // If we're able to unify the types at `types[i]` and `types[j]`, we
  // decrement `end`, swap `types[j]` with the unified type, and
  // break. Otherwise, we keep `end` where it is to signify that the
  // new end of the vector hasn't shifted
  size_t end_idx = types->size()-1;
  for (size_t i = types->size()-1; i > 0; --i) {
    for (size_t j = std::min(i-1, end_idx); ; --j) {
      c10::optional<TypePtr> unified;
      unified = get_supertype((*types)[i], (*types)[j]);
      if (unified) {
        (*types)[j] = *unified;
        (*types)[i] = (*types)[end_idx];
        --end_idx;
        break;
      }
      // Break condition here so we don't get `j = 0; j = j-1` and end
      // up with MAX_INT
      if (j == 0) {
        break;
      }
    }
  }
  // Cut off the vector's tail so that `end` is the real last element
  types->erase(types->begin() + end_idx + 1, types->end());

}

void sortUnion(std::vector<TypePtr>* types) {
  // We want the elements to be sorted so we can easily compare two
  // UnionType objects for equality in the future. Note that this order
  // is guaranteed to be stable since we've already coalesced any
  // possible types
  std::sort(types->begin(), types->end(),
          [](const TypePtr a, const TypePtr b) -> bool {
            if (a->kind() != b->kind()) {
              return a->kind() < b->kind();
            }
            return a->str() < b->str();
          });
}

void standardizeVectorForUnion(std::vector<TypePtr>& reference, std::vector<TypePtr>* to_fill) {
  for (auto type : reference) {
    flattenUnion(type, to_fill);
  }
  filterDuplicateSubtypes(to_fill);
  sortUnion(to_fill);
}

void standardizeVectorForUnion(std::vector<TypePtr>* to_flatten) {
  TORCH_INTERNAL_ASSERT(to_flatten, "`standardizeVectorForUnion` was ",
                        "passed a `nullptr`");
  std::vector<TypePtr> to_fill;
  standardizeVectorForUnion(*to_flatten, &to_fill);
  *to_flatten = to_fill;
}

UnionType::UnionType(std::vector<TypePtr> reference, TypeKind kind) : Type(kind) {
  TORCH_INTERNAL_ASSERT(!reference.empty(), "Cannot create an empty Union");

  standardizeVectorForUnion(reference, &types_);

  // Gate the assert in a regular conditional so that we don't create
  // this long error message unnecessarily
  if (types_.size() == 1) {
    std::stringstream msg;
    msg << "After type unification was performed, the Union with the "
        << "original types {";
    for (const auto i : c10::irange(reference.size())) {
      msg << reference[i]->repr_str();
      if (i > 0) {
        msg << ",";
      }
      msg << " ";
    }
    msg << "} has the single type " << types_[0]->repr_str()
         << ". Use the common supertype instead of creating a Union"
         << "type";
    TORCH_INTERNAL_ASSERT(false, msg.str());
  }

  can_hold_none_ = false;
  has_free_variables_ = false;

  for (const TypePtr& type : types_) {
    if (type->kind() == NoneType::Kind) {
      can_hold_none_ = true;
    }
    if (type->hasFreeVariables()) {
      has_free_variables_ = true;
    }
  }

}

UnionTypePtr UnionType::create(std::vector<TypePtr> reference) {
  auto union_type = new UnionType(std::move(reference));

  // Some very special-cased logic for `Optional`. This will be deleted
  // in a later PR
  bool int_found = false;
  bool float_found = false;
  bool complex_found = false;
  bool nonetype_found = false;

  auto update_is_opt_flags = [&](TypePtr t) {
    if (t == IntType::get()) {
      int_found = true;
    } else if (t == FloatType::get()) {
      float_found  = true;
    } else if (t == ComplexType::get()) {
      complex_found = true;
    } else if (t == NoneType::get()) {
      nonetype_found = true;
    }
  };

  for (const auto& t : union_type->containedTypes()) {
    update_is_opt_flags(t);
  }

  bool numbertype_found = int_found && float_found && complex_found;

  if (nonetype_found) {
    if (union_type->containedTypes().size() == 4 && numbertype_found) {
      return OptionalType::create(NumberType::get());
    }
    if (union_type->containedTypes().size() == 2) {
      auto not_none = union_type->containedTypes()[0] != NoneType::get()
                      ? union_type->containedTypes()[0]
                      : union_type->containedTypes()[1];
      return OptionalType::create(not_none);
    }
  }

  return UnionTypePtr(union_type);
}

bool UnionType::operator==(const Type& rhs) const {
  if (auto union_rhs = rhs.cast<UnionType>()) {
    // We can't compare the type vectors for equality using `operator=`,
    // because the vectors hold `TypePtr`s and we want to compare `Type`
    // equality
    if (union_rhs->containedTypes().size() != this->containedTypes().size()) {
      return false;
    }
    // Check that all the types in `this->types_` are also in
    // `union_rhs->types_`
    return std::all_of(this->containedTypes().begin(), this->containedTypes().end(),
                       [&](TypePtr lhs_type) {
                         return std::any_of(union_rhs->containedTypes().begin(),
                                            union_rhs->containedTypes().end(),
                                            [&](TypePtr rhs_type) {
                                              return *lhs_type == *rhs_type;
                                            });
                       });
  } else if (auto optional_rhs = rhs.cast<OptionalType>()) {
    if (optional_rhs->getElementType() == NumberType::get()) {
      return this->containedTypes().size() == 4
             && this->can_hold_none_
             && this->canHoldType(*NumberType::get());
    }
    auto optional_lhs = this->toOptional();
    return optional_lhs && *optional_rhs == *((optional_lhs.value())->expect<OptionalType>());
  } else if (rhs.kind() == NumberType::Kind) {
    return this->containedTypes().size() == 3 && canHoldType(*NumberType::get());
  } else {
    return false;
  }
}

bool UnionType::isSubtypeOfExt(const Type& rhs, std::ostream* why_not) const {
  std::vector<const Type*> rhs_types;
  if (const auto union_rhs = rhs.cast<UnionType>()) {
    // Fast path
    if (this->containedTypes() == rhs.containedTypes()) {
      return true;
    }
    for (const auto& typePtr: rhs.containedTypes()) {
      rhs_types.push_back(typePtr.get());
    }
  } else if (const auto optional_rhs = rhs.cast<OptionalType>()) {
    rhs_types.push_back(NoneType::get().get());
    if (optional_rhs->getElementType() == NumberType::get()) {
      std::array<const Type*, 3> number_types{IntType::get().get(), FloatType::get().get(), ComplexType::get().get()};
      rhs_types.insert(rhs_types.end(), number_types.begin(), number_types.end());
    } else {
      rhs_types.push_back(optional_rhs->getElementType().get());
    }
  } else if (const auto number_rhs = rhs.cast<NumberType>()) {
    std::array<const Type*, 3> number_types{IntType::get().get(), FloatType::get().get(), ComplexType::get().get()};
    rhs_types.insert(rhs_types.end(), number_types.begin(), number_types.end());
  } else {
    rhs_types.push_back(&rhs);
  }
  return std::all_of(this->containedTypes().begin(), this->containedTypes().end(),
                     [&](const TypePtr& lhs_type) -> bool {
                      return std::any_of(rhs_types.begin(),
                                         rhs_types.end(),
                                         [&](const Type* rhs_type) -> bool {
                                           return lhs_type->isSubtypeOfExt(*rhs_type, why_not);
                                         });
  });
}

std::string UnionType::unionStr(TypePrinter printer, bool is_annotation_str)
    const {
  std::stringstream ss;

  bool can_hold_numbertype = this->canHoldType(*NumberType::get());

  std::vector<TypePtr> number_types{IntType::get(), FloatType::get(), ComplexType::get()};

  auto is_numbertype = [&](TypePtr lhs) {
    for (const auto& rhs : number_types) {
      if (*lhs == *rhs) {
        return true;
      }
    }
    return false;
  };

  std::string open_delimeter = is_annotation_str ? "[" : "(";
  std::string close_delimeter = is_annotation_str ? "]" : ")";

  ss << "Union" + open_delimeter;
  bool printed = false;
  for (size_t i = 0; i < types_.size(); ++i) {
    if (!can_hold_numbertype || !is_numbertype(types_[i])) {
      if (i > 0) {
        ss << ", ";
        printed = true;
      }
      if (is_annotation_str) {
        ss << this->containedTypes()[i]->annotation_str(printer);
      } else {
        ss << this->containedTypes()[i]->str();
      }
    }
  }
  if (can_hold_numbertype) {
    if (printed) {
      ss << ", ";
    }
    if (is_annotation_str) {
      ss << NumberType::get()->annotation_str(printer);
    } else {
      ss << NumberType::get()->str();
    }
  }
  ss << close_delimeter;
  return ss.str();
}

std::string UnionType::str() const {
  return this->unionStr(nullptr, /*is_annotation_str=*/false);
}

std::string UnionType::annotation_str_impl(TypePrinter printer) const {
  return this->unionStr(printer, /*is_annotation_str=*/true);
}

bool UnionType::canHoldType(const Type& type) const {
  if (&type == NumberType::get().get()) {
    return canHoldType(*IntType::get())
           && canHoldType(*FloatType::get())
           && canHoldType(*ComplexType::get());
  } else {
    return std::any_of(this->containedTypes().begin(), this->containedTypes().end(),
                    [&](const TypePtr& inner) {
                      return type.isSubtypeOf(*inner);
                    });
  }
}

c10::optional<TypePtr> UnionType::toOptional() const {
  if (!canHoldType(*NoneType::get())) {
      return c10::nullopt;
  }

  std::vector<TypePtr> copied_types = this->containedTypes().vec();

  auto maybe_opt = UnionType::create(std::move(copied_types));

  if (maybe_opt->kind() == UnionType::Kind) {
    return c10::nullopt;
  } else {
    return maybe_opt;
  }
}

c10::optional<TypePtr> UnionType::subtractTypeSet(std::vector<TypePtr>& to_subtract) const {
  std::vector<TypePtr> types;

  // Given a TypePtr `lhs`, this function says whether or not `lhs` (or
  // one of its parent types) is in the `to_subtract` vector
  auto should_subtract = [&](const TypePtr& lhs) -> bool {
    return std::any_of(to_subtract.begin(), to_subtract.end(),
                        [&](const TypePtr& rhs) {
                          return lhs->isSubtypeOf(*rhs);
                        });
  };

  // Copy all the elements that should NOT be subtracted to the `types`
  // vector
  std::copy_if(this->containedTypes().begin(), this->containedTypes().end(),
              std::back_inserter(types),
              [&](const TypePtr& t) {
                return !should_subtract(t);
              });

  if (types.size() == 0) {
    return c10::nullopt;
  } else if (types.size() == 1) {
    return types[0];
  } else {
    return UnionType::create(std::move(types));
  }
}

OptionalType::OptionalType(TypePtr contained)
                           : UnionType({contained, NoneType::get()}, TypeKind::OptionalType) {
  bool is_numbertype = false;
  if (auto as_union = contained->cast<UnionType>()) {
    is_numbertype = as_union->containedTypes().size() == 3 &&
                    as_union->canHoldType(*NumberType::get());
  }
  if (UnionType::containedTypes().size() == 2) {
    contained_ = UnionType::containedTypes()[0]->kind()!= NoneType::Kind
                 ? UnionType::containedTypes()[0]
                 : UnionType::containedTypes()[1];
  } else if (contained == NumberType::get() || is_numbertype) {
    contained_ = NumberType::get();
    types_.clear();
    types_.push_back(NumberType::get());
    types_.push_back(NoneType::get());
  } else {
    std::vector<TypePtr> to_subtract{NoneType::get()};
    auto without_none = this->subtractTypeSet(to_subtract);
    contained_ = UnionType::create({*without_none});
  }
  has_free_variables_ = contained_->hasFreeVariables();
}

bool OptionalType::operator==(const Type& rhs) const {
  if (auto union_rhs = rhs.cast<UnionType>()) {
    auto optional_rhs = union_rhs->toOptional();
    // `**optional_rhs` = `*` to get value of `c10::optional<TypePtr>`,
    // then `*` to dereference the pointer
    return optional_rhs && *this == **optional_rhs;
  } else if (auto optional_rhs = rhs.cast<OptionalType>()) {
    return *this->getElementType() == *optional_rhs->getElementType();
  } else {
    return false;
  }
}

bool OptionalType::isSubtypeOfExt(const Type& rhs, std::ostream* why_not) const {
  if (auto optional_rhs = rhs.castRaw<OptionalType>()) {
    return getElementType()->isSubtypeOfExt(*optional_rhs->getElementType(), why_not);
  } else if (auto union_rhs = rhs.castRaw<UnionType>()) {
    if (!union_rhs->canHoldType(*NoneType::get())) {
      if (why_not) {
        *why_not << rhs.repr_str() << " cannot hold None";
      }
      return false;
    } else if (!union_rhs->canHoldType(*this->getElementType())) {
      if (why_not) {
        *why_not << rhs.repr_str() << " cannot hold " << this->getElementType();
      }
      return false;
    } else {
      return true;
    }
  } else {
    // NOLINTNEXTLINE(bugprone-argument-comment)
    return Type::isSubtypeOfExt(rhs, why_not);
  }
}

bool NumberType::operator==(const Type& rhs) const {
  if (auto union_type = rhs.cast<UnionType>()) {
    return union_type->containedTypes().size() == 3 && union_type->canHoldType(*NumberType::get());
  } else {
    return rhs.kind() == this->kind();
  }
}

bool NumberType::isSubtypeOfExt(const Type& rhs, std::ostream* why_not) const {
  if (auto union_type = rhs.cast<UnionType>()) {
    return union_type->canHoldType(*NumberType::get());
  } else {
    return Type::isSubtypeOfExt(rhs, why_not);
  }
}

TupleType::TupleType(
    std::vector<TypePtr> elements,
    c10::optional<c10::QualifiedName> name,
    std::shared_ptr<FunctionSchema> schema)
    : NamedType(TypeKind::TupleType, std::move(name)),
      elements_(std::move(elements)),
      schema_(std::move(schema)) {
  has_free_variables_ =
      std::any_of(elements_.begin(), elements_.end(), [](TypePtr v) {
        if (!v) {
          throw std::runtime_error("Can not create tuple with None type");
        }
        return v->hasFreeVariables();
      });
  if (schema_) {
    for (const Argument& arg : schema_->arguments()) {
      checkNoAny(*this, "attribute", arg.name(), arg.type());
    }
  }
}

bool TupleType::isSubtypeOfExt(const Type& rhs_, std::ostream* why_not) const {
  if (Type::isSubtypeOfExt(rhs_, why_not)) {
    return true;
  }
  if (rhs_.kind() == AnyTupleType::Kind) {
    return true;
  }
  auto rhs = rhs_.cast<TupleType>();
  if (!rhs)
    return false;
  // unnamed tuple is not a subtype of nametuple
  if (!schema() && rhs->schema())
    return false;
  // namedtuple may be a subtype of unnamed tuple
  auto test_names_match = [&](const std::shared_ptr<FunctionSchema>& lhs, const std::shared_ptr<FunctionSchema>& rhs) {
    const auto& args_lhs = lhs->arguments();
    const auto& args_rhs = rhs->arguments();
    if (args_lhs.size() != args_rhs.size()) {
      return false;
    }

    for (size_t i = 0; i < args_lhs.size(); ++i) {
      if (args_lhs[i].name() != args_rhs[i].name()) {
        return false;
      }
    }
    return true;
  };
  bool names_match = !rhs->schema() || test_names_match(schema(), rhs->schema());
  // co-variant rules for tuples
  return names_match && compare(*rhs, [&](const Type& a, const Type& b) {
    return a.isSubtypeOfExt(b, why_not);
  });
}

bool ListType::isSubtypeOfExt(const Type& rhs_, std::ostream* why_not) const {
  if (Type::isSubtypeOfExt(rhs_, why_not)) {
    return true;
  }
  if (rhs_.kind() == AnyListType::Kind) {
    return true;
  }
  return false;
}

 bool TupleType::operator==(const Type& rhs) const {
   bool typesSame =
       compare(rhs, [](const Type& a, const Type& b) { return a == b; });
   if (!typesSame) {
     return false;
  }

  // `compare` guarantees that rhs is always a TupleType.
  auto rhsTuple = rhs.expect<TupleType>();
  if (schema_ == nullptr && rhsTuple->schema_ == nullptr) {
    return typesSame;
  }
  if (schema_ == nullptr || rhsTuple->schema_ == nullptr) {
    return false;
  }
  return *schema_ == *rhsTuple->schema_;
}

std::string TupleType::str() const {
  std::stringstream ss;
  if (schema_ && name()) {
    ss << name()->qualifiedName();
  } else {
    ss << "(";
    for(size_t i = 0; i < elements().size(); ++i) {
      if(i > 0)
        ss << ", ";
      ss << elements()[i]->str();
    }
    ss << ")";
  }
  return ss.str();
}
std::string TupleType::annotation_str_impl(TypePrinter printer) const {
  std::stringstream ss;
  if (schema_ && name()) {
    ss << name()->qualifiedName();
  } else {
    ss << "Tuple[";
    if (elements().size() == 0) {
      // `typing.Tuple` special-cases the annotation syntax for empty tuple
      // with `typing.Tuple[()]`. See
      // https://docs.python.org/3/library/typing.html#typing.Tuple
      ss << "()";
    } else {
      for (size_t i = 0; i < elements().size(); ++i) {
        if (i > 0)
          ss << ", ";
        ss << elements()[i]->annotation_str(printer);
      }
    }
    ss << "]";
  }
  return ss.str();
}

VaryingShape<int64_t> TensorType::strides() const {
  if (!strides_.size().has_value()) {
    return VaryingShape<int64_t>();
  }
  std::vector<c10::optional<int64_t>> ss(*strides_.size());
  for (size_t i = 0; i < *strides_.size(); i++) {
    if (!strides_[i].has_value()) {
      continue;
    }
    auto s = *strides_[i];
    if (s.stride_index_.has_value() && s.stride_.has_value()) {
      ss[*s.stride_index_] = *s.stride_;
    }
  }
  return VaryingShape<int64_t>(ss);
}

VaryingShape<Stride> TensorType::computeStrideProps(
    at::IntArrayRef sizes,
    at::IntArrayRef strides,
    bool tensor_contiguity) {
  int n_dim = static_cast<int>(sizes.size());
  std::vector<size_t> stride_indices(n_dim);

  // Sorting strides in ascending order
  // Example:
  //  Prior to sorting
  //  Idx:     [0,   1,  2,  3]
  //  sizes:   [8,   1, 10, 16]
  //  Strides: [160, 1, 16,  1]
  //  After sorting
  //  Idx:     [1,  3,  2,   0]
  //  sizes:   [1, 16, 10,   8]
  //  Strides: [1,  1, 16, 160]
  //
  // The logic below follows what TensorIterator uses in its logic:
  //   1. Fast_set_up is the short-cut to identify a. channels_last and
  //      b. contiguous format, which is what we have in the below logic.
  //   2. In more generla cases, it does best effort to preserve permutatoin.
  if (is_channels_last_strides_2d(sizes, strides) || is_channels_last_strides_3d(sizes, strides)) {
    // case 1.a. short cut channels last
    std::iota(stride_indices.rbegin() + 1, stride_indices.rend() - 1, 2);
    stride_indices[0] = 1;
    stride_indices[n_dim - 1] = 0;
  } else if (is_contiguous_strides(sizes, strides)) {
    // case 1.b. short cut contiguous
    std::iota(stride_indices.rbegin(), stride_indices.rend(), 0);
  } else {
    std::iota(stride_indices.begin(), stride_indices.end(), 0);
    // case 2.
    //
    // For broadcasted dimension where stride is 0, we have to stick to
    // TensorIterator behavior in eager, where they introduce an ambiguous
    // comparison result to preserve permutation by best effort.
    // For more details, see NOTE: [Computing output strides]
    auto should_swap = [&](size_t a, size_t b) {
      if (strides[a] == 0 || strides[b] == 0) {
        return 0;
      } else if (strides[a] < strides[b]) {
        return -1;
      } else if (strides[a] > strides[b]) {
        return 1;
      } else { // strides[a] == strides[b]
        if (sizes[a] < sizes[b] || a > b ) {
          return 1;
        }
      }
      return 0;
    };
    for (int i = 1; i < n_dim; i++) {
      int dim1 = i;
      for (int dim0 = i - 1; dim0 >= 0; dim0--) {
        int comparison = should_swap(stride_indices[dim0], stride_indices[dim1]);
        if (comparison > 0) {
          std::swap(stride_indices[dim0], stride_indices[dim1]);
          dim1 = dim0;
        } else if (comparison < 0) {
          break;
        }
      }
    }
  }
  std::vector<Stride> stride_properties;
  for (size_t i = 0; i < stride_indices.size(); i++) {
    bool contiguous_ = tensor_contiguity;
    if (!contiguous_) {
      // innermost stride expected to be 1
      // TODO: turn contiguous_ into an enum CONTIGUOUS, NONCONTIGUOUS,
      // BROADCASTED
      if (i == 0) {
        contiguous_ = strides[stride_indices[i]] == 1;
      } else {
        contiguous_ = strides[stride_indices[i]] == 1 ||
            (strides[stride_indices[i]] != 0 &&
             strides[stride_indices[i]] ==
                 strides[stride_indices[i - 1]] * sizes[stride_indices[i - 1]]);
      }
    }
    stride_properties.emplace_back(stride_indices[i], contiguous_, strides[stride_indices[i]]);
  }

  return VaryingShape<Stride>{stride_properties};
}

std::atomic<size_t> ShapeSymbol::num_symbols{1};

template struct VaryingShape<c10::ShapeSymbol>;
template struct VaryingShape<bool>;
template struct VaryingShape<size_t>;
template struct VaryingShape<int64_t>;

TensorType::TensorType(
    c10::optional<at::ScalarType> scalar_type,
    c10::optional<Device> device,
    // NOLINTNEXTLINE(modernize-pass-by-value)
    const SymbolicShape& sizes,
    const VaryingShape<Stride>& strides,
    c10::optional<bool> requires_grad,
    c10::optional<bool> undefined)
    : Type(TypeKind::TensorType),
      scalar_type_(scalar_type),
      device_(device),
      sizes_(sizes),
      strides_(strides),
      requires_grad_(requires_grad),
      undefined_(undefined) {}

TensorTypePtr TensorType::create(const at::Tensor& t) {
  VaryingShape<bool> contiguity;
  VaryingShape<size_t> stride_indices;
  VaryingShape<int64_t> strides;
  VaryingShape<int64_t> sizes;
  if (!t.is_mkldnn() && !t.is_sparse()) {
    sizes = VaryingShape<int64_t>{t.sizes().vec()};
    strides = VaryingShape<int64_t>{t.strides().vec()};
    return TensorType::create(
        t.scalar_type(), t.device(), sizes, strides, t.requires_grad(), false, t.is_contiguous());
  }

  return TensorType::create(
      t.scalar_type(),
      t.device(),
      SymbolicShape(),
      VaryingShape<Stride>{},
      t.requires_grad(),
      false);
}

TensorTypePtr TensorType::create(
    c10::optional<at::ScalarType> scalar_type,
    c10::optional<Device> device,
    const VaryingShape<int64_t>& sizes,
    const VaryingShape<int64_t>& strides,
    c10::optional<bool> requires_grad,
    c10::optional<bool> undefined, bool tensor_contiguity) {
  if(strides.concrete_sizes() && strides.concrete_sizes().has_value()){
    // handles case where strides are set
    TORCH_INTERNAL_ASSERT(sizes.concrete_sizes()->size() == strides.concrete_sizes()->size());
    auto sprops = strides.concrete_sizes().has_value()
      ? computeStrideProps(*sizes.concrete_sizes(), *strides.concrete_sizes(), tensor_contiguity)
      : VaryingShape<Stride>();
    auto symbol_sizes = SymbolicShape(*sizes.concrete_sizes());
    return TensorType::create(
      scalar_type, device, symbol_sizes, sprops, requires_grad, undefined);
  } else {
    // strides are all null, but still have number of strides equal to number of ranks
    TORCH_INTERNAL_ASSERT(sizes.sizes() && sizes.size());
    auto symbol_sizes = SymbolicShape(*sizes.sizes());
    return TensorType::create(
      scalar_type, device, symbol_sizes, VaryingShape<Stride>(*sizes.size()), requires_grad, undefined);
  }
}

TensorTypePtr TensorType::create(
    c10::optional<at::ScalarType> scalar_type,
    c10::optional<Device> device,
    const SymbolicShape& sizes,
    const VaryingShape<Stride>& strides,
    c10::optional<bool> requires_grad,
    c10::optional<bool> undefined) {
  auto pt = TensorTypePtr(new TensorType(
      scalar_type, device, sizes, strides, requires_grad, undefined));
  return pt;
}

TensorTypePtr TensorType::create(
    c10::optional<at::ScalarType> scalar_type,
    c10::optional<Device> device,
    c10::optional<size_t> dim,
    c10::optional<bool> requires_grad) {
  return TensorType::create(
      scalar_type,
      device,
      SymbolicShape(dim),
      VaryingShape<Stride>(dim),
      requires_grad);
}

TensorTypePtr TensorType::createContiguous(
    at::ScalarType scalar_type,
    at::Device device,
    at::IntArrayRef sizes) {
  auto strides = contiguousStridesOf(sizes);
  TORCH_INTERNAL_ASSERT(strides.size() == sizes.size());
  return create(
      scalar_type,
      device,
      VaryingShape<int64_t>(sizes),
      VaryingShape<int64_t>(strides),
      c10::nullopt);
}

const SymbolicShape& TensorType::symbolic_sizes() const {
  return sizes_;
}

bool TensorType::isSubtypeOfExt(const Type& rhs, std::ostream* why_not) const {
  if (auto rhs_p = rhs.cast<TensorType>()) {
    // if we have the same pointer, avoid computing the merge
    if (this == rhs_p.get()) {
      return true;
    }
    return *merge(*rhs_p) == *rhs_p;
  }
  return Type::isSubtypeOfExt(rhs, why_not);
}

InterfaceTypePtr InterfaceType::create(QualifiedName qualifiedName, bool is_module) {
  return InterfaceTypePtr(
      new InterfaceType(std::move(qualifiedName), is_module));
}

void ClassType::addMethod(torch::jit::Function* method) {
  TORCH_CHECK(
      findMethod(method->name()) == nullptr,
      "Can't redefine method: ",
      method->name(),
      " on class: ",
      repr_str());
  methods_.push_back(method);
}

const std::vector<torch::jit::Function*>& ClassType::getForwardHooks() const {
    return forward_hooks_;
}

const std::vector<torch::jit::Function*>& ClassType::getForwardPreHooks() const {
    return forward_pre_hooks_;
}

void ClassType::addForwardPreHook(torch::jit::Function* pre_hook_ptr) {
    forward_pre_hooks_.emplace_back(pre_hook_ptr);
}

void ClassType::addForwardHook(torch::jit::Function* hook_ptr) {
    forward_hooks_.emplace_back(hook_ptr);
}

torch::jit::Function* ClassType::findForwardPreHook(const std::string& name) const {
  for (const auto& pre_hook : forward_pre_hooks_) {
    if (name == pre_hook->name()) {
      return pre_hook;
    }
  }
  return nullptr;
}

torch::jit::Function* ClassType::findForwardHook(const std::string& name) const {
  for (const auto& hook : forward_hooks_) {
    if (name == hook->name()) {
      return hook;
    }
  }
  return nullptr;
}

std::string getSchemaInputTypesString(const FunctionSchema& schema) {
  std::stringstream input_types;
  const std::vector<Argument>& forward_args = schema.arguments();
  for (const auto i : c10::irange(1, forward_args.size())) {
    input_types << forward_args[i].type()->annotation_str();
    if (forward_args.size() - 1 != i) {
      input_types << ", ";
    }
  }
  if (forward_args.size() == 1) {
    input_types << "()";
  }
  return input_types.str();
}

std::string ClassType::getForwardPreHookErrorMessage(int pre_hook_idx) const {
  const std::string& pre_hook_name = forward_pre_hooks_[pre_hook_idx]->name();
  const FunctionSchema& forward_schema = getMethod("forward").getSchema();
  std::string input_types = getSchemaInputTypesString(forward_schema);
  const std::vector<Argument>& forward_args = forward_schema.arguments();

  std::string single_output = "";
  if (forward_args.size() == 2 &&
      forward_args[1].type()->cast<TupleType>() == nullptr) {
    // if the output type is a single tuple, it needs to be wrapped in an outer tuple
    // to match eager's behavior
    single_output = ", '" + forward_args[1].type()->annotation_str() + "',";
  }
  std::string pre_hook_schema =
      pre_hook_name + "(self, input: Tuple[" + input_types + "])";
  std::string return_string =
      "This error occured while scripting the forward pre-hook '" +
      pre_hook_name + "' on module '" + name()->name() +
      "'. If you did not want to script this pre-hook remove it from the "
      "original NN module before scripting. Pre-hooks for module '" +
      name()->name() + "' are expected to have the following signature: "
      + pre_hook_schema + " with a return type of either 'None'" +
      single_output + " or 'Tuple[" + input_types + "]'.";
  return return_string;
}

std::string ClassType::getForwardHookErrorMessage(int hook_idx) const {
  const std::string& hook_name = forward_hooks_[hook_idx]->name();
  const FunctionSchema& forward_schema = getMethod("forward").getSchema();
  std::string input_types = getSchemaInputTypesString(forward_schema);

  // create expected output types string
  const Argument& pre_output =
      (hook_idx == 0)
          ? forward_schema.returns()[0]
          : forward_hooks_[hook_idx - 1]->getSchema().returns()[0];
  std::string output_types = pre_output.type()->annotation_str();
  // create error message
  std::string hook_schema = hook_name + "(self, input: Tuple[" +
                            input_types + "], output: " + output_types + ")";
  std::string return_string =
      "This error occured while scripting the forward hook '"
      + hook_name + "' on module " + name()->name() +
      ". If you did not want to script this hook remove it from" +
      " the original NN module before scripting. This hook was" +
      " expected to have the following signature: " + hook_schema +
      ". The type of the output arg is the returned type from" +
      " either the forward method or the previous hook if it exists. " +
      "Note that hooks can return anything, but if the hook is " +
      "on a submodule the outer module is expecting" +
      " the same return type as the submodule's forward.";
  return return_string;
}

bool ClassType::isUnresolvedClassAttribute(const std::string& name) const {
  return std::find(
      unresolved_class_attributes_.begin(),
      unresolved_class_attributes_.end(),
      name) != unresolved_class_attributes_.end();
}

void checkForwardHookInputArguments(
    const FunctionSchema& forward_schema,
    const FunctionSchema& hook_schema,
    const std::string& hook_id,
    const std::string& hook_err_msg) {
  // check for proper tuple input types
  const std::vector<Argument>& forward_args = forward_schema.arguments();
  const Argument input_arg = hook_schema.arguments()[1];
  TORCH_CHECK(
      input_arg.type()->cast<TupleType>() != nullptr,
      hook_id,
      "expected the input argument to be typed as a Tuple but found type: '",
      input_arg.type()->annotation_str(),
      "' instead.\n",
      hook_err_msg
   );

  const at::ArrayRef<TypePtr> input_tuple_types = input_arg.type()->castRaw<TupleType>()->elements();
  if (forward_args.size() == 1) {
    // check for empty forward case
    TORCH_CHECK(
        input_tuple_types.size() == 0,
        hook_id,
        "was expecting Tuple[()] as the input type. Received type: '",
        input_arg.type()->annotation_str(),
        "'.\n",
        hook_err_msg
      );
  } else {
    // check input tuple for correct size and correct contained types
    TORCH_CHECK(
        input_tuple_types.size() == forward_args.size() - 1,
        hook_id,
        "has the wrong number of contained types for the",
        " input argument's Tuple. Received type: '",
        input_arg.type()->annotation_str(),
        "'.\n",
        hook_err_msg
    );

    for (const auto i : c10::irange(1, forward_args.size())) {
      if (*forward_args[i].type() != *input_tuple_types[i - 1]) {
        TORCH_CHECK(
            false,
            hook_id,
            "has the wrong inner types for the input tuple argument. Received type: '",
            input_arg.type()->annotation_str(),
            "'.\n",
            hook_err_msg
        );
      }
    }
  }
}

void ClassType::checkForwardPreHookSchema(
    int pre_hook_idx,
    const FunctionSchema& pre_hook_schema) const {
  const torch::jit::Function* pre_hook = forward_pre_hooks_[pre_hook_idx];
  std::string hook_id =
      "Pre-hook '" + pre_hook->name() + "' on module '" + name()->name() + "' ";
  std::string pre_hook_err_msg = getForwardPreHookErrorMessage(pre_hook_idx) + "\n";

  // Pre-hooks are expecting two inputs: self, and a Tuple containing the
  // non-self arguments passed to Forward
  TORCH_CHECK(
      pre_hook_schema.arguments().size() == 2,
      hook_id,
      "was expected to only have exactly 2 inputs but it had ",
      pre_hook_schema.arguments().size(),
      " inputs. ",
      pre_hook_err_msg
   );

  const FunctionSchema& forward_schema = getMethod("forward").getSchema();
  const std::vector<Argument>& forward_args = forward_schema.arguments();
  checkForwardHookInputArguments(forward_schema, pre_hook_schema, hook_id, pre_hook_err_msg);

  // check return type, expected to be either None, the same type as the input,
  // or the contained single type if the input was a tuple containing a single
  // type.
  TORCH_CHECK(
            pre_hook_schema.returns().size() != 0,
            hook_id,
            "is missing a return annotation. Return annotations are required, please add one.\n",
            pre_hook_err_msg
  );
  const Argument return_arg = pre_hook_schema.returns()[0];
  std::string wrong_type_returned_err_msg = hook_id +
      "returned the wrong type of: '" +
      return_arg.type()->annotation_str() + "'.";

  if (return_arg.type()->kind() == NoneType::get()->kind()) {
    return;
  }
  if (forward_args.size() == 2 && *forward_args[1].type() == *return_arg.type()) {
    // TORCH_CHECK below is for the edge case where forward's input is a tuple and the
    // pre-hook returns a matching tuple. Eager doesn't support this- the working eager return
    // for a tuple type is the forward's input tuple wrapped inside of another tuple.
    TORCH_CHECK(
        return_arg.type()->cast<TupleType>() == nullptr,
        wrong_type_returned_err_msg,
        " When forward has a single tuple input argument, the return needs",
        " to be 'None' or a nested tuple containing forward's input tuple",
        " argument as in: 'Tuple[",
        forward_args[1].type()->annotation_str(),
        "]'.\n",
        pre_hook_err_msg
    );
    return;
  }
  // return can only be tuple of nested types now
  // check to make sure return is of tuple type
  TORCH_CHECK(
      return_arg.type()->cast<TupleType>() != nullptr,
      wrong_type_returned_err_msg,
      pre_hook_err_msg
  );
  const at::ArrayRef<TypePtr> return_tuple_types =
      return_arg.type()->castRaw<TupleType>()->elements();
  // check for edge case of Tuple[()] for when forward has no arguments
  if (forward_args.size() == 1) {
    TORCH_CHECK(
        return_tuple_types.size() == 0,
        wrong_type_returned_err_msg,
        " Was expecting either 'None' or 'Tuple[()]' since forward had ",
        "no arguments.\n",
        pre_hook_err_msg
    );
    return;
  }

  // check that tuple has proper number of contained types
  TORCH_CHECK(
      return_tuple_types.size() == forward_args.size() - 1,
      wrong_type_returned_err_msg,
      " The returned tuple contains the wrong number of contained types.\n",
      pre_hook_err_msg
  );
  // check that contained types match forward types
  for (const auto i : c10::irange(1, forward_args.size())) {
    if (*forward_args[i].type() != *return_tuple_types[i - 1]) {
      TORCH_CHECK(
          false,
          wrong_type_returned_err_msg,
          " The returned tuple contains the wrong inner types.\n",
          pre_hook_err_msg);
    }
  }
}

void ClassType::checkForwardHookSchema(
      int hook_idx,
      const FunctionSchema& hook_schema) const {
  const torch::jit::Function* hook = forward_hooks_[hook_idx];
  std::string hook_id =
      "Hook '" + hook->name() + "' on module '" + name()->name() + "' ";
  std::string hook_err_msg = getForwardHookErrorMessage(hook_idx) + "\n";
  // Hooks are expecting three inputs: self, a Tuple containing the non-self
  // arguments passed to Forward, and the output of either Forward or the
  // previous hook
  TORCH_CHECK(
      hook_schema.arguments().size() == 3,
      hook_id,
      "was expected to only have exactly 3 inputs but it had ",
      hook_schema.arguments().size(),
      " inputs. ",
      hook_err_msg
  );

  const FunctionSchema& forward_schema = getMethod("forward").getSchema();
  checkForwardHookInputArguments(forward_schema, hook_schema, hook_id, hook_err_msg);

  // check output tuple
  const Argument& prev_output = (hook_idx == 0)
            ? forward_schema.returns()[0]
            : forward_hooks_[hook_idx - 1]->getSchema().returns()[0];
  const Argument return_arg = hook_schema.arguments()[2];

  // output tuple needs to match prev_output's return exactly
  TORCH_CHECK(
      *prev_output.type() == *return_arg.type(),
      hook_id,
      "has the wrong type for the output argument. Received type: '",
      return_arg.type()->annotation_str(),
      "'. Expected type: '",
      prev_output.type()->annotation_str(),
      "'.\n",
      hook_err_msg
  );
}

torch::jit::Function* ClassType::findMethod(const std::string& name) const {
  for (auto method : methods_) {
    if (name == method->name()) {
      return method;
    }
  }
  return nullptr;
}
torch::jit::Function& ClassType::getMethod(const std::string& name) const {
  auto method = findMethod(name);
  TORCH_CHECK(
      method != nullptr,
      "Couldn't find method: '",
      name,
      "' on class: '",
      repr_str(),
      "'");
  return *method;
}

torch::jit::Function* ClassType::findHook(const std::string& name) const {
  auto hook = findForwardHook(name);
  if (hook == nullptr) {
    hook = findForwardPreHook(name);
  }
  return hook;
}

torch::jit::Function& ClassType::getHook(const std::string& name) const {
  torch::jit::Function* function = findHook(name);
  TORCH_CHECK(
      function != nullptr,
      "Couldn't find: '",
      name,
      "' on class: '",
      repr_str(),
      "'as forward hook or forward pre_hook.");
  return *function;
}

bool ClassType::hasMethod(const std::string& name) const {
  return findMethod(name) != nullptr;
}

void ClassType::addStaticMethod(torch::jit::Function* method) {
  TORCH_CHECK(
      findStaticMethod(method->name()) == nullptr &&
          findMethod(method->name()) == nullptr, "Can't redefine method: ",
      method->name(),
      " on class: ",
      repr_str());
  staticmethods_.emplace_back(method);
}

torch::jit::Function* ClassType::findStaticMethod(const std::string& name) const {
  for (auto method : staticmethods_) {
    if (name == method->name()) {
      return method;
    }
  }
  return nullptr;
}

void ClassType::unsafeRemoveMethod(const std::string& name) {
  size_t slot = 0;
  for (auto method : methods_) {
    if (method->name() == name) {
      methods_.erase(methods_.begin() + slot);
      return;
    }
    slot++;
  }
  TORCH_CHECK(
      false,
      "Can't delete undefined method ",
      name,
      " on class: ",
      repr_str());
}

ClassTypePtr ClassType::refine(at::ArrayRef<TypePtr> refined_slots) const {
  auto ptr = ClassType::create(name(), compilation_unit_, is_module());
  AT_ASSERT(numAttributes() == refined_slots.size());
  for (size_t i = 0; i < attributes_.size(); ++i) {
    AT_ASSERT(refined_slots[i]->isSubtypeOf(*attributes_[i].getType()));
    ptr->addAttribute(attributes_[i].getName(), refined_slots[i], (attributes_[i].getKind() == AttributeKind::PARAMETER),
    (attributes_[i].getKind() == AttributeKind::BUFFER));
  }
  // Copy methods over
  for (const auto& method : methods()) {
    ptr->addMethod(method);
  }
  return ptr;
}

bool ClassType::isSubtypeOfExt(const Type& rhs, std::ostream* why_not) const {
  if (rhs.castRaw<AnyClassType>()) {
    return true;
  }
  // to improve performance, this check can be cached
  if (auto iface = rhs.cast<InterfaceType>()) {
    // ClassType is not a subtype of InterfaceType if the InterfaceType is a
    // Module Interface Type but the Class Type is not a Module Class Type
    if (!is_module() && iface->is_module()) {
      if (why_not) {
        *why_not << "Class '" << repr_str() << "' is not a subtype of "
                 << "the module interface '" << rhs.repr_str()
                 << "' , only ScriptModule class can be subtype of module"
                 << " interface.\n";
      }
      return false;
    }
    for (const FunctionSchema& schema : iface->methods()) {
      auto self_method = findMethod(schema.name());
      if (!self_method) {
        if (why_not) {
          *why_not << "Class '" << repr_str() << "' does not have method '"
                   << schema.name() << "' but '" << rhs.repr_str()
                   << "' does.\n";
        }
        return false;
      }
      if (!self_method->getSchema().isSubtypeOf(
              // NOLINTNEXTLINE(bugprone-argument-comment)
              schema, /*is_method=*/true, why_not)) {
        if (why_not) {
          *why_not << "Method on class '" << repr_str()
                   << "' (1) is not compatible with interface '"
                   << rhs.repr_str() << "' (2)\n"
                   << "  (1) " << self_method->getSchema() << "\n"
                   << "  (2) " << schema << "\n";
        }
        return false;
      }
    }
    return true;
  }
  return Type::isSubtypeOfExt(rhs, why_not);
}

FunctionType::FunctionType(torch::jit::Function* function)
  : NamedType(TypeKind::FunctionType, function->qualname()),
    function_(function) {}

bool InterfaceType::isSubTypeImpl(
    const InterfaceType& lhs,
    const InterfaceType& rhs,
    std::ostream* why_not) {
  if (!lhs.is_module() && rhs.is_module()) {
    if (why_not) {
      *why_not << "Interface '" << lhs.repr_str() << "' is not a subtype of "
               << "the module interface '" << rhs.repr_str() << "'.\n";
    }
    return false;
  }
    for (const FunctionSchema& schema : *rhs.methods_) {
      auto self_schema = lhs.getMethod(schema.name());
      if (!self_schema) {
        if (why_not) {
          *why_not << "Interface '" << lhs.repr_str()
                   << "' does not have method '" << schema.name() << "' but interface '"
                   << rhs.repr_str() << "' does.\n";
        }
        return false;
      }
      // NOLINTNEXTLINE(bugprone-argument-comment)
      if (!self_schema->isSubtypeOf(schema, /*is_method=*/true, why_not)) {
        if (why_not) {
          *why_not << "Method on interface '" << lhs.repr_str()
                   << "' (1) is not compatible with interface '"
                   << rhs.repr_str() << "' (2)\n"
                   << "  (1) " << *self_schema << "\n"
                   << "  (2) " << schema << "\n";
          return false;
        }
        return false;
      }
    }
    return true;
}

bool InterfaceType::isSubtypeOfExt(const Type& rhs, std::ostream* why_not) const {
  // to improve performance this check can be cached
  if (auto iface = rhs.castRaw<InterfaceType>()) {
    return isSubTypeImpl(*this, *iface, why_not);
  }
  return Type::isSubtypeOfExt(rhs, why_not);
}

const FunctionSchema* InterfaceType::getMethod(const std::string& name) const {
  for (const FunctionSchema& method : *methods_) {
    if (method.name() == name) {
      return &method;
    }
  }
  return nullptr;
}
void InterfaceType::addMethod(FunctionSchema schema) {
  methods_->emplace_back(std::move(schema));
}
InterfaceType::InterfaceType(QualifiedName name, bool is_module)
    : NamedType(InterfaceType::Kind, std::move(name)),
      methods_(std::make_shared<std::vector<FunctionSchema>>()),
      is_module_(is_module) {}

InterfaceType::~InterfaceType() = default;

ClassTypePtr ClassType::create(
    c10::optional<QualifiedName> qualifiedName,
    std::weak_ptr<CompilationUnit> cu,
    bool is_module,
    std::string doc_string,
    std::vector<std::string> unresolved_class_attributes) {
  return ClassTypePtr(new ClassType(
      std::move(qualifiedName),
      std::move(cu),
      is_module,
      std::move(doc_string),
      std::move(unresolved_class_attributes)));
}

ClassType::ClassType(
    c10::optional<QualifiedName> name,
    std::weak_ptr<CompilationUnit> cu,
    bool is_module,
    std::string doc_string,
    std::vector<std::string> unresolved_class_attributes)
    : NamedType(TypeKind::ClassType, std::move(name)),
      compilation_unit_(std::move(cu)),
      isModule_(is_module),
      doc_string_(std::move(doc_string)),
      unresolved_class_attributes_(std::move(unresolved_class_attributes)) {}

const std::vector<torch::jit::Function*>& ClassType::methods() const {
  return methods_;
}

void ClassType::checkNotExist(const std::string& name, const std::string& what) const {
  // Check no overlap with existing constants
  for (size_t i = 0; i < constantNames_.size(); ++i) {
    TORCH_CHECK(
        name != constantNames_[i],
        "attempting to add ",
        what,
        " '",
        name,
        "' to ",
        repr_str(),
        " but a constant field of the same name already exists with value ",
        constantValues_[i]);
  }

  // Check no overlap with existing attributes
  for (const auto & attribute : attributes_) {
    TORCH_CHECK(
        name != attribute.getName(),
        "attempting to add ",
        what,
        " '",
        name,
        "' to ",
        repr_str(),
        " but an attribute field of the same name already exists with type ",
        attribute.getType()->repr_str());
  }
}

void ClassType::addAttribute(ClassAttribute classAttribute) {
    attributes_.push_back(classAttribute);
    attributeTypes_.push_back(classAttribute.getType());
    AT_ASSERT(attributes_.size() == attributeTypes_.size());
}

size_t ClassType::addAttribute(
    const std::string& name,
    TypePtr type,
    bool is_parameter,
    bool is_buffer) {
  if (is_parameter && is_buffer){
    TORCH_INTERNAL_ASSERT(false, "Attribute cannot be both a parameter and a buffer!");
  }

  std::string what = is_parameter ? "parameter" : "attribute";
  what += (is_buffer? "buffer" : "not buffer");
  checkNotExist(name, what);

  size_t slot = attributes_.size();

  AttributeKind kind = AttributeKind::REGULAR_ATTRIBUTE;
  if (is_parameter) {
    kind = AttributeKind::PARAMETER;
  } else if (is_buffer) {
    kind = AttributeKind::BUFFER;
  }


  if (is_parameter || is_buffer) {
    TORCH_INTERNAL_ASSERT(is_module(), "adding a parameter or buffer to a non module");
    TORCH_CHECK(
        (type->kind() == TensorType::Kind) ||
            (type->kind() == OptionalType::Kind &&
            type->expectRef<OptionalType>().getElementType()->kind() ==
                TensorType::Kind) ||
            (type->kind() == UnionType::Kind &&
            TensorType::get()->isSubtypeOf(type->expectRef<UnionType>())) ||
            (type->kind() == NoneType::Kind),
        "Expecting parameter or buffer to have either None, Tensor or Optional[Tensor] type, but got: ",
        toString(type));
  }

  addAttribute(ClassAttribute(kind, std::move(type), name));

  return slot;
}

void ClassType::unsafeRemoveAttribute(const std::string& name) {
  auto slot = getAttributeSlot(name);
  attributes_.erase(attributes_.begin() + slot);
  attributeTypes_.erase(attributeTypes_.begin() + slot);
  AT_ASSERT(attributes_.size() == attributeTypes_.size());
}

void ClassType::unsafeChangeAttributeType(const std::string& name, TypePtr new_ty) {
  auto slot = getAttributeSlot(name);
  auto old_attr_info = attributes_[slot];
  AT_ASSERT(old_attr_info.getKind() == AttributeKind::REGULAR_ATTRIBUTE);
  attributes_[slot] = ClassAttribute(old_attr_info.getKind(), new_ty, old_attr_info.getName());
  attributeTypes_[slot] = new_ty;
}

size_t ClassType::addConstant(const std::string& name, const IValue& value) {
  checkNotExist(name, "constant");
  size_t slot = constantNames_.size();
  constantNames_.push_back(name);
  constantValues_.push_back(value);
  return slot;
}

IValue ClassType::getConstant(const std::string& name) const {
  const auto& v = findConstant(name);
  TORCH_CHECK(
      v.has_value(),
      repr_str(),
      " does not have a constant field with name '",
      name,
      "'");
  return *v;
}

IValue ClassType::getConstant(size_t slot) const {
  TORCH_INTERNAL_ASSERT(constantNames_.size() == constantValues_.size());
  TORCH_CHECK(
      slot < constantValues_.size(),
      repr_str(),
      " does not have a constant slot of index ",
      slot);
  return constantValues_[slot];
}

c10::optional<IValue> ClassType::findConstant(const std::string& name) const {
  TORCH_INTERNAL_ASSERT(constantNames_.size() == constantValues_.size());
  size_t pos = 0;
  for (const auto& c : constantNames_) {
    if (name == c) {
      break;
    }
    ++pos;
  }

  if (pos >= constantNames_.size()) {
    return c10::nullopt;
  }
  return constantValues_[pos];
}

void ClassType::unsafeRemoveConstant(const std::string& name) {
  auto slot = getConstantSlot(name);
  constantNames_.erase(constantNames_.begin() + slot);
  constantValues_.erase(constantValues_.begin() + slot);
}

std::shared_ptr<CompilationUnit> ClassType::compilation_unit() {
  auto cu = compilation_unit_.lock();
  return cu;
}

std::shared_ptr<const CompilationUnit> ClassType::compilation_unit() const {
  auto cu = compilation_unit_.lock();
  return cu;
}

c10::optional<ClassType::Property> ClassType::getProperty(const std::string& name) {
  for (auto& prop : properties_) {
    if (name == prop.name) {
      return prop;
    }
  }

  return c10::nullopt;
}

void ClassType::addProperty(const std::string& name, torch::jit::Function* getter, torch::jit::Function* setter) {
  TORCH_INTERNAL_ASSERT(!getProperty(name), "Property named ", name, " already exists!");
  properties_.push_back({name, getter, setter});
}


static bool containsAny(const TypePtr& type) {
  std::vector<TypePtr> to_scan = { type };
  while (!to_scan.empty()) {
    const auto typ = to_scan.back();
    to_scan.pop_back();
    if (typ->kind() == AnyType::Kind) {
      return true;
    }
    for (const TypePtr& sub : typ->containedTypes()) {
      to_scan.emplace_back(sub);
    }
  }
  return false;
}

void checkNoAny(const Type& base, const char* what, const std::string& attrname, const TypePtr& attrtype) {
  TORCH_CHECK(
      !containsAny(attrtype),
      "attempting to add ",
      what,
      " '",
      attrname,
      "' of type ",
      attrtype->repr_str(),
      " to '",
      base.repr_str(),
      "' but it contains an Any type. Any types cannot be members of modules, classes, or named tuples.");
}

SymbolicShape SymbolicShape::merge(const SymbolicShape& other) const {
  if (!dims_ || !other.dims_ || dims_->size() != other.dims_->size()) {
    return SymbolicShape();
  }
  std::vector<ShapeSymbol> dims;
  for (size_t i = 0, n = dims_->size(); i < n; i++) {
    dims.push_back(merge_primitive((*dims_)[i], (*other.dims_)[i]));
  }
  return SymbolicShape(std::move(dims));
}

void SymbolicShape::dump() const {
  std::cout << *this << "\n";
}

bool EnumType::isSubtypeOfExt(const Type& rhs, std::ostream* why_not) const {
  return rhs.kind() == TypeKind::AnyType ||
      rhs.kind() == TypeKind::AnyEnumType ||
      *this == rhs ||
      Type::isSubtypeOfExt(rhs, why_not);
}

} // namespace c10
