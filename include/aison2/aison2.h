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
template<class... Ts>
struct TypeList {};

template<class T, class List>
struct Contains;

template<class T, class... Ts>
struct Contains<T, TypeList<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template<class T, class List>
struct IndexOf;

inline constexpr std::size_t kIndexNotFound = static_cast<std::size_t>(-1);

template<class T>
struct IndexOf<T, TypeList<>> : std::integral_constant<std::size_t, kIndexNotFound> {};

template<class T, class Head, class... Tail>
struct IndexOf<T, TypeList<Head, Tail...>>
    : std::conditional_t<
          std::is_same_v<T, Head>,
          std::integral_constant<std::size_t, 0>,
          std::conditional_t<
              (IndexOf<T, TypeList<Tail...>>::value == kIndexNotFound),
              std::integral_constant<std::size_t, kIndexNotFound>,
              std::integral_constant<std::size_t, 1 + IndexOf<T, TypeList<Tail...>>::value>>> {};

template<class List>
struct TypeListSize;

template<class... Ts>
struct TypeListSize<TypeList<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template<class ListA, class ListB>
struct ConcatTwo;

template<class... As, class... Bs>
struct ConcatTwo<TypeList<As...>, TypeList<Bs...>> {
    using Type = TypeList<As..., Bs...>;
};

template<class... Lists>
struct Concat;

template<class List>
struct Concat<List> {
    using Type = List;
};

template<class ListA, class ListB, class... Rest>
struct Concat<ListA, ListB, Rest...> {
    using Type = typename Concat<typename ConcatTwo<ListA, ListB>::Type, Rest...>::Type;
};

template<class List>
struct MakeUnique;

template<>
struct MakeUnique<TypeList<>> {
    using Type = TypeList<>;
};

template<class Head, class... Tail>
struct MakeUnique<TypeList<Head, Tail...>> {
private:
    using TailUnique = typename MakeUnique<TypeList<Tail...>>::Type;

public:
    using Type = std::conditional_t<
        Contains<Head, TailUnique>::value,
        TailUnique,
        typename ConcatTwo<TypeList<Head>, TailUnique>::Type>;
};

template<class Needed, class Declared>
struct AllContained;

template<class Declared>
struct AllContained<TypeList<>, Declared> : std::true_type {};

template<class Head, class... Tail, class Declared>
struct AllContained<TypeList<Head, Tail...>, Declared>
    : std::conditional_t<
          Contains<Head, Declared>::value,
          AllContained<TypeList<Tail...>, Declared>,
          std::false_type> {};

// Dependencies only track class/enum types; fundamentals are treated as
// built-ins that do not require schema declarations.
template<class T>
struct DependencyFor {
    using Type =
        std::conditional_t<std::is_class_v<T> || std::is_enum_v<T>, TypeList<T>, TypeList<>>;
};

template<class T>
struct DependencyFor<std::optional<T>> {
    using Type = typename DependencyFor<T>::Type;
};

template<class T, class Alloc>
struct DependencyFor<std::vector<T, Alloc>> {
    using Type = typename DependencyFor<T>::Type;
};

template<class CharT, class Traits, class Alloc>
struct DependencyFor<std::basic_string<CharT, Traits, Alloc>> {
    using Type = TypeList<>;
};

template<class... Ts>
struct DependencyFor<std::variant<Ts...>> {
    using Type = typename MakeUnique<
        typename Concat<TypeList<>, typename DependencyFor<Ts>::Type...>::Type>::Type;
};

// Field plumbing -------------------------------------------------------------
template<class Owner, class Field>
struct FieldDef {
    using OwnerType = Owner;
    using FieldType = Field;
    const char* name;
    Field Owner::* ptr;
};

template<class... FieldDefs>
struct FieldList {
    std::tuple<FieldDefs...> fields;

    constexpr explicit FieldList(FieldDefs... defs)
        : fields(std::move(defs)...)
    {}
};

template<class... FieldDefs>
FieldList(FieldDefs...) -> FieldList<FieldDefs...>;

template<class... FieldDefs>
struct Fields : FieldList<FieldDefs...> {
    using FieldList<FieldDefs...>::FieldList;
};

template<class... FieldDefs>
Fields(FieldDefs...) -> Fields<FieldDefs...>;

template<class Owner>
struct ObjectContext {
    template<class Field>
    constexpr auto add(Field Owner::* ptr, const char* name) const
    {
        return FieldDef<Owner, Field>{name, ptr};
    }
};

template<class FieldDefs>
struct FieldDependencies;

template<class... FieldDefs>
struct FieldDependencies<FieldList<FieldDefs...>> {
private:
    template<class FieldDefT>
    using Dependency = typename DependencyFor<typename FieldDefT::FieldType>::Type;

public:
    using Type =
        typename MakeUnique<typename Concat<TypeList<>, Dependency<FieldDefs>...>::Type>::Type;
};

template<class... FieldDefs>
struct FieldDependencies<Fields<FieldDefs...>> : FieldDependencies<FieldList<FieldDefs...>> {};

// Enum plumbing --------------------------------------------------------------
template<class T>
struct EnumValue {
    const char* name;
    T value;
};

template<class... Values>
struct EnumValues {
    std::tuple<Values...> values;

    constexpr explicit EnumValues(Values... v)
        : values(std::move(v)...)
    {}
};

template<class... Values>
EnumValues(Values...) -> EnumValues<Values...>;

template<class T>
struct EnumContext {
    constexpr auto value(const char* name, T v) const { return EnumValue<T>{name, v}; }
};

// Variant plumbing -----------------------------------------------------------
template<class Variant>
struct VariantTypeInfo;

template<class... Alternatives>
struct VariantTypeInfo<std::variant<Alternatives...>> {
    using AlternativesList = TypeList<Alternatives...>;
    static constexpr std::size_t kSize = sizeof...(Alternatives);
};

struct VariantConfig {
    const char* tag = "type";
};

template<class Alt>
struct VariantAlternative {
    using Type = Alt;
    const char* tag;
};

template<class... Alternatives>
struct VariantAlternatives {
    std::tuple<Alternatives...> alternatives;

    constexpr explicit VariantAlternatives(Alternatives... alts)
        : alternatives(std::move(alts)...)
    {}
};

template<class... Alternatives>
VariantAlternatives(Alternatives...) -> VariantAlternatives<Alternatives...>;

template<class VariantType>
class VariantContext
{
public:
    constexpr VariantContext() = default;

    template<class Alt>
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

    template<class TargetVariant = VariantType>
    constexpr auto finalize() const
    {
        using Info = VariantTypeInfo<TargetVariant>;
        return finalizeImpl(typename Info::AlternativesList{});
    }

private:
    template<class... Alternatives>
    constexpr auto finalizeImpl(TypeList<Alternatives...>) const
    {
        for (bool isSeen : seen_) {
            assert(isSeen && "All variant alternatives must be registered");
        }

        return VariantAlternatives<VariantAlternative<Alternatives>...>{
            VariantAlternative<Alternatives>{
                tags_[IndexOf<Alternatives, TypeList<Alternatives...>>::value]}...};
    }

    std::array<const char*, VariantTypeInfo<VariantType>::kSize> tags_{};
    std::array<bool, VariantTypeInfo<VariantType>::kSize> seen_{};
};

template<class Alternatives>
struct VariantDependencies;

template<class... Alternatives>
struct VariantDependencies<VariantAlternatives<Alternatives...>> {
private:
    template<class Alt>
    using Dependency = typename DependencyFor<typename Alt::Type>::Type;

public:
    using Type =
        typename MakeUnique<typename Concat<TypeList<>, Dependency<Alternatives>...>::Type>::Type;
};

// Def traits -----------------------------------------------------------------
template<class T, class Fields>
struct ObjectDef {
    using Type = T;
    using FieldsType = Fields;
    using Deps = typename FieldDependencies<FieldsType>::Type;

    FieldsType fields;
};

template<class T, class Values>
struct EnumDef {
    using Type = T;
    using ValuesType = Values;
    using Deps = TypeList<>;

    ValuesType values;
};

template<class T, class Alternatives>
struct VariantDef {
    using Type = T;
    using AlternativesType = Alternatives;
    using Deps = typename VariantDependencies<AlternativesType>::Type;

    VariantConfig config{};
    AlternativesType alternatives;
};

// Custom plumbing ------------------------------------------------------------
template<class T, class EncoderFn, class DecoderFn>
struct CustomDef {
    using Type = T;
    using Encoder = EncoderFn;
    using Decoder = DecoderFn;
    using Deps = TypeList<>;

    Encoder encoder;
    Decoder decoder;
};

template<class T>
struct DeclareDef {
    using Type = T;
    using Deps = TypeList<>;
};

template<class Def>
struct DefTraits;

template<class T, class Fields>
struct DefTraits<ObjectDef<T, Fields>> {
    using Type = T;
    using Deps = typename ObjectDef<T, Fields>::Deps;
    static constexpr bool isDeclare = false;
};

template<class T, class Values>
struct DefTraits<EnumDef<T, Values>> {
    using Type = T;
    using Deps = typename EnumDef<T, Values>::Deps;
    static constexpr bool isDeclare = false;
};

template<class T, class Alternatives>
struct DefTraits<VariantDef<T, Alternatives>> {
    using Type = T;
    using Deps = typename VariantDef<T, Alternatives>::Deps;
    static constexpr bool isDeclare = false;
};

template<class T>
struct DefTraits<DeclareDef<T>> {
    using Type = T;
    using Deps = typename DeclareDef<T>::Deps;
    static constexpr bool isDeclare = true;
};

template<class T, class EncoderFn, class DecoderFn>
struct DefTraits<CustomDef<T, EncoderFn, DecoderFn>> {
    using Type = T;
    using Deps = typename CustomDef<T, EncoderFn, DecoderFn>::Deps;
    static constexpr bool isDeclare = false;
};

template<class Def>
using TypeFromDef = std::
    conditional_t<DefTraits<Def>::isDeclare, TypeList<>, TypeList<typename DefTraits<Def>::Type>>;

template<class Def>
using DeclaredFromDef = TypeList<typename DefTraits<Def>::Type>;

template<class Def>
using DepsFromDef = typename DefTraits<Def>::Deps;

}  // namespace detail

// Public API -----------------------------------------------------------------
template<class T, class F>
constexpr auto Object(F&& fn)  // NOLINT(readability-identifier-naming)
{
    detail::ObjectContext<T> ctx;
    auto fields = fn(ctx);
    using FieldsType = std::decay_t<decltype(fields)>;
    return detail::ObjectDef<T, FieldsType>{std::move(fields)};
}

template<class T, class F>
constexpr auto Enum(F&& fn)  // NOLINT(readability-identifier-naming)
{
    detail::EnumContext<T> ctx;
    auto values = fn(ctx);
    using ValuesType = std::decay_t<decltype(values)>;
    return detail::EnumDef<T, ValuesType>{std::move(values)};
}

template<class T>
constexpr auto Declare()  // NOLINT(readability-identifier-naming)
{
    return detail::DeclareDef<T>{};
}

template<class T, class EncoderFn, class DecoderFn>
constexpr auto Custom(
    EncoderFn encoder, DecoderFn decoder)  // NOLINT(readability-identifier-naming)
{
    return detail::CustomDef<T, EncoderFn, DecoderFn>{std::move(encoder), std::move(decoder)};
}

template<class T, class F>
constexpr auto Variant(
    detail::VariantConfig config, F&& fn)  // NOLINT(readability-identifier-naming)
{
    detail::VariantContext<T> ctx{};
    fn(ctx);
    auto alternatives = ctx.template finalize<T>();
    return detail::VariantDef<T, decltype(alternatives)>{config, std::move(alternatives)};
}

template<class T, class F>
constexpr auto Variant(F&& fn)  // NOLINT(readability-identifier-naming)
{
    return Variant<T>(detail::VariantConfig{}, std::forward<F>(fn));
}

template<class... Defs>
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

    template<class T>
    static constexpr bool defines()
    {
        return detail::Contains<T, DefinedTypes>::value;
    }

    template<class T>
    static constexpr bool declares()
    {
        return detail::Contains<T, DeclaredTypes>::value;
    }
};

template<class... Defs>
Schema(Defs...) -> Schema<Defs...>;

using detail::EnumValue;
using detail::EnumValues;
using detail::Fields;
using detail::VariantAlternatives;
using detail::VariantConfig;
using detail::VariantContext;

}  // namespace aison2
