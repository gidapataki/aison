#pragma once

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
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

template<class ListA, class ListB>
struct ConcatTwo;

template<class... As, class... Bs>
struct ConcatTwo<TypeList<As...>, TypeList<Bs...>> {
    using type = TypeList<As..., Bs...>;
};

template<class... Lists>
struct Concat;

template<class List>
struct Concat<List> {
    using type = List;
};

template<class ListA, class ListB, class... Rest>
struct Concat<ListA, ListB, Rest...> {
    using type = typename Concat<typename ConcatTwo<ListA, ListB>::type, Rest...>::type;
};

template<class List>
struct MakeUnique;

template<>
struct MakeUnique<TypeList<>> {
    using type = TypeList<>;
};

template<class Head, class... Tail>
struct MakeUnique<TypeList<Head, Tail...>> {
private:
    using TailUnique = typename MakeUnique<TypeList<Tail...>>::type;

public:
    using type = std::conditional_t<
        Contains<Head, TailUnique>::value,
        TailUnique,
        typename ConcatTwo<TypeList<Head>, TailUnique>::type>;
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
    using type =
        std::conditional_t<std::is_class_v<T> || std::is_enum_v<T>, TypeList<T>, TypeList<>>;
};

template<class T>
struct DependencyFor<std::optional<T>> {
    using type = typename DependencyFor<T>::type;
};

template<class T, class Alloc>
struct DependencyFor<std::vector<T, Alloc>> {
    using type = typename DependencyFor<T>::type;
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
    using Dependency = typename DependencyFor<typename FieldDefT::FieldType>::type;

public:
    using type =
        typename MakeUnique<typename Concat<TypeList<>, Dependency<FieldDefs>...>::type>::type;
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

// Def traits -----------------------------------------------------------------
template<class T, class Fields>
struct ObjectDef {
    using Type = T;
    using FieldsType = Fields;
    using Deps = typename FieldDependencies<FieldsType>::type;

    FieldsType fields;
};

template<class T, class Values>
struct EnumDef {
    using Type = T;
    using ValuesType = Values;
    using Deps = TypeList<>;

    ValuesType values;
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

template<class T>
struct DefTraits<DeclareDef<T>> {
    using Type = T;
    using Deps = typename DeclareDef<T>::Deps;
    static constexpr bool isDeclare = true;
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
constexpr auto Object(F&& fn)
{
    detail::ObjectContext<T> ctx;
    auto fields = fn(ctx);
    using FieldsType = std::decay_t<decltype(fields)>;
    return detail::ObjectDef<T, FieldsType>{std::move(fields)};
}

template<class T, class F>
constexpr auto Enum(F&& fn)
{
    detail::EnumContext<T> ctx;
    auto values = fn(ctx);
    using ValuesType = std::decay_t<decltype(values)>;
    return detail::EnumDef<T, ValuesType>{std::move(values)};
}

template<class T>
constexpr auto Declare()
{
    return detail::DeclareDef<T>{};
}

template<class... Defs>
struct Schema {
    constexpr Schema(Defs... inDefs) : defs(std::move(inDefs)...) {}

    using DefinedTypes = typename detail::MakeUnique<
        typename detail::Concat<detail::TypeList<>, detail::TypeFromDef<Defs>...>::type>::type;

    using DeclaredTypes = typename detail::MakeUnique<
        typename detail::Concat<DefinedTypes, detail::DeclaredFromDef<Defs>...>::type>::type;

    using DepTypes = typename detail::MakeUnique<
        typename detail::Concat<detail::TypeList<>, detail::DepsFromDef<Defs>...>::type>::type;

    static_assert(
        detail::AllContained<DepTypes, DeclaredTypes>::value,
        "Schema is missing definitions or declarations for referenced types");

    std::tuple<Defs...> defs;

    constexpr const std::tuple<Defs...>& definitions() const
    {
        return defs;
    }

    static constexpr std::size_t size()
    {
        return sizeof...(Defs);
    }

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

}  // namespace aison2
