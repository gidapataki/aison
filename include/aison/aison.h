#pragma once
#include <json/json.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// Forward declarations ////////////////////////////////////////////////////////////////////////////

namespace aison {

struct EmptyConfig;
struct EncodeOnly;
struct DecodeOnly;
struct EncodeDecode;

struct Error;
struct Result;
struct SchemaDefaults;

template<typename Schema, typename T>
struct Object;

template<typename Schema, typename T>
struct Variant;

template<typename Schema, typename T>
struct Enum;

template<typename Schema, typename T>
struct Custom;

template<typename Derived, typename FacetTag, typename Config>
struct Schema;

// Introspection types

using TypeId = const void*;

enum class TypeClass;
struct TypeInfo;
struct FieldInfo;
struct ObjectInfo;
struct EnumInfo;
struct VariantInfo;
struct AlternativeInfo;

template<typename Schema>
class Introspection;

}  // namespace aison

namespace aison::detail {

class Context;
struct PathSegment;
struct PathGuard;
struct EnumBase;

using FieldAccessorDeleter = void (*)(void*);
using FieldAccessorStorage = std::unique_ptr<void, FieldAccessorDeleter>;
using FieldAccessorPtr = const void*;
using FieldAccessorId = const void*;

template<typename Schema>
class EncodeContext;

template<typename Schema>
class DecodeContext;

template<typename Schema, typename T>
class EnumImpl;

template<typename Schema, typename Owner>
class ObjectImpl;

template<typename Schema, typename Variant>
class VariantImpl;

template<typename Schema, typename Variant>
struct VariantDecoder;

template<typename Owner, typename T>
struct FieldAccessor;

template<typename Schema>
class IntrospectionRegistry;

template<typename Schema>
class IntrospectionImpl;

// Traits
template<typename T>
struct IsOptional;

template<typename T>
struct IsVector;

template<typename T>
struct IsVariant;

template<typename Schema, typename T, typename = void>
struct HasEnumTag;

template<typename Schema, typename T, typename = void>
struct HasObjectTag;

template<typename Schema, typename Variant, typename = void>
struct HasVariantTag;

template<typename Schema, typename T, typename = void>
struct HasCustomTag;

template<typename Schema, typename = void>
struct HasSchemaEnableAssert;

template<typename Schema, typename = void>
struct HasSchemaStrictOptional;

template<typename Schema, typename = void>
struct HasSchemaEnableIntrospection;

template<typename Schema, typename = void>
struct SchemaEnableAssert;

template<typename Schema, typename = void>
struct SchemaStrictOptional;

template<typename Schema, typename = void>
struct SchemaEnableIntrospection;

// Functions
template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, EncodeContext<Schema>& ctx);

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, DecodeContext<Schema>& ctx);

template<typename Schema, typename T>
void encodeDefault(const T& value, Json::Value& dst, EncodeContext<Schema>& ctx);

template<typename Schema, typename T>
void decodeDefault(const Json::Value& src, T& value, DecodeContext<Schema>& ctx);

template<typename Schema>
constexpr bool hasEncodeFacet();

template<typename Schema>
constexpr bool hasDecodeFacet();

template<typename Schema, typename T>
constexpr void validateEnumType();

template<typename Schema, typename Variant>
constexpr void validateVariant();

template<typename Owner, typename T>
FieldAccessorId getFieldAccessorId();

template<typename Owner, typename T>
FieldAccessorStorage makeFieldAccessor(T Owner::* member);

template<typename T>
T& getSchemaObject();

template<typename Schema>
constexpr bool getSchemaEnableAssert();

template<typename Schema>
constexpr bool getSchemaStrictOptional();

template<typename Schema>
constexpr bool getSchemaEnableIntrospection();

template<typename Schema>
IntrospectionRegistry<Schema>& getIntrospectionRegistry();

template<typename T>
TypeId getTypeId();

template<typename Schema, typename Owner, typename T>
void ensureTypeRegistration();

template<typename Schema, typename T>
void registerObjectMapping();

template<typename Schema, typename Variant>
void registerVariantMapping();

template<typename Schema, typename E>
void registerEnumMapping();

template<typename Schema, typename T>
void registerCustomMapping();

template<typename Schema, typename Owner, typename Variant, std::size_t... Is>
void ensureVariantAlternatives(std::index_sequence<Is...>);

}  // namespace aison::detail

// Implementation //////////////////////////////////////////////////////////////////////////////////

namespace aison {

struct Error {
    std::string path;
    std::string message;
};

struct Result {
    std::vector<Error> errors;

    explicit operator bool() const { return errors.empty(); }
};

struct EmptyConfig {};

// Facet tag types

struct EncodeOnly {};
struct DecodeOnly {};
struct EncodeDecode {};

struct SchemaDefaults {
    static constexpr auto enableAssert = true;
    static constexpr auto strictOptional = true;
    static constexpr auto enableIntrospection = false;
};

template<typename Derived, typename Facet = EncodeDecode, typename Config = EmptyConfig>
struct Schema {
    using SchemaTag = void;
    using FacetType = Facet;
    using ConfigType = Config;

    // template<typename T> struct Object;
    // template<typename Variant> struct Variant;
    // template<typename T> struct Enum;
    // template<typename T> struct Custom;
};

enum class TypeClass {
    Unknown,
    Integral,  //< Size & signedness available via typeInfo.data.numeric
    Floating,  //< Size available via typeInfo.data.numeric
    Bool,
    String,
    Enum,
    Object,
    Custom,    //< Custom encoder/decoder mapping
    Optional,  //< Inner type available via typeInfo.data.element
    Vector,    //< Inner type available via typeInfo.data.element
    Variant,   //< Inner type available via typeInfo.data.variant
};

struct TypeInfo {
    TypeClass cls = TypeClass::Unknown;
    TypeId typeId = nullptr;
    const char* name = nullptr;
    union Data {
        struct {
            const TypeInfo* type;  //< Inner typeInfo for optional
        } optional;
        struct {
            const TypeInfo* type;  //< Inner typeInfo for vector
        } vector;
        struct {
            const TypeInfo* const* types;  //< Variant alternative types
            std::size_t count;             //< Variant alternative count
        } variant;
        struct {
            int size;       //< Size in bytes
            bool isSigned;  //< Signedness
        } integral;
        struct {
            int size;  //< Size in bytes
        } floating;
    } data = {};

    constexpr TypeInfo() = default;

    template<typename T>
    static constexpr TypeInfo scalar(TypeClass c)
    {
        TypeInfo t;
        t.cls = c;
        t.typeId = detail::getTypeId<T>();
        return t;
    }

    template<typename T>
    static constexpr TypeInfo integral()
    {
        TypeInfo t;
        t.cls = TypeClass::Integral;
        t.typeId = detail::getTypeId<T>();
        t.data.integral = {sizeof(T), std::is_signed_v<T>};
        return t;
    }

    template<typename T>
    static constexpr TypeInfo floating()
    {
        TypeInfo t;
        t.cls = TypeClass::Floating;
        t.typeId = detail::getTypeId<T>();
        t.data.floating = {sizeof(T)};
        return t;
    }

    template<typename T>
    static constexpr TypeInfo optional(const TypeInfo* type)
    {
        TypeInfo t;
        t.cls = TypeClass::Optional;
        t.typeId = detail::getTypeId<T>();
        t.data.optional.type = type;
        return t;
    }

    template<typename T>
    static constexpr TypeInfo vector(const TypeInfo* type)
    {
        TypeInfo t;
        t.cls = TypeClass::Vector;
        t.typeId = detail::getTypeId<T>();
        t.data.vector.type = type;
        return t;
    }

    template<typename T>
    static constexpr TypeInfo variant(const TypeInfo* const* vars, std::size_t count)
    {
        TypeInfo t;
        t.cls = TypeClass::Variant;
        t.typeId = detail::getTypeId<T>();
        t.data.variant = {vars, count};
        return t;
    }
};

struct FieldInfo {
    std::string name;
    const TypeInfo* type = nullptr;
};

struct ObjectInfo {
    std::string name;
    std::vector<FieldInfo> fields;
};

struct EnumInfo {
    std::string name;
    std::vector<std::string> values;
};

struct AlternativeInfo {
    std::string name;
    const TypeInfo* type = nullptr;
};

struct VariantInfo {
    std::string name;
    std::string discriminator;
    std::vector<AlternativeInfo> alternatives;
};

using ObjectInfoMap = std::unordered_map<TypeId, ObjectInfo>;
using VariantInfoMap = std::unordered_map<TypeId, VariantInfo>;
using EnumInfoMap = std::unordered_map<TypeId, EnumInfo>;

}  // namespace aison

namespace aison::detail {

template<typename T>
TypeId getTypeId()
{
    static int id = 0x71931d;
    return &id;
}

struct PathSegment {
    enum class Kind { Key, Index } kind = {};
    union Data {
        const char* key;
        std::size_t index;

        constexpr Data()
            : key(nullptr)
        {}
        constexpr Data(const char* k)
            : key(k)
        {}
        constexpr Data(std::size_t i)
            : index(i)
        {}
    } data;

    static PathSegment makeKey(const char* k)
    {
        PathSegment s;
        s.kind = Kind::Key;
        s.data = Data{k};
        return s;
    }

    static PathSegment makeIndex(std::size_t i)
    {
        PathSegment s;
        s.kind = Kind::Index;
        s.data = Data{i};
        return s;
    }
};

class Context
{
public:
    std::string buildPath() const
    {
        std::string result = "$";
        result.reserve(64);
        for (const auto& seg : pathStack_) {
            if (seg.kind == PathSegment::Kind::Key) {
                result.push_back('.');
                result += seg.data.key;
            } else {
                result.push_back('[');
                result += std::to_string(seg.data.index);
                result.push_back(']');
            }
        }
        return result;
    }

    void addError(const std::string& msg) { errors_.push_back(Error{buildPath(), msg}); }

    size_t errorCount() const { return errors_.size(); }
    std::vector<Error> takeErrors() { return std::move(errors_); }
    const std::vector<Error>& errors() const { return errors_; }

protected:
    friend struct PathGuard;
    std::vector<PathSegment> pathStack_;
    std::vector<Error> errors_;
};

struct PathGuard {
    Context* ctx = nullptr;

    PathGuard(Context& context, const std::string& key)
        : PathGuard(context, key.c_str())
    {}

    PathGuard(Context& context, const char* key)
        : ctx(&context)
    {
        ctx->pathStack_.push_back(PathSegment::makeKey(key));
    }

    PathGuard(Context& context, std::size_t index)
        : ctx(&context)
    {
        ctx->pathStack_.push_back(PathSegment::makeIndex(index));
    }

    PathGuard(const PathGuard&) = delete;
    PathGuard& operator=(const PathGuard&) = delete;

    PathGuard(PathGuard&& other) noexcept
        : ctx(other.ctx)
    {
        other.ctx = nullptr;
    }
    PathGuard& operator=(PathGuard&&) = delete;

    ~PathGuard()
    {
        if (ctx) {
            ctx->pathStack_.pop_back();
        }
    }
};

// Traits ///////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct IsOptional : std::false_type {};

template<typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template<typename T>
struct IsVector : std::false_type {};

template<typename T, typename A>
struct IsVector<std::vector<T, A>> : std::true_type {};

template<typename T>
struct IsVariant : std::false_type {};

template<typename... Ts>
struct IsVariant<std::variant<Ts...>> : std::true_type {};

template<typename Schema, typename T, typename>
struct HasEnumTag : std::false_type {};

template<typename Schema, typename T>
struct HasEnumTag<Schema, T, std::void_t<typename Schema::template Enum<T>::EnumTag>>
    : std::true_type {};

template<typename Schema, typename T, typename>
struct HasObjectTag : std::false_type {};

template<typename Schema, typename T>
struct HasObjectTag<Schema, T, std::void_t<typename Schema::template Object<T>::ObjectTag>>
    : std::true_type {};

template<typename Schema, typename T, typename>
struct HasVariantTag : std::false_type {};

template<typename Schema, typename T>
struct HasVariantTag<Schema, T, std::void_t<typename Schema::template Variant<T>::VariantTag>>
    : std::true_type {};

template<typename Schema, typename T, typename>
struct HasCustomTag : std::false_type {};

template<typename Schema, typename T>
struct HasCustomTag<Schema, T, std::void_t<typename Schema::template Custom<T>::CustomTag>>
    : std::true_type {};

template<typename Schema, typename CustomSpec, typename T, typename = void>
struct HasCustomEncode : std::false_type {};

template<typename Schema, typename CustomSpec, typename T>
struct HasCustomEncode<
    Schema,
    CustomSpec,
    T,
    std::void_t<decltype(std::declval<CustomSpec>().encode(
        std::declval<const T&>(),
        std::declval<Json::Value&>(),
        std::declval<EncodeContext<Schema>&>()))>> : std::true_type {};

template<typename Schema, typename CustomSpec, typename T, typename = void>
struct HasCustomDecode : std::false_type {};

template<typename Schema, typename CustomSpec, typename T>
struct HasCustomDecode<
    Schema,
    CustomSpec,
    T,
    std::void_t<decltype(std::declval<CustomSpec>().decode(
        std::declval<const Json::Value&>(),
        std::declval<T&>(),
        std::declval<DecodeContext<Schema>&>()))>> : std::true_type {};

template<typename...>
struct DependentFalse : std::false_type {};

template<typename Schema, typename Owner, typename T>
void ensureTypeRegistration();

template<typename Schema, typename T>
TypeInfo& makeTypeInfo();

template<typename Schema, typename Variant>
TypeInfo& makeVariantTypeInfo();

template<typename Schema, typename T>
void setTypeName(const std::string& name);

// TypeInfo ////////////////////////////////////////////////////////////////////

template<typename Schema, typename Variant, std::size_t... Is>
const TypeInfo* const* makeVariantAlternatives(std::index_sequence<Is...>)
{
    static const TypeInfo* const arr[] = {
        &makeTypeInfo<Schema, std::variant_alternative_t<Is, Variant>>()...};
    return arr;
}

template<typename Schema, typename T>
void registerObjectMapping()
{
    if constexpr (HasObjectTag<Schema, T>::value) {
        const auto& obj = getSchemaObject<typename Schema::template Object<T>>();
        if constexpr (getSchemaEnableIntrospection<Schema>()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(
                    obj.hasName() &&
                    "Schema::Object<T>::name(...) is required when introspection is enabled.");
            }
            if (obj.hasName()) {
                setTypeName<Schema, T>(obj.name());
                getIntrospectionRegistry<Schema>().setObjectName(getTypeId<T>(), obj.name());
            }
        }
    }
}

template<typename Schema, typename Variant, std::size_t Index>
void registerVariantAlternative()
{
    if constexpr (getSchemaEnableIntrospection<Schema>()) {
        using Alt = std::variant_alternative_t<Index, Variant>;
        const auto& objectDef = getSchemaObject<typename Schema::template Object<Alt>>();
        AlternativeInfo info;
        info.type = &makeTypeInfo<Schema, Alt>();
        if (objectDef.hasName()) {
            info.name = objectDef.name();
        }
        getIntrospectionRegistry<Schema>().addVariantAlternative(
            getTypeId<Variant>(), std::move(info));
    }
}

template<typename Schema, typename Variant, std::size_t... Is>
void registerVariantAlternatives(std::index_sequence<Is...>)
{
    (registerVariantAlternative<Schema, Variant, Is>(), ...);
}

template<typename Schema, typename T>
void registerVariantMapping()
{
    if constexpr (HasVariantTag<Schema, T>::value) {
        if constexpr (getSchemaEnableIntrospection<Schema>()) {
            auto& reg = getIntrospectionRegistry<Schema>();
            const auto& def = getSchemaObject<typename Schema::template Variant<T>>();
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(
                    def.hasName() &&
                    "Schema::Variant<V>::name(...) is required when introspection is enabled.");
            }
            if (def.hasName()) {
                reg.setVariantName(getTypeId<T>(), def.name());
                setTypeName<Schema, T>(def.name());
            }
            reg.setVariantDiscriminator(getTypeId<T>(), def.discriminator());
            registerVariantAlternatives<Schema, T>(
                std::make_index_sequence<std::variant_size_v<T>>{});
        }
    }
}

template<typename Schema, typename E>
void registerEnumMapping()
{
    if constexpr (HasEnumTag<Schema, E>::value) {
        const auto& enumDef = getSchemaObject<typename Schema::template Enum<E>>();
        if constexpr (getSchemaEnableIntrospection<Schema>()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(
                    enumDef.hasName() &&
                    "Schema::Enum<E>::name(...) is required when introspection is enabled.");
            }
            if (enumDef.hasName()) {
                getIntrospectionRegistry<Schema>().setEnumName(getTypeId<E>(), enumDef.name());
                setTypeName<Schema, E>(enumDef.name());
            }
        }
    }
}

template<typename Schema, typename T>
void registerCustomMapping()
{
    if constexpr (HasCustomTag<Schema, T>::value) {
        const auto& custom = getSchemaObject<typename Schema::template Custom<T>>();
        if constexpr (getSchemaEnableIntrospection<Schema>()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(
                    custom.hasName() &&
                    "Schema::Custom<T>::name(...) is required when introspection is enabled.");
            }
            if (custom.hasName()) {
                setTypeName<Schema, T>(custom.name());
            }
        }
    }
}

template<typename Schema, typename Owner, typename Variant, std::size_t... Is>
void ensureVariantAlternatives(std::index_sequence<Is...>)
{
    (ensureTypeRegistration<Schema, Owner, std::variant_alternative_t<Is, Variant>>(), ...);
}

template<typename Schema, typename Owner, typename T>
void ensureTypeRegistration()
{
    if constexpr (!getSchemaEnableIntrospection<Schema>()) {
        return;
    } else if constexpr (IsOptional<T>::value) {
        using U = typename T::value_type;
        ensureTypeRegistration<Schema, Owner, U>();
    } else if constexpr (IsVector<T>::value) {
        using U = typename T::value_type;
        ensureTypeRegistration<Schema, Owner, U>();
    } else if constexpr (IsVariant<T>::value) {
        using VT = std::decay_t<T>;
        registerVariantMapping<Schema, VT>();
        ensureVariantAlternatives<Schema, Owner, VT>(
            std::make_index_sequence<std::variant_size_v<VT>>{});
    } else if constexpr (std::is_enum_v<T>) {
        registerEnumMapping<Schema, T>();
    } else if constexpr (HasObjectTag<Schema, T>::value && !std::is_same_v<Owner, T>) {
        registerObjectMapping<Schema, T>();
    } else if constexpr (HasCustomTag<Schema, T>::value) {
        registerCustomMapping<Schema, T>();
    }
}

template<typename Schema, typename T>
TypeInfo& makeTypeInfo()
{
    if constexpr (IsOptional<T>::value) {
        static TypeInfo info =
            TypeInfo::optional<T>(&makeTypeInfo<Schema, typename T::value_type>());
        return info;
    } else if constexpr (IsVector<T>::value) {
        static TypeInfo info = TypeInfo::vector<T>(&makeTypeInfo<Schema, typename T::value_type>());
        return info;
    } else if constexpr (IsVariant<T>::value) {
        validateVariant<Schema, T>();
        return makeVariantTypeInfo<Schema, T>();
    } else if constexpr (std::is_same_v<T, bool>) {
        static TypeInfo info = TypeInfo::scalar<T>(TypeClass::Bool);
        return info;
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        static TypeInfo info = TypeInfo::integral<T>();
        return info;
    } else if constexpr (std::is_floating_point_v<T>) {
        static TypeInfo info = TypeInfo::floating<T>();
        return info;
    } else if constexpr (std::is_same_v<T, std::string>) {
        static TypeInfo info = TypeInfo::scalar<T>(TypeClass::String);
        return info;
    } else if constexpr (std::is_enum_v<T>) {
        static TypeInfo info = TypeInfo::scalar<T>(TypeClass::Enum);
        return info;
    } else if constexpr (HasObjectTag<Schema, T>::value) {
        static TypeInfo info = TypeInfo::scalar<T>(TypeClass::Object);
        return info;
    } else if constexpr (HasCustomTag<Schema, T>::value) {
        static TypeInfo info = TypeInfo::scalar<T>(TypeClass::Custom);
        return info;
    } else {
        static_assert(
            DependentFalse<T>::value,
            "Unsupported type for introspection. "
            "Provide a Schema::Object / Schema::Enum mapping / Schema::Custom.");
        static TypeInfo info = TypeInfo::scalar<T>(TypeClass::Unknown);
        return info;
    }
}

template<typename Schema, typename T>
TypeInfo& makeVariantTypeInfo()
{
    constexpr auto count = std::variant_size_v<T>;
    static const TypeInfo* const* altArray =
        makeVariantAlternatives<Schema, T>(std::make_index_sequence<count>{});
    static TypeInfo info = TypeInfo::variant<T>(altArray, count);
    return info;
}

template<typename Schema, typename T>
void setTypeName(const std::string& name)
{
    auto& info = makeTypeInfo<Schema, T>();  // todo fixme
    info.name = name.empty() ? nullptr : name.c_str();
}

template<typename Schema>
class IntrospectionRegistry
{
public:
    void addObjectField(TypeId typeId, FieldInfo field)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = objects_[typeId];
        entry.fields.push_back(std::move(field));
    }

    void setObjectName(TypeId typeId, std::string name)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = objects_[typeId];
        entry.name = std::move(name);
    }

    void addEnumName(TypeId typeId, std::string_view name)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = enums_[typeId];
        entry.values.push_back(std::string(name));
    }

    void setEnumName(TypeId typeId, std::string name)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = enums_[typeId];
        entry.name = std::move(name);
    }

    void setVariantName(TypeId typeId, std::string name)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        variants_[typeId].name = std::move(name);
    }

    void setVariantDiscriminator(TypeId typeId, std::string key)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        variants_[typeId].discriminator = std::move(key);
    }

    void addVariantAlternative(TypeId typeId, AlternativeInfo alt)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = variants_[typeId];
        for (const auto& existing : entry.alternatives) {
            if (existing.type == alt.type) {
                return;
            }
        }
        entry.alternatives.push_back(std::move(alt));
    }

    const std::unordered_map<TypeId, ObjectInfo>& objects() const { return objects_; }
    const std::unordered_map<TypeId, EnumInfo>& enums() const { return enums_; }
    const std::unordered_map<TypeId, VariantInfo>& variants() const { return variants_; }

private:
    std::unordered_map<TypeId, ObjectInfo> objects_;
    std::unordered_map<TypeId, EnumInfo> enums_;
    std::unordered_map<TypeId, VariantInfo> variants_;
};

template<typename Schema>
IntrospectionRegistry<Schema>& getIntrospectionRegistry()
{
    static IntrospectionRegistry<Schema> reg;
    return reg;
}

// Enum impl + validation //////////////////////////////////////////////////////////////////

struct EnumBase {};

template<typename Schema, typename E>
class EnumImpl : public EnumBase
{
public:
    using Entry = std::pair<E, std::string>;
    using EntryVec = std::vector<Entry>;

    void name(std::string_view value)
    {
        if (value.empty()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Enum name cannot be empty.");
            }
            return;
        }
        if (hasName_) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Enum name already set.");
            }
            return;
        }
        hasName_ = true;
        name_ = std::string(value);
        if constexpr (getSchemaEnableIntrospection<Schema>()) {
            setTypeName<Schema, E>(name_);
            getIntrospectionRegistry<Schema>().setEnumName(getTypeId<E>(), name_);
        }
    }

    void add(E value, std::string_view name)
    {
        for (const auto& entry : entries_) {
            // Disallow duplicate value or duplicate name
            if (entry.first == value || entry.second == name) {
                if constexpr (getSchemaEnableAssert<Schema>()) {
                    assert(false && "Duplicate enum mapping in Schema::Enum.");
                }
                return;
            }
        }
        entries_.emplace_back(value, std::string(name));
        if constexpr (getSchemaEnableIntrospection<Schema>()) {
            getIntrospectionRegistry<Schema>().addEnumName(getTypeId<E>(), name);
        }
    }

    const EntryVec& entries() const { return entries_; }
    const std::string& name() const { return name_; }
    bool hasName() const { return hasName_; }

private:
    std::vector<Entry> entries_;
    std::string name_;
    bool hasName_ = false;
};

template<typename Schema, typename Variant>
class VariantImpl
{
public:
    VariantImpl()
    {
        static_assert(
            IsVariant<Variant>::value,
            "Schema::Variant<T> must map a std::variant of object-mapped alternatives.");
    }

    void name(std::string_view value)
    {
        if (value.empty()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Variant name cannot be empty.");
            }
            return;
        }
        if (hasName_) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Variant name already set.");
            }
            return;
        }
        hasName_ = true;
        name_ = std::string(value);
        if constexpr (getSchemaEnableIntrospection<Schema>()) {
            setTypeName<Schema, Variant>(name_);
        }
    }

    void discriminator(std::string_view key)
    {
        if (key.empty()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Discriminator key cannot be empty.");
            }
            return;
        }
        if (hasDiscriminator_) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Variant discriminator already set.");
            }
            return;
        }
        hasDiscriminator_ = true;
        discriminator_ = std::string(key);
    }

    const std::string& name() const { return name_; }
    const std::string& discriminator() const { return discriminator_; }
    bool hasName() const { return hasName_; }
    bool hasDiscriminator() const { return !discriminator_.empty(); }

private:
    std::string name_;
    std::string discriminator_;
    bool hasName_ = false;
    bool hasDiscriminator_ = false;
};

template<typename Schema, typename T>
constexpr void validateEnumType()
{
    static_assert(
        HasEnumTag<Schema, T>::value,
        "No schema enum mapping for this type. "
        "Define `template<> struct Schema::Enum<T> : aison::Enum<Schema, T>` and "
        "list all enum values.");

    if constexpr (HasEnumTag<Schema, T>::value) {
        using EnumDef = typename Schema::template Enum<T>;
        static_assert(
            std::is_base_of_v<EnumBase, EnumDef>,
            "Schema::Enum<T> must inherit from aison::Enum<Schema, T>.");
    }
}

// Variant validation //////////////////////////////////////////////////////////////////////

template<typename Schema, typename Variant, typename Enable = void>
struct VariantValidator {
    static constexpr void validate() {}
};

template<typename Schema, typename Context, typename Variant, typename Enable = void>
struct VariantKeyValidator {
    static bool validate(Context&) { return true; }
};

template<typename Schema, typename T>
struct VariantAltCheck {
    static constexpr void check()
    {
        static_assert(
            HasObjectTag<Schema, T>::value,
            "std::variant alternative is not mapped as an object. "
            "Define `template<> struct Schema::Object<T> : aison::Object<Schema, T>` "
            "for each variant alternative.");
    }
};

template<typename Schema, typename... Ts>
struct VariantValidator<Schema, std::variant<Ts...>, void> {
    static constexpr void validate()
    {
        static_assert(
            HasVariantTag<Schema, std::variant<Ts...>>::value,
            "No schema variant mapping for this type. "
            "Define `template<> struct Schema::Variant<V> : aison::Variant<Schema, V>`.");
        using VariantSpec = typename Schema::template Variant<std::variant<Ts...>>;
        static_assert(
            std::is_base_of_v<VariantImpl<Schema, std::variant<Ts...>>, VariantSpec>,
            "Schema::Variant<V> must inherit from aison::Variant<Schema, V>.");
        static_assert(sizeof...(Ts) > 0, "std::variant must have at least one alternative.");
        // Each alternative must have an object mapping.
        (VariantAltCheck<Schema, Ts>::check(), ...);
    }
};

template<typename Schema, typename Context, typename... Ts>
struct VariantKeyValidator<Schema, Context, std::variant<Ts...>, void> {
    static bool validate(Context& ctx)
    {
        using VariantType = std::variant<Ts...>;
        const auto& variantDef = getSchemaObject<typename Schema::template Variant<VariantType>>();
        // TODO: this is not needed here

        if (variantDef.discriminator().empty()) {
            ctx.addError("Discriminator key not set for variant.");
            return false;
        }

        bool missingTag = false;
        ((missingTag =
              missingTag || !getSchemaObject<typename Schema::template Object<Ts>>().hasName()),
         ...);

        if (missingTag) {
            ctx.addError("Variant alternative missing name().");
            return false;
        }

        return true;
    }
};

template<typename Schema, typename Variant>
constexpr void validateVariant()
{
    if constexpr (IsVariant<Variant>::value) {
        VariantValidator<Schema, Variant>::validate();
    }
}

template<typename T>
T& getSchemaObject()
{
    static T instance{};
    return instance;
}

// EnableAssert

template<typename Schema, typename>
struct HasSchemaEnableAssert : std::false_type {};

template<typename Schema>
struct HasSchemaEnableAssert<Schema, std::void_t<decltype(Schema::enableAssert)>>
    : std::true_type {};

template<typename Schema, typename>
struct SchemaEnableAssert {
    constexpr static bool get() { return SchemaDefaults::enableAssert; }
};

template<typename Schema>
struct SchemaEnableAssert<Schema, std::enable_if_t<HasSchemaEnableAssert<Schema>::value>> {
    constexpr static bool get()
    {
        using Type = std::decay_t<decltype(Schema::enableAssert)>;
        static_assert(std::is_same_v<Type, bool>, "Schema::enableAssert must be bool.");
        return Schema::enableAssert;
    }
};

template<typename Schema>
constexpr bool getSchemaEnableAssert()
{
    return SchemaEnableAssert<Schema>::get();
}

// Optional strictness

template<typename Schema, typename>
struct HasSchemaStrictOptional : std::false_type {};

template<typename Schema>
struct HasSchemaStrictOptional<Schema, std::void_t<decltype(Schema::strictOptional)>>
    : std::true_type {};

template<typename Schema, typename>
struct SchemaStrictOptional {
    constexpr static bool get() { return SchemaDefaults::strictOptional; }
};

template<typename Schema>
struct SchemaStrictOptional<Schema, std::enable_if_t<HasSchemaStrictOptional<Schema>::value>> {
    constexpr static bool get()
    {
        using Type = std::decay_t<decltype(Schema::strictOptional)>;
        static_assert(std::is_same_v<Type, bool>, "Schema::strictOptional must be bool.");
        return Schema::strictOptional;
    }
};

template<typename Schema>
constexpr bool getSchemaStrictOptional()
{
    return SchemaStrictOptional<Schema>::get();
}

// Introspection flag

template<typename Schema, typename>
struct HasSchemaEnableIntrospection : std::false_type {};

template<typename Schema>
struct HasSchemaEnableIntrospection<Schema, std::void_t<decltype(Schema::enableIntrospection)>>
    : std::true_type {};

template<typename Schema, typename>
struct SchemaEnableIntrospection {
    constexpr static bool get() { return SchemaDefaults::enableIntrospection; }
};

template<typename Schema>
struct SchemaEnableIntrospection<
    Schema,
    std::enable_if_t<HasSchemaEnableIntrospection<Schema>::value>> {
    constexpr static bool get()
    {
        using Type = std::decay_t<decltype(Schema::enableIntrospection)>;
        static_assert(std::is_same_v<Type, bool>, "Schema::enableIntrospection must be bool.");
        return Schema::enableIntrospection;
    }
};

template<typename Schema>
constexpr bool getSchemaEnableIntrospection()
{
    return SchemaEnableIntrospection<Schema>::get();
}

// EncoderImpl / DecoderImpl ///////////////////////////////////////////////////////////////

template<typename Schema>
class EncodeContext : public Context
{
public:
    using Config = typename Schema::ConfigType;

    EncodeContext(const Config& cfg)
        : config_(cfg)
    {
        using Facet = typename Schema::FacetType;
        static_assert(
            hasEncodeFacet<Schema>(),
            "EncoderImpl<Schema> cannot be used with a DecodeOnly schema facet.");
    }

    template<typename T>
    void encode(const T& value, Json::Value& dst)
    {
        encodeValue<Schema, T>(value, dst, *this);
    }

    const Config& config() const { return config_; }

private:
    const Config& config_;
};

template<typename Schema>
class DecodeContext : public Context
{
public:
    using Config = typename Schema::ConfigType;

    DecodeContext(const Config& cfg)
        : config_(cfg)
    {
        using Facet = typename Schema::FacetType;
        static_assert(
            hasDecodeFacet<Schema>(),
            "DecoderImpl<Schema> cannot be used with an EncodeOnly schema facet.");
    }

    template<typename T>
    void decode(const Json::Value& src, T& value)
    {
        decodeValue<Schema, T>(src, value, *this);
    }

    const Config& config() const { return config_; }

private:
    const Config& config_;
};

// Custom encoder/decoder dispatch /////////////////////////////////////////////////////////

template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, EncodeContext<Schema>& ctx)
{
    if constexpr (HasCustomTag<Schema, T>::value) {
        using CustomSpec = typename Schema::template Custom<T>;
        static_assert(
            std::is_base_of_v<Custom<Schema, T>, CustomSpec>,
            "Schema::Custom<T> must inherit aison::Custom<Schema, T>.");
        static_assert(
            HasCustomEncode<Schema, CustomSpec, T>::value,
            "Schema::Custom<T> must implement encode(const T&, Json::Value&, EncodeContext&).");
        auto& custom = getSchemaObject<CustomSpec>();
        custom.encode(value, dst, ctx);
    } else {
        encodeDefault<Schema, T>(value, dst, ctx);
    }
}

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, DecodeContext<Schema>& ctx)
{
    if constexpr (HasCustomTag<Schema, T>::value) {
        using CustomSpec = typename Schema::template Custom<T>;
        static_assert(
            std::is_base_of_v<Custom<Schema, T>, CustomSpec>,
            "Schema::Custom<T> must inherit aison::Custom<Schema, T>.");
        static_assert(
            HasCustomDecode<Schema, CustomSpec, T>::value,
            "Schema::Custom<T> must implement decode(const Json::Value&, T&, DecodeContext&).");
        auto& custom = getSchemaObject<CustomSpec>();
        custom.decode(src, value, ctx);
    } else {
        decodeDefault<Schema, T>(src, value, ctx);
    }
}

// Encode defaults ///////////////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
void encodeDefault(const T& value, Json::Value& dst, EncodeContext<Schema>& ctx)
{
    if constexpr (std::is_same_v<T, bool>) {
        dst = value;
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        dst = static_cast<std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>>(value);
    } else if constexpr (std::is_same_v<T, float>) {
        if (std::isnan(value)) {
            ctx.addError("NaN is not allowed here.");
            return;
        }
        dst = value;
    } else if constexpr (std::is_same_v<T, double>) {
        if (std::isnan(value)) {
            ctx.addError("NaN is not allowed here.");
            return;
        }
        dst = value;
    } else if constexpr (std::is_same_v<T, std::string>) {
        dst = value;
    } else if constexpr (std::is_enum_v<T>) {
        validateEnumType<Schema, T>();

        using EnumSpec = typename Schema::template Enum<T>;
        const auto& entries = getSchemaObject<EnumSpec>().entries();
        for (const auto& entry : entries) {
            if (entry.first == value) {
                dst = Json::Value(std::string(entry.second));
                return;
            }
        }
        using U = typename std::underlying_type<T>::type;
        ctx.addError(
            "Unhandled enum value during encode (underlying = " + std::to_string(U(value)) + ").");
    } else if constexpr (IsOptional<T>::value) {
        using U = typename T::value_type;
        if (!value) {
            dst = Json::nullValue;
        } else {
            encodeValue<Schema, U>(*value, dst, ctx);
        }
    } else if constexpr (IsVariant<T>::value) {
        // Discriminated polymorphic encoding for std::variant.
        validateVariant<Schema, T>();
        if (!VariantKeyValidator<Schema, EncodeContext<Schema>, T>::validate(ctx)) {
            return;
        }
        const auto& variantDef = getSchemaObject<typename Schema::template Variant<T>>();
        dst = Json::objectValue;
        std::visit(
            [&](const auto& alt) {
                using Alt = std::decay_t<decltype(alt)>;

                const auto& objectDef = getSchemaObject<typename Schema::template Object<Alt>>();
                if (!objectDef.hasVariantTag()) {
                    PathGuard guard(ctx, variantDef.discriminator());
                    ctx.addError("Variant alternative missing name().");
                    return;
                }

                // Encode discriminator using a string payload.
                Json::Value tagJson;
                const std::string tagValue(objectDef.variantTag());
                encodeDefault<Schema, std::string>(tagValue, tagJson, ctx);

                // Encode variant-specific fields into the same object.
                objectDef.encodeFields(alt, dst, ctx);

                // Write discriminator field.
                dst[variantDef.discriminator()] = std::move(tagJson);
            },
            value);
    } else if constexpr (IsVector<T>::value) {
        using U = typename T::value_type;
        dst = Json::arrayValue;
        std::size_t index = 0;
        for (const auto& elem : value) {
            PathGuard guard(ctx, index++);
            Json::Value v;
            encodeValue<Schema, U>(elem, v, ctx);
            dst.append(v);
        }
    } else if constexpr (std::is_class_v<T>) {
        static_assert(
            HasObjectTag<Schema, T>::value,
            "Type is not mapped as an object. Either define "
            "`template<> struct Schema::Object<T> : aison::Object<Schema, T>` and call "
            "add(...) for its fields, or provide a custom mapping via Schema::Custom<T>`.");
        const auto& objectDef = getSchemaObject<typename Schema::template Object<T>>();
        objectDef.encodeFields(value, dst, ctx);
    } else if constexpr (std::is_pointer_v<T>) {
        static_assert(false && std::is_pointer_v<T>, "Pointers are not supported.");
    } else {
        static_assert(
            DependentFalse<T>::value,
            "Unsupported type. Define Schema::Custom<T> to provide an encoder.");
    }
}

// Decode defaults //////////////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
void decodeDefault(const Json::Value& src, T& value, DecodeContext<Schema>& ctx)
{
    if constexpr (std::is_same_v<T, bool>) {
        if (!src.isBool()) {
            ctx.addError("Expected bool.");
            return;
        }
        value = src.asBool();
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        if (!src.isIntegral()) {
            ctx.addError("Expected integral value.");
            return;
        }
        if constexpr (std::is_signed_v<T>) {
            auto v = src.asInt64();
            if (v < std::numeric_limits<T>::min() || v > std::numeric_limits<T>::max()) {
                ctx.addError("Integer value out of range.");
                return;
            }
            value = static_cast<T>(v);
        } else {
            auto v = src.asUInt64();
            if (v > std::numeric_limits<T>::max()) {
                ctx.addError("Unsigned integer value out of range.");
                return;
            }
            value = static_cast<T>(v);
        }
    } else if constexpr (std::is_same_v<T, float>) {
        if (!src.isDouble() && !src.isInt()) {
            ctx.addError("Expected float.");
            return;
        }
        value = static_cast<float>(src.asDouble());
    } else if constexpr (std::is_same_v<T, double>) {
        if (!src.isDouble() && !src.isInt()) {
            ctx.addError("Expected double.");
            return;
        }
        value = src.asDouble();
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!src.isString()) {
            ctx.addError("Expected string.");
            return;
        }
        value = src.asString();
    } else if constexpr (std::is_enum_v<T>) {
        validateEnumType<Schema, T>();

        if (!src.isString()) {
            ctx.addError("Expected string for enum.");
            return;
        }
        const std::string s = src.asString();
        using EnumSpec = typename Schema::template Enum<T>;
        const auto& entries = getSchemaObject<EnumSpec>().entries();
        for (const auto& entry : entries) {
            if (s == entry.second) {
                value = entry.first;
                return;
            }
        }
        ctx.addError("Unknown enum value '" + s + "'.");
    } else if constexpr (IsOptional<T>::value) {
        using U = typename T::value_type;
        if (src.isNull()) {
            value.reset();
        } else {
            U tmp{};
            decodeValue<Schema, U>(src, tmp, ctx);
            value = std::move(tmp);
        }
    } else if constexpr (IsVariant<T>::value) {
        // Discriminated polymorphic decoding for std::variant.
        validateVariant<Schema, T>();
        VariantDecoder<Schema, T>::decode(src, value, ctx);
    } else if constexpr (IsVector<T>::value) {
        using U = typename T::value_type;
        value.clear();
        if (!src.isArray()) {
            ctx.addError("Expected array.");
            return;
        }
        for (Json::ArrayIndex i = 0; i < src.size(); ++i) {
            PathGuard guard(ctx, i);
            U elem{};
            decodeValue<Schema, U>(src[i], elem, ctx);
            value.push_back(std::move(elem));
        }
    } else if constexpr (std::is_class_v<T>) {
        static_assert(
            HasObjectTag<Schema, T>::value,
            "Type is not mapped as an object. Either define "
            "`template<> struct Schema::Object<T> : aison::Object<Schema, T>` and call "
            "add(...) for its fields, or provide a custom mapping via Schema::Custom<T>`.");
        if (!src.isObject()) {
            ctx.addError("Expected object.");
            return;
        }

        const auto& objectDef = getSchemaObject<typename Schema::template Object<T>>();
        objectDef.decodeFields(src, value, ctx);
    } else if constexpr (std::is_pointer_v<T>) {
        static_assert(false && std::is_pointer_v<T>, "Pointers are not supported.");
    } else {
        static_assert(
            DependentFalse<T>::value,
            "Unsupported type. Define Schema::Custom<T> to provide a decoder.");
    }
}

// Variant decoding /////////////////////////////////////////////////////////////////////////

template<typename Schema, typename... Ts>
struct VariantDecoder<Schema, std::variant<Ts...>> {
    using VariantType = std::variant<Ts...>;

    static void decode(const Json::Value& src, VariantType& value, DecodeContext<Schema>& ctx)
    {
        // Ensure src is an object
        if (!src.isObject()) {
            ctx.addError("Expected object for discriminated variant.");
            return;
        }

        const auto& variantDef = getSchemaObject<typename Schema::template Variant<VariantType>>();
        const auto& fieldName = variantDef.discriminator();
        if (!VariantKeyValidator<Schema, DecodeContext<Schema>, std::variant<Ts...>>::validate(ctx))
        {
            return;
        }

        std::string tagValue;
        {
            PathGuard discGuard(ctx, fieldName);
            if (!src.isMember(fieldName)) {
                ctx.addError("Missing discriminator field.");
                return;
            }

            const Json::Value& tagNode = src[fieldName];
            if (!tagNode.isString()) {
                ctx.addError("Expected string.");
                return;
            }
            tagValue = tagNode.asString();
        }

        bool matched = false;

        // Try each alternative in turn
        (tryAlternative<Ts>(src, tagValue, fieldName, value, ctx, matched), ...);

        if (!matched) {
            PathGuard discGuard(ctx, fieldName);
            ctx.addError("Unknown discriminator value for variant.");
        }
    }

private:
    template<typename Alt>
    static void tryAlternative(
        const Json::Value& src,
        const std::string& tagValue,
        const std::string& discriminatorKey,
        VariantType& value,
        DecodeContext<Schema>& ctx,
        bool& matched)
    {
        using ObjectSpec = typename Schema::template Object<Alt>;
        const auto& objectDef = getSchemaObject<ObjectSpec>();
        if (!objectDef.hasVariantTag()) {
            PathGuard guard(ctx, discriminatorKey);
            ctx.addError("Variant alternative missing name().");
            return;
        }
        if (matched || tagValue != objectDef.variantTag()) {
            return;
        }

        matched = true;

        Alt alt{};
        objectDef.decodeFields(src, alt, ctx);
        value = std::move(alt);
    }
};

// Encode/decode thunks /////////////////////////////////////////////////////////////

template<typename Owner, typename T>
struct FieldAccessor {
    T Owner::* member;
};

template<typename Schema, typename Owner, typename T>
void encodeFieldThunk(
    const Owner& owner, Json::Value& dst, EncodeContext<Schema>& ctx, FieldAccessorPtr ptr)
{
    using Accessor = FieldAccessor<Owner, T>;
    auto* accessor = static_cast<const Accessor*>(ptr);
    auto& member = accessor->member;
    const T& ref = owner.*member;
    encodeValue<Schema, T>(ref, dst, ctx);
}

template<typename Schema, typename Owner, typename T>
void decodeFieldThunk(
    const Json::Value& src, Owner& owner, DecodeContext<Schema>& ctx, FieldAccessorPtr ptr)
{
    using Accessor = FieldAccessor<Owner, T>;
    auto* accessor = static_cast<const Accessor*>(ptr);
    auto& member = accessor->member;
    T& ref = owner.*member;
    decodeValue<Schema, T>(src, ref, ctx);
}

// Object implementation /////////////////////////////////////////////////////////

template<typename Owner, typename T>
FieldAccessorStorage makeFieldAccessor(T Owner::* member)
{
    using Accessor = FieldAccessor<Owner, T>;
    auto* ctx = new Accessor{member};
    auto deleter = +[](void* p) { delete static_cast<Accessor*>(p); };
    return {ctx, deleter};
}

template<typename Owner, typename T>
FieldAccessorId getFieldAccessorId()
{
    static int id = 0xf1e1d1d;
    return &id;
}

template<typename Schema>
constexpr bool hasEncodeFacet()
{
    using Facet = typename Schema::FacetType;
    return std::is_same_v<Facet, EncodeDecode> || std::is_same_v<Facet, EncodeOnly>;
}

template<typename Schema>
constexpr bool hasDecodeFacet()
{
    using Facet = typename Schema::FacetType;
    return std::is_same_v<Facet, EncodeDecode> || std::is_same_v<Facet, DecodeOnly>;
}

template<typename Schema, typename Owner>
class ObjectImpl
{
private:
    struct FieldDef {
        using EncodeFn =
            void (*)(const Owner&, Json::Value&, EncodeContext<Schema>&, FieldAccessorPtr ptr);
        using DecodeFn =
            void (*)(const Json::Value&, Owner&, DecodeContext<Schema>&, FieldAccessorPtr ptr);

        FieldDef(FieldAccessorStorage&& accessor)
            : accessor(std::move(accessor))
        {}

        FieldAccessorStorage accessor;
        FieldAccessorId accessorId = nullptr;  // Note: this is needed to avoid UB casts
        EncodeFn encode = nullptr;
        DecodeFn decode = nullptr;

        std::string name;
        bool isOptional = false;
    };

public:
    void name(std::string_view value)
    {
        if (value.empty()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Object name cannot be empty.");
            }
            return;
        }
        if (hasName_) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Object name already set.");
            }
            return;
        }
        hasName_ = true;
        name_ = std::string(value);
        if constexpr (getSchemaEnableIntrospection<Schema>()) {
            setTypeName<Schema, Owner>(name_);
            getIntrospectionRegistry<Schema>().setObjectName(detail::getTypeId<Owner>(), name_);
        }
    }

    template<typename T>
    void add(T Owner::* member, std::string_view name)
    {
        // Check if member or name is already mapped
        auto accessorId = getFieldAccessorId<Owner, T>();
        for (const auto& field : fields_) {
            using Ctx = FieldAccessor<Owner, T>;
            if (field.accessorId == accessorId &&
                reinterpret_cast<const Ctx*>(field.accessor.get())->member == member)
            {
                if constexpr (getSchemaEnableAssert<Schema>()) {
                    assert(false && "Same member is mapped multiple times in Schema::Object.");
                }
                return;
            }

            if (field.name == name) {
                if constexpr (getSchemaEnableAssert<Schema>()) {
                    assert(false && "Duplicate field name in Schema::Object.");
                }
                return;
            }
        }

        auto& field = fields_.emplace_back(makeFieldAccessor(member));

        field.name = std::string(name);
        field.accessorId = accessorId;
        field.isOptional = IsOptional<T>::value;

        if constexpr (hasEncodeFacet<Schema>()) {
            field.encode = &encodeFieldThunk<Schema, Owner, T>;
        }

        if constexpr (hasDecodeFacet<Schema>()) {
            field.decode = &decodeFieldThunk<Schema, Owner, T>;
        }

        if constexpr (getSchemaEnableIntrospection<Schema>()) {
            FieldInfo fi;
            fi.name = std::string(name);
            fi.type = &makeTypeInfo<Schema, T>();
            getIntrospectionRegistry<Schema>().addObjectField(getTypeId<Owner>(), std::move(fi));
            ensureTypeRegistration<Schema, Owner, T>();
        }
    }

    template<typename S = Schema, typename = std::enable_if_t<hasEncodeFacet<S>()>>
    void encodeFields(const Owner& src, Json::Value& dst, EncodeContext<Schema>& ctx) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            PathGuard guard(ctx, field.name);
            Json::Value node;
            field.encode(src, node, ctx, field.accessor.get());
            if (!getSchemaStrictOptional<Schema>() && field.isOptional && node.isNull()) {
                continue;
            }
            dst[field.name] = std::move(node);
        }
    }

    template<typename S = Schema, typename = std::enable_if_t<hasDecodeFacet<S>()>>
    void decodeFields(const Json::Value& src, Owner& dst, DecodeContext<Schema>& ctx) const
    {
        for (const auto& field : fields_) {
            const auto& key = field.name;
            if (!src.isMember(key)) {
                if (!getSchemaStrictOptional<Schema>() && field.isOptional) {
                    PathGuard guard(ctx, key);
                    field.decode(Json::nullValue, dst, ctx, field.accessor.get());
                    continue;
                }
                ctx.addError(std::string("Missing required field '") + key + "'.");
                continue;
            }
            const Json::Value& node = src[key];
            PathGuard guard(ctx, key);
            field.decode(node, dst, ctx, field.accessor.get());
        }
    }

    const std::string& name() const { return name_; }
    bool hasName() const { return hasName_; }
    bool hasVariantTag() const { return hasName_; }
    const std::string& variantTag() const { return name_; }
    const std::vector<FieldDef>& fields() const { return fields_; }

private:
    std::vector<FieldDef> fields_;
    std::string name_;
    bool hasName_ = false;
};

template<typename Schema>
class IntrospectionImpl
{
public:
    IntrospectionImpl()
    {
        static_assert(
            detail::getSchemaEnableIntrospection<Schema>(),
            "Introspection is disabled for this schema. Set `static constexpr bool "
            "enableIntrospection = true;` in your Schema to use Introspection.");
    }

    template<typename T>
    void add()
    {
        using U = std::decay_t<T>;
        if constexpr (std::is_enum_v<U>) {
            validateEnumType<Schema, U>();
            registerEnumMapping<Schema, U>();
            collectEnum(getTypeId<U>());
        } else if constexpr (IsOptional<U>::value) {
            add<typename U::value_type>();
        } else if constexpr (IsVector<U>::value) {
            add<typename U::value_type>();
        } else if constexpr (IsVariant<U>::value) {
            registerVariantMapping<Schema, U>();
            ensureVariantAlternatives<Schema, U, U>(
                std::make_index_sequence<std::variant_size_v<U>>{});
            addVariantAlternatives<U>(std::make_index_sequence<std::variant_size_v<U>>{});
            collectVariant(getTypeId<U>());
        } else if constexpr (std::is_class_v<U>) {
            static_assert(
                HasObjectTag<Schema, U>::value || HasCustomTag<Schema, U>::value,
                "Type is not part of the schema. Define Schema::Object<T> or Schema::Custom<T>.");

            if constexpr (HasObjectTag<Schema, U>::value) {
                registerObjectMapping<Schema, U>();
                collectObject(getTypeId<U>());
            } else {
                registerCustomMapping<Schema, U>();
            }
        } else {
            static_assert(!std::is_class_v<U>, "Unsupported type.");
        }
    }

    void collectEnum(TypeId typeId)
    {
        if (enums_.count(typeId)) {
            return;
        }
        const auto& reg = getIntrospectionRegistry<Schema>();
        auto it = reg.enums().find(typeId);
        if (it == reg.enums().end()) {
            return;
        }
        if (it->second.name.empty()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Enum must have name() when introspection is enabled.");
            }
            return;
        }
        enums_.emplace(it->first, it->second);
    }

    void collectObject(TypeId typeId)
    {
        if (objects_.count(typeId)) {
            return;
        }
        const auto& reg = getIntrospectionRegistry<Schema>();
        auto it = reg.objects().find(typeId);
        if (it == reg.objects().end()) {
            return;
        }

        if (it->second.name.empty()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Object must have name() when introspection is enabled.");
            }
            return;
        }

        objects_.emplace(it->first, it->second);
        const auto& objInfo = objects_.find(typeId)->second;

        for (const auto& field : objInfo.fields) {
            traverseType(field.type);
        }
    }

    void collectVariant(TypeId typeId)
    {
        if (variants_.count(typeId)) {
            return;
        }
        const auto& reg = getIntrospectionRegistry<Schema>();
        auto it = reg.variants().find(typeId);
        if (it == reg.variants().end()) {
            return;
        }
        if (it->second.name.empty()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Variant must have name() when introspection is enabled.");
            }
            return;
        }
        variants_.emplace(it->first, it->second);
        const auto& varInfo = variants_.find(typeId)->second;
        for (const auto& alt : varInfo.alternatives) {
            traverseType(alt.type);
        }
    }

    void traverseType(const TypeInfo* info)
    {
        if (!info) return;
        switch (info->cls) {
            case TypeClass::Object:
                collectObject(info->typeId);
                break;
            case TypeClass::Enum:
                collectEnum(info->typeId);
                break;
            case TypeClass::Optional:
                traverseType(info->data.optional.type);
                break;
            case TypeClass::Vector:
                traverseType(info->data.vector.type);
                break;
            case TypeClass::Variant:
                collectVariant(info->typeId);
                for (std::size_t i = 0; i < info->data.variant.count; ++i) {
                    traverseType(info->data.variant.types ? info->data.variant.types[i] : nullptr);
                }
                break;
            default:
                break;
        }
    }

    template<typename Variant, std::size_t... Is>
    void addVariantAlternatives(std::index_sequence<Is...>)
    {
        (add<std::variant_alternative_t<Is, Variant>>(), ...);
    }

    const ObjectInfoMap& objects() const { return objects_; }
    const VariantInfoMap& variants() const { return variants_; }
    const EnumInfoMap& enums() const { return enums_; }

private:
    ObjectInfoMap objects_;
    VariantInfoMap variants_;
    EnumInfoMap enums_;
};

}  // namespace aison::detail

namespace aison {

// Object / Enum bases ///////////////////////////////////////////////////////////////////

template<typename Schema, typename Owner>
struct Object : detail::ObjectImpl<Schema, Owner> {
    using ObjectTag = void;
    using Impl = detail::ObjectImpl<Schema, Owner>;

    using Impl::add;
    using Impl::name;
};

template<typename Schema, typename E>
struct Enum : detail::EnumImpl<Schema, E> {
    using EnumTag = void;
    using Impl = detail::EnumImpl<Schema, E>;
    using Base = Enum;

    using Impl::add;
    using Impl::name;
};

template<typename Schema, typename T>
struct Variant : detail::VariantImpl<Schema, T> {
    using VariantTag = void;
    using Impl = detail::VariantImpl<Schema, T>;

    using Impl::discriminator;
    using Impl::name;
};

// Custom mapping base ///////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
class Custom
{
public:
    using CustomTag = void;
    using ConfigType = typename Schema::ConfigType;
    using EncodeContext = detail::EncodeContext<Schema>;
    using DecodeContext = detail::DecodeContext<Schema>;

    void name(std::string_view value)
    {
        if (value.empty()) {
            if constexpr (detail::getSchemaEnableAssert<Schema>()) {
                assert(false && "Custom name cannot be empty.");
            }
            return;
        }
        if (hasName_) {
            if constexpr (detail::getSchemaEnableAssert<Schema>()) {
                assert(false && "Custom name already set.");
            }
            return;
        }
        hasName_ = true;
        name_ = std::string(value);
        if constexpr (detail::getSchemaEnableIntrospection<Schema>()) {
            detail::setTypeName<Schema, T>(name_);
        }
    }

    const std::string& name() const { return name_; }

    bool hasName() const { return hasName_; }

private:
    std::string name_;
    bool hasName_ = false;
};

// Introspection ///////////////////////////////////////////////////////////////////////////

template<typename Schema>
class Introspection
{
public:
    template<typename T>
    void add()
    {
        impl_.template add<T>();
    }

    const ObjectInfoMap& objects() const { return impl_.objects(); }
    const EnumInfoMap& enums() const { return impl_.enums(); }
    const VariantInfoMap& variants() const { return impl_.variants(); }

private:
    detail::IntrospectionImpl<Schema> impl_;
};

// API functions //////////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
Result encode(const T& value, Json::Value& dst, const typename Schema::ConfigType& config = {})
{
    detail::EncodeContext<Schema> ctx(config);
    ctx.encode(value, dst);
    return Result{ctx.takeErrors()};
}

template<typename Schema, typename T>
Result decode(const Json::Value& src, T& value, const typename Schema::ConfigType& config = {})
{
    detail::DecodeContext<Schema> ctx(config);
    ctx.decode(src, value);
    return Result{ctx.takeErrors()};
}

template<typename Schema>
Introspection<Schema> introspect()
{
    return Introspection<Schema>{};
}

}  // namespace aison
