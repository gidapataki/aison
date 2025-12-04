#pragma once

#include <array>
#include <cassert>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace aison2 {
namespace detail {

// Simple typelist helpers ----------------------------------------------------
template<typename... Ts>
struct TypeList {};

template<typename T, typename List>
struct Contains;

template<typename T, typename... Ts>
struct Contains<T, TypeList<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template<typename T, typename List>
struct IndexOf;

inline constexpr std::size_t kIndexNotFound = static_cast<std::size_t>(-1);

template<typename T>
struct IndexOf<T, TypeList<>> : std::integral_constant<std::size_t, kIndexNotFound> {};

template<typename T, typename Head, typename... Tail>
struct IndexOf<T, TypeList<Head, Tail...>>
    : std::conditional_t<
          std::is_same_v<T, Head>,
          std::integral_constant<std::size_t, 0>,
          std::conditional_t<
              (IndexOf<T, TypeList<Tail...>>::value == kIndexNotFound),
              std::integral_constant<std::size_t, kIndexNotFound>,
              std::integral_constant<std::size_t, 1 + IndexOf<T, TypeList<Tail...>>::value>>> {};

template<typename List>
struct TypeListSize;

template<typename... Ts>
struct TypeListSize<TypeList<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template<typename T>
struct IsTuple : std::false_type {};
template<typename... Ts>
struct IsTuple<std::tuple<Ts...>> : std::true_type {};

template<typename ListA, typename ListB>
struct ConcatTwo;

template<typename... As, typename... Bs>
struct ConcatTwo<TypeList<As...>, TypeList<Bs...>> {
    using Type = TypeList<As..., Bs...>;
};

template<typename... Lists>
struct Concat;

template<typename List>
struct Concat<List> {
    using Type = List;
};

template<typename ListA, typename ListB, typename... Rest>
struct Concat<ListA, ListB, Rest...> {
    using Type = typename Concat<typename ConcatTwo<ListA, ListB>::Type, Rest...>::Type;
};

template<typename List>
struct MakeUnique;

template<>
struct MakeUnique<TypeList<>> {
    using Type = TypeList<>;
};

template<typename Head, typename... Tail>
struct MakeUnique<TypeList<Head, Tail...>> {
private:
    using TailUnique = typename MakeUnique<TypeList<Tail...>>::Type;

public:
    using Type = std::conditional_t<
        Contains<Head, TailUnique>::value,
        TailUnique,
        typename ConcatTwo<TypeList<Head>, TailUnique>::Type>;
};

template<typename Needed, typename Declared>
struct AllContained;

template<typename Declared>
struct AllContained<TypeList<>, Declared> : std::true_type {};

template<typename Head, typename... Tail, typename Declared>
struct AllContained<TypeList<Head, Tail...>, Declared>
    : std::conditional_t<
          Contains<Head, Declared>::value,
          AllContained<TypeList<Tail...>, Declared>,
          std::false_type> {};

// Dependencies only track class/enum types; fundamentals are treated as
// built-ins that do not require schema declarations.
template<typename T>
struct DependencyFor {
    using Type =
        std::conditional_t<std::is_class_v<T> || std::is_enum_v<T>, TypeList<T>, TypeList<>>;
};

template<typename T>
struct DependencyFor<std::optional<T>> {
    using Type = typename DependencyFor<T>::Type;
};

template<typename T, typename Alloc>
struct DependencyFor<std::vector<T, Alloc>> {
    using Type = typename DependencyFor<T>::Type;
};

template<typename CharT, typename Traits, typename Alloc>
struct DependencyFor<std::basic_string<CharT, Traits, Alloc>> {
    using Type = TypeList<>;
};

template<typename... Ts>
struct DependencyFor<std::variant<Ts...>> {
    using Type = typename MakeUnique<
        typename Concat<TypeList<>, typename DependencyFor<Ts>::Type...>::Type>::Type;
};

// Field plumbing -------------------------------------------------------------
template<typename Owner, typename Field>
struct FieldDef {
    using OwnerType = Owner;
    using FieldType = Field;
    const char* name;
    Field Owner::* ptr;
};

template<typename... FieldDefs>
struct FieldList {
    std::tuple<FieldDefs...> fields;

    constexpr explicit FieldList(FieldDefs... defs)
        : fields(std::move(defs)...)
    {}
};

template<typename... FieldDefs>
FieldList(FieldDefs...) -> FieldList<FieldDefs...>;

template<typename... FieldDefs>
struct Fields : FieldList<FieldDefs...> {
    using FieldList<FieldDefs...>::FieldList;
};

template<typename... FieldDefs>
Fields(FieldDefs...) -> Fields<FieldDefs...>;

template<typename Owner>
struct ObjectContext {
    template<typename Field>
    constexpr auto add(Field Owner::* ptr, const char* name) const
    {
        return FieldDef<Owner, Field>{name, ptr};
    }
};

template<typename FieldDefs>
struct FieldDependencies;

template<typename... FieldDefs>
struct FieldDependencies<FieldList<FieldDefs...>> {
private:
    template<typename FieldDefT>
    using Dependency = typename DependencyFor<typename FieldDefT::FieldType>::Type;

public:
    using Type =
        typename MakeUnique<typename Concat<TypeList<>, Dependency<FieldDefs>...>::Type>::Type;
};

template<typename... FieldDefs>
struct FieldDependencies<Fields<FieldDefs...>> : FieldDependencies<FieldList<FieldDefs...>> {};

// Enum plumbing --------------------------------------------------------------
template<typename T>
struct EnumValue {
    const char* name;
    T value;
};

template<typename... Values>
struct EnumValues {
    std::tuple<Values...> values;

    constexpr explicit EnumValues(Values... v)
        : values(std::move(v)...)
    {}
};

template<typename... Values>
EnumValues(Values...) -> EnumValues<Values...>;

template<typename T>
struct EnumContext {
    constexpr auto value(const char* name, T v) const { return EnumValue<T>{name, v}; }
};

// Variant plumbing -----------------------------------------------------------
template<typename Variant>
struct VariantTypeInfo;

template<typename... Ts>
struct VariantTypeInfo<std::variant<Ts...>> {
    using AlternativesList = TypeList<Ts...>;
    static constexpr std::size_t kSize = sizeof...(Ts);
};

struct VariantConfig {
    const char* tag = "type";
};

template<typename T>
struct NamedType {
    using Type = T;
    const char* tag;
};

template<typename... Ts>
struct Types {
    std::tuple<Ts...> alternatives;

    constexpr explicit Types(Ts... alts)
        : alternatives(std::move(alts)...)
    {}
};

template<typename... Ts>
Types(Ts...) -> Types<Ts...>;

template<typename VariantType>
class VariantContext
{
public:
    constexpr VariantContext() = default;

    template<typename Alt>
    constexpr void add(const char* tag)
    {
        using Info = VariantTypeInfo<VariantType>;
        static_assert(
            IndexOf<Alt, typename Info::AlternativesList>::value != kIndexNotFound,
            "Variant alternative must be part of the variant type");

        constexpr std::size_t index = IndexOf<Alt, typename Info::AlternativesList>::value;
        tags_[index] = tag;
        seen_[index] = true;
    }

    template<typename TargetVariant = VariantType>
    constexpr auto finalize() const
    {
        using Info = VariantTypeInfo<TargetVariant>;
        return finalizeImpl(typename Info::AlternativesList{});
    }

private:
    template<typename... Ts>
    constexpr auto finalizeImpl(TypeList<Ts...>) const
    {
        for (bool isSeen : seen_) {
            assert(isSeen && "All variant alternatives must be registered");
        }

        return Types<NamedType<Ts>...>{
            NamedType<Ts>{tags_[IndexOf<Ts, TypeList<Ts...>>::value]}...};
    }

    std::array<const char*, VariantTypeInfo<VariantType>::kSize> tags_{};
    std::array<bool, VariantTypeInfo<VariantType>::kSize> seen_{};
};

template<typename Alternatives>
struct VariantDependencies;

template<typename... Ts>
struct VariantDependencies<Types<Ts...>> {
private:
    template<typename Alt>
    using Dependency = typename DependencyFor<typename Alt::Type>::Type;

public:
    using Type = typename MakeUnique<typename Concat<TypeList<>, Dependency<Ts>...>::Type>::Type;
};

// Def traits -----------------------------------------------------------------
template<typename T, typename Fields>
struct ObjectDef {
    using Type = T;
    using FieldsType = Fields;
    using Deps = typename FieldDependencies<FieldsType>::Type;

    FieldsType fields;
};

template<typename T, typename Values>
struct EnumDef {
    using Type = T;
    using ValuesType = Values;
    using Deps = TypeList<>;

    ValuesType values;
};

template<typename T, typename Alternatives>
struct VariantDef {
    using Type = T;
    using AlternativesType = Alternatives;
    using Deps = typename VariantDependencies<AlternativesType>::Type;

    VariantConfig config{};
    AlternativesType alternatives;
};

// Custom plumbing ------------------------------------------------------------
template<typename T, typename EncoderFn, typename DecoderFn>
struct CustomDef {
    using Type = T;
    using Encoder = EncoderFn;
    using Decoder = DecoderFn;
    using Deps = TypeList<>;

    Encoder encoder;
    Decoder decoder;
};

template<typename T>
struct DeclareDef {
    using Type = T;
    using Deps = TypeList<>;
};

template<typename Def>
struct DefTraits;

template<typename T, typename Fields>
struct DefTraits<ObjectDef<T, Fields>> {
    using Type = T;
    using Deps = typename ObjectDef<T, Fields>::Deps;
    static constexpr bool isDeclare = false;
};

template<typename T, typename Values>
struct DefTraits<EnumDef<T, Values>> {
    using Type = T;
    using Deps = typename EnumDef<T, Values>::Deps;
    static constexpr bool isDeclare = false;
};

template<typename T, typename Alternatives>
struct DefTraits<VariantDef<T, Alternatives>> {
    using Type = T;
    using Deps = typename VariantDef<T, Alternatives>::Deps;
    static constexpr bool isDeclare = false;
};

template<typename T>
struct DefTraits<DeclareDef<T>> {
    using Type = T;
    using Deps = typename DeclareDef<T>::Deps;
    static constexpr bool isDeclare = true;
};

template<typename T, typename EncoderFn, typename DecoderFn>
struct DefTraits<CustomDef<T, EncoderFn, DecoderFn>> {
    using Type = T;
    using Deps = typename CustomDef<T, EncoderFn, DecoderFn>::Deps;
    static constexpr bool isDeclare = false;
};

template<typename Def>
using TypeFromDef = std::
    conditional_t<DefTraits<Def>::isDeclare, TypeList<>, TypeList<typename DefTraits<Def>::Type>>;

template<typename Def>
using DeclaredFromDef = TypeList<typename DefTraits<Def>::Type>;

template<typename Def>
using DepsFromDef = typename DefTraits<Def>::Deps;

}  // namespace detail

// Public API -----------------------------------------------------------------
template<typename Owner, typename Field>
constexpr auto field(Field Owner::* ptr, const char* name)
{
    return detail::FieldDef<Owner, Field>{name, ptr};
}

template<typename T, typename FieldsType>
constexpr auto object(FieldsType fields)
{
    return detail::ObjectDef<T, FieldsType>{std::move(fields)};
}

template<typename T>
constexpr auto value(T v, const char* name)
{
    return detail::EnumValue<T>{name, v};
}

template<typename T, typename ValuesType>
constexpr auto enumeration(ValuesType values)
{
    return detail::EnumDef<T, ValuesType>{std::move(values)};
}

template<typename T>
constexpr auto declare()
{
    return detail::DeclareDef<T>{};
}

template<typename T, typename EncoderFn, typename DecoderFn>
constexpr auto custom(EncoderFn encoder, DecoderFn decoder)
{
    return detail::CustomDef<T, EncoderFn, DecoderFn>{std::move(encoder), std::move(decoder)};
}

template<typename T, typename AlternativesType>
constexpr auto variant(detail::VariantConfig config, AlternativesType alts)
{
    return detail::VariantDef<T, AlternativesType>{config, std::move(alts)};
}

template<typename T, typename AlternativesType>
constexpr auto variant(AlternativesType alts)
{
    return detail::VariantDef<T, AlternativesType>{detail::VariantConfig{}, std::move(alts)};
}

template<typename T>
constexpr auto type(const char* tag)
{
    return detail::NamedType<T>{tag};
}

template<typename... Defs>
struct Schema {
    constexpr Schema(Defs... inDefs)
        : defs(std::move(inDefs)...)
    {}

    using DefinedTypes = typename detail::MakeUnique<
        typename detail::Concat<detail::TypeList<>, detail::TypeFromDef<Defs>...>::Type>::Type;

    using DeclaredTypes = typename detail::MakeUnique<
        typename detail::Concat<DefinedTypes, detail::DeclaredFromDef<Defs>...>::Type>::Type;

    using DepTypes = typename detail::MakeUnique<
        typename detail::Concat<detail::TypeList<>, detail::DepsFromDef<Defs>...>::Type>::Type;

    static_assert(
        detail::AllContained<DepTypes, DeclaredTypes>::value,
        "Schema is missing definitions or declarations for referenced types");

    std::tuple<Defs...> defs;

    constexpr const std::tuple<Defs...>& definitions() const { return defs; }

    static constexpr std::size_t size() { return sizeof...(Defs); }

    template<typename T>
    static constexpr bool defines()
    {
        return detail::Contains<T, DefinedTypes>::value;
    }

    template<typename T>
    static constexpr bool declares()
    {
        return detail::Contains<T, DeclaredTypes>::value;
    }
};

template<typename... Defs>
Schema(Defs...) -> Schema<Defs...>;

namespace detail {
template<typename Tuple, std::size_t... I>
constexpr auto tupleToSchema(Tuple&& defs, std::index_sequence<I...>)
{
    return Schema<std::tuple_element_t<I, std::decay_t<Tuple>>...>{
        std::get<I>(std::forward<Tuple>(defs))...};
}

template<typename Tuple>
constexpr auto makeSchemaFromTuple(Tuple&& defs)
{
    constexpr std::size_t kSize = std::tuple_size_v<std::decay_t<Tuple>>;
    return tupleToSchema(std::forward<Tuple>(defs), std::make_index_sequence<kSize>{});
}
}  // namespace detail

template<typename Tuple>
constexpr auto schema(Tuple&& defs)
{
    static_assert(detail::IsTuple<std::decay_t<Tuple>>::value, "schema expects std::tuple");
    return detail::makeSchemaFromTuple(std::forward<Tuple>(defs));
}

using detail::EnumValue;
using detail::EnumValues;
using detail::Fields;
using detail::Types;
using detail::VariantConfig;
using detail::VariantContext;

}  // namespace aison2
