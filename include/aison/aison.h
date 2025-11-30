#pragma once
#include <json/json.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// Forward declarations ////////////////////////////////////////////////////////////////////////////

namespace aison {

struct EmptyConfig;
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

template<typename Derived, typename Config>
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
constexpr bool getEncodeEnabled();

template<typename Schema>
constexpr bool getDecodeEnabled();

template<typename Schema>
constexpr bool getEnableAssert();

template<typename Schema>
constexpr bool getStrictOptional();

template<typename Schema>
constexpr bool getEnableIntrospection();

template<typename Schema, typename T>
constexpr void validateEnumSpec();

template<typename Schema, typename T>
constexpr void validateObjectSpec();

template<typename Schema, typename T>
constexpr void validateCustomSpec();

template<typename Schema, typename Variant>
constexpr void validateVariantSpec();

template<typename Owner, typename T>
FieldAccessorId getFieldAccessorId();

template<typename Owner, typename T>
FieldAccessorStorage makeFieldAccessor(T Owner::* member);

template<typename Schema, typename T>
auto& getVariantDef();

template<typename Schema, typename T>
auto& getObjectDef();

template<typename Schema, typename T>
auto& getEnumDef();

template<typename Schema, typename T>
auto& getCustomDef();

template<typename Schema, typename T>
constexpr bool isSupportedFieldType();

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

struct SchemaDefaults {
    static constexpr auto strictOptional = true;
    static constexpr auto enableAssert = true;
    static constexpr auto enableIntrospection = false;
    static constexpr auto enableEncode = true;
    static constexpr auto enableDecode = true;
};

template<typename Derived, typename Config = EmptyConfig>
struct Schema {
    using SchemaTag = void;
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

struct IntrospectError {
    std::string type;
    std::string message;
};

struct IntrospectResult {
    ObjectInfoMap objects;
    VariantInfoMap variants;
    EnumInfoMap enums;
    std::vector<IntrospectError> errors;

    explicit operator bool() const { return errors.empty(); }
};

}  // namespace aison

namespace aison::detail {

template<typename T>
TypeId getTypeId()
{
    static int id = 0x71931d;
    return &id;
}

class Context
{
public:
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

    PathGuard guard(const std::string& key) { return PathGuard(*this, key); }
    PathGuard guard(const char* key) { return PathGuard(*this, key); }
    PathGuard guard(std::size_t index) { return PathGuard(*this, index); }

    void addError(const std::string& msg) { errors_.push_back(Error{buildPath(), msg}); }
    size_t errorCount() const { return errors_.size(); }
    std::vector<Error> takeErrors() { return std::move(errors_); }
    const std::vector<Error>& errors() const { return errors_; }

private:
    std::vector<PathSegment> pathStack_;
    std::vector<Error> errors_;
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

template<typename Schema, typename Variant, std::size_t... Is>
constexpr bool isSupportedVariantAlternatives(std::index_sequence<Is...>)
{
    return (HasObjectTag<Schema, std::variant_alternative_t<Is, Variant>>::value && ...);
}

template<typename Schema, typename T>
constexpr bool isSupportedFieldType()
{
    using U = std::decay_t<T>;
    if constexpr (IsOptional<U>::value) {
        return isSupportedFieldType<Schema, typename U::value_type>();
    } else if constexpr (IsVector<U>::value) {
        return isSupportedFieldType<Schema, typename U::value_type>();
    } else if constexpr (IsVariant<U>::value) {
        if constexpr (!HasVariantTag<Schema, U>::value) {
            return false;
        }
        return isSupportedVariantAlternatives<Schema, U>(
            std::make_index_sequence<std::variant_size_v<U>>{});
    } else if constexpr (HasObjectTag<Schema, U>::value) {
        return true;
    } else if constexpr (HasEnumTag<Schema, U>::value) {
        return true;
    } else if constexpr (HasCustomTag<Schema, U>::value) {
        return true;
    } else if constexpr (std::is_same_v<U, bool>) {
        return true;
    } else if constexpr (std::is_integral_v<U>) {
        return !std::is_same_v<U, bool>;
    } else if constexpr (std::is_same_v<U, float> || std::is_same_v<U, double>) {
        return true;
    } else if constexpr (std::is_same_v<U, std::string>) {
        return true;
    } else {
        return false;
    }
}

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
        auto& obj = getObjectDef<Schema, T>();
        if constexpr (getEnableIntrospection<Schema>()) {
            if constexpr (getEnableAssert<Schema>()) {
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
    if constexpr (getEnableIntrospection<Schema>()) {
        using Alt = std::variant_alternative_t<Index, Variant>;
        auto& obj = getObjectDef<Schema, Alt>();
        AlternativeInfo info;
        info.type = &makeTypeInfo<Schema, Alt>();
        if (obj.hasName()) {
            info.name = obj.name();
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
        if constexpr (getEnableIntrospection<Schema>()) {
            auto& reg = getIntrospectionRegistry<Schema>();
            auto& var = getVariantDef<Schema, T>();
            if constexpr (getEnableAssert<Schema>()) {
                assert(
                    var.hasName() &&
                    "Schema::Variant<V>::name(...) is required when introspection is enabled.");
            }
            if (var.hasName()) {
                reg.setVariantName(getTypeId<T>(), var.name());
                setTypeName<Schema, T>(var.name());
            }
            reg.setVariantDiscriminator(getTypeId<T>(), var.discriminator());
            registerVariantAlternatives<Schema, T>(
                std::make_index_sequence<std::variant_size_v<T>>{});
        }
    }
}

template<typename Schema, typename E>
void registerEnumMapping()
{
    if constexpr (HasEnumTag<Schema, E>::value) {
        auto& def = getEnumDef<Schema, E>();
        if constexpr (getEnableIntrospection<Schema>()) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(
                    def.hasName() &&
                    "Schema::Enum<E>::name(...) is required when introspection is enabled.");
            }
            if (def.hasName()) {
                getIntrospectionRegistry<Schema>().setEnumName(getTypeId<E>(), def.name());
                setTypeName<Schema, E>(def.name());
            }
        }
    }
}

template<typename Schema, typename T>
void registerCustomMapping()
{
    if constexpr (HasCustomTag<Schema, T>::value) {
        auto& custom = getCustomDef<Schema, T>();
        if constexpr (getEnableIntrospection<Schema>()) {
            if constexpr (getEnableAssert<Schema>()) {
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
    if constexpr (!getEnableIntrospection<Schema>()) {
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
        validateVariantSpec<Schema, T>();
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
    static std::string storedName;
    storedName = name;
    auto& info = makeTypeInfo<Schema, T>();
    info.name = storedName.empty() ? nullptr : storedName.c_str();
}

template<typename Schema>
class IntrospectionRegistry
{
public:
    void addObjectField(TypeId typeId, FieldInfo field)
    {
        if (!getEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = objects_[typeId];
        entry.fields.push_back(std::move(field));
    }

    void setObjectName(TypeId typeId, std::string name)
    {
        if (!getEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = objects_[typeId];
        entry.name = std::move(name);
    }

    void addEnumName(TypeId typeId, std::string_view name)
    {
        if (!getEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = enums_[typeId];
        entry.values.push_back(std::string(name));
    }

    void setEnumName(TypeId typeId, std::string name)
    {
        if (!getEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = enums_[typeId];
        entry.name = std::move(name);
    }

    void setVariantName(TypeId typeId, std::string name)
    {
        if (!getEnableIntrospection<Schema>()) {
            return;
        }
        variants_[typeId].name = std::move(name);
    }

    void setVariantDiscriminator(TypeId typeId, std::string key)
    {
        if (!getEnableIntrospection<Schema>()) {
            return;
        }
        variants_[typeId].discriminator = std::move(key);
    }

    void addVariantAlternative(TypeId typeId, AlternativeInfo alt)
    {
        if (!getEnableIntrospection<Schema>()) {
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

template<typename Schema, typename E>
class EnumImpl
{
public:
    using Entry = std::pair<E, std::string>;
    using EntryVec = std::vector<Entry>;

    void name(std::string_view value)
    {
        if (value.empty()) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Enum name cannot be empty.");
            }
            return;
        }
        if (hasName_) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Enum name already set.");
            }
            return;
        }
        hasName_ = true;
        name_ = std::string(value);
        if constexpr (getEnableIntrospection<Schema>()) {
            setTypeName<Schema, E>(name_);
            getIntrospectionRegistry<Schema>().setEnumName(getTypeId<E>(), name_);
        }
    }

    void add(E value, std::string_view name)
    {
        for (const auto& entry : entries_) {
            // Disallow duplicate value or duplicate name
            if (entry.first == value || entry.second == name) {
                if constexpr (getEnableAssert<Schema>()) {
                    assert(false && "Duplicate enum mapping in Schema::Enum.");
                }
                return;
            }
        }
        entries_.emplace_back(value, std::string(name));
        if constexpr (getEnableIntrospection<Schema>()) {
            getIntrospectionRegistry<Schema>().addEnumName(getTypeId<E>(), name);
        }
    }

    const std::string* find(E value) const
    {
        for (auto& entry : entries_) {
            if (entry.first == value) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    const E* find(const std::string& value) const
    {
        for (auto& entry : entries_) {
            if (entry.second == value) {
                return &entry.first;
            }
        }
        return nullptr;
    }

    const EntryVec& entries() const { return entries_; }
    const std::string& name() const { return name_; }
    bool hasName() const { return hasName_; }

private:
    std::vector<Entry> entries_;
    std::string name_;
    bool hasName_ = false;
};

template<typename Schema, typename... Ts>
class VariantImpl<Schema, std::variant<Ts...>>
{
public:
    using VariantType = std::variant<Ts...>;

    void name(std::string_view value)
    {
        if (value.empty()) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Variant name cannot be empty.");
            }
            return;
        }
        if (hasName_) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Variant name already set.");
            }
            return;
        }
        hasName_ = true;
        name_ = std::string(value);
        if constexpr (getEnableIntrospection<Schema>()) {
            setTypeName<Schema, VariantType>(name_);
        }
    }

    void discriminator(std::string_view key)
    {
        if (key.empty()) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Discriminator key cannot be empty.");
            }
            return;
        }
        if (hasDiscriminator_) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Variant discriminator already set.");
            }
            return;
        }
        hasDiscriminator_ = true;
        discriminator_ = std::string(key);
    }

    bool hasNamesInAlternatives() const
    {
        bool hasNames = true;
        ((hasNames = hasNames && getObjectDef<Schema, Ts>().hasName()), ...);

        return hasNames;
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
constexpr void validateObjectType()
{
    validateObjectSpec<Schema, T>();
}

// Variant validation //////////////////////////////////////////////////////////////////////

template<typename Schema, typename Variant, typename Enable = void>
struct VariantValidator {
    static constexpr void validate() {}
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
        if constexpr (HasObjectTag<Schema, T>::value) {
            using ObjectSpec = typename Schema::template Object<T>;
            static_assert(
                std::is_base_of_v<Object<Schema, T>, ObjectSpec>,
                "Schema::Object<T> must inherit from aison::Object<Schema, T>.");
        }
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
            std::is_base_of_v<Variant<Schema, std::variant<Ts...>>, VariantSpec>,
            "Schema::Variant<V> must inherit from aison::Variant<Schema, V>.");
        static_assert(sizeof...(Ts) > 0, "std::variant must have at least one alternative.");
        // Each alternative must have an object mapping.
        (VariantAltCheck<Schema, Ts>::check(), ...);
    }
};

template<typename Key>
bool shouldReportSchemaError()
{
    static bool reported = false;
    if (reported) {
        return false;
    }
    reported = true;
    return true;
}

template<typename...>
struct SchemaErrorKeyTag {};

template<typename Key>
void addSchemaErrorOnce(Context& ctx, const std::string& message)
{
    if (shouldReportSchemaError<Key>()) {
        ctx.addError(message);
    }
}

template<typename Schema, typename T>
constexpr void validateObjectSpec()
{
    using Type = std::decay_t<T>;
    static_assert(
        HasObjectTag<Schema, Type>::value,
        "No schema object mapping for this type. Define `template<> struct Schema::Object<T> : "
        "aison::Object<Schema, T>` and map its fields.");
    using ObjectSpec = typename Schema::template Object<Type>;
    static_assert(
        std::is_base_of_v<aison::Object<Schema, Type>, ObjectSpec>,
        "Schema::Object<T> must inherit from aison::Object<Schema, T>.");
}

template<typename Schema, typename T>
void validateObjectSchema(Context& ctx, const typename Schema::template Object<T>& obj)
{
    validateObjectSpec<Schema, T>();
    if constexpr (getEnableIntrospection<Schema>()) {
        if (!obj.hasName()) {
            using Key = SchemaErrorKeyTag<Schema, std::decay_t<T>, struct ObjectNameMissing>;
            addSchemaErrorOnce<Key>(
                ctx,
                "(Schema error) Schema::Object<T>::name(...) is required when "
                "introspection is enabled.");
        }
    }
}

template<typename Schema, typename T>
constexpr void validateEnumSpec()
{
    using Type = std::decay_t<T>;
    static_assert(
        HasEnumTag<Schema, Type>::value,
        "No schema enum mapping for this type. Define `template<> struct Schema::Enum<T> : "
        "aison::Enum<Schema, T>` and list all enum values.");
    using EnumSpec = typename Schema::template Enum<Type>;
    static_assert(
        std::is_base_of_v<aison::Enum<Schema, Type>, EnumSpec>,
        "Schema::Enum<T> must inherit from aison::Enum<Schema, T>.");
}

template<typename Schema, typename T>
void validateEnumSchema(Context& ctx, const typename Schema::template Enum<T>& enumDef)
{
    validateEnumSpec<Schema, T>();
    if constexpr (getEnableIntrospection<Schema>()) {
        if (!enumDef.hasName()) {
            using Key = SchemaErrorKeyTag<Schema, std::decay_t<T>, struct EnumNameMissing>;
            addSchemaErrorOnce<Key>(
                ctx,
                "(Schema error) Schema::Enum<T>::name(...) is required when introspection is "
                "enabled.");
        }
    }
}

template<typename Schema, typename T>
constexpr void validateCustomSpec()
{
    using Type = std::decay_t<T>;
    static_assert(
        HasCustomTag<Schema, Type>::value,
        "No schema custom mapping for this type. Define `template<> struct Schema::Custom<T> : "
        "aison::Custom<Schema, T>`.");
    using CustomSpec = typename Schema::template Custom<Type>;
    static_assert(
        std::is_base_of_v<aison::Custom<Schema, Type>, CustomSpec>,
        "Schema::Custom<T> must inherit from aison::Custom<Schema, T>.");
}

template<typename Schema, typename T>
void validateCustomSchema(Context& ctx, const typename Schema::template Custom<T>& custom)
{
    validateCustomSpec<Schema, T>();
    if constexpr (getEnableIntrospection<Schema>()) {
        if (!custom.hasName()) {
            using Key = SchemaErrorKeyTag<Schema, std::decay_t<T>, struct CustomNameMissing>;
            addSchemaErrorOnce<Key>(
                ctx,
                "(Schema error) Schema::Custom<T>::name(...) is required when introspection "
                "is enabled.");
        }
    }
}

template<typename Schema, typename Variant>
constexpr void validateVariantSpec()
{
    using Type = std::decay_t<Variant>;
    static_assert(
        IsVariant<Type>::value,
        "Schema::Variant<T> must map a std::variant of object-mapped alternatives.");
    VariantValidator<Schema, Type>::validate();
}

template<typename Schema, typename Variant>
bool validateVariantSchema(Context& ctx, const typename Schema::template Variant<Variant>& def)
{
    using Type = std::decay_t<Variant>;
    validateVariantSpec<Schema, Type>();
    bool ok = true;
    if constexpr (getEnableIntrospection<Schema>()) {
        if (!def.hasName()) {
            using Key = SchemaErrorKeyTag<Schema, Type, struct VariantNameMissing>;
            addSchemaErrorOnce<Key>(
                ctx,
                "(Schema error) Schema::Variant<V>::name(...) is required when introspection "
                "is enabled.");
            ok = false;
        }
    }
    if (!def.hasDiscriminator()) {
        using Key = SchemaErrorKeyTag<Schema, Type, struct VariantDiscriminatorMissing>;
        addSchemaErrorOnce<Key>(ctx, "(Schema error) Discriminator key not set.");
        ok = false;
    }
    if (!def.hasNamesInAlternatives()) {
        using Key = SchemaErrorKeyTag<Schema, Type, struct VariantAltNameMissing>;
        addSchemaErrorOnce<Key>(ctx, "(Schema error) Variant alternative missing name.");
        ok = false;
    }
    return ok;
}

template<typename Schema, typename T>
auto& getVariantDef()
{
    using Type = typename Schema::template Variant<T>;
    static Type instance{};
    return instance;
}

template<typename Schema, typename T>
auto& getObjectDef()
{
    using Type = typename Schema::template Object<T>;
    static Type instance{};
    return instance;
}

template<typename Schema, typename T>
auto& getEnumDef()
{
    using Type = typename Schema::template Enum<T>;
    static Type instance{};
    return instance;
}

template<typename Schema, typename T>
auto& getCustomDef()
{
    using Type = typename Schema::template Custom<T>;
    static Type instance{};
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
constexpr bool getEnableAssert()
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
constexpr bool getStrictOptional()
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
constexpr bool getEnableIntrospection()
{
    return SchemaEnableIntrospection<Schema>::get();
}

template<typename Schema, typename = void>
struct HasSchemaTag : std::false_type {};

template<typename Schema>
struct HasSchemaTag<Schema, std::void_t<typename Schema::SchemaTag>> : std::true_type {};

template<typename Schema>
constexpr void validateSchemaDefinition()
{
    static_assert(
        HasSchemaTag<Schema>::value,
        "Schema must define SchemaTag. Did you inherit from aison::Schema<...>?");
    using Base = aison::Schema<Schema, typename Schema::ConfigType>;
    static_assert(
        std::is_base_of_v<Base, Schema>,
        "Schema must inherit from aison::Schema<Derived, Config>.");
}

template<typename Schema>
constexpr void validateSchemaBase()
{
    validateSchemaDefinition<Schema>();
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
        validateSchemaDefinition<Schema>();
        static_assert(
            getEncodeEnabled<Schema>(),
            "EncodeContext<Schema> cannot be used when Schema::enableEncode is false.");
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
        validateSchemaBase<Schema>();
        static_assert(
            getDecodeEnabled<Schema>(),
            "DecodeContext<Schema> cannot be used when Schema::enableDecode is false.");
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

// Encode ///////////////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
void encodeValue(const T& src, Json::Value& dst, EncodeContext<Schema>& ctx)
{
    if constexpr (HasCustomTag<Schema, T>::value) {
        using CustomSpec = typename Schema::template Custom<T>;
        static_assert(
            HasCustomEncode<Schema, CustomSpec, T>::value,
            "Schema::Custom<T> must implement encode(const T&, Json::Value&, EncodeContext&).");
        auto& custom = getCustomDef<Schema, T>();
        validateCustomSchema<Schema, T>(ctx, custom);
        custom.encode(src, dst, ctx);

    } else if constexpr (HasObjectTag<Schema, T>::value) {
        const auto& def = getObjectDef<Schema, T>();
        validateObjectSchema<Schema, T>(ctx, def);
        def.encodeFields(src, dst, ctx);

    } else if constexpr (HasEnumTag<Schema, T>::value) {
        using EnumSpec = typename Schema::template Enum<T>;
        auto& def = getEnumDef<Schema, T>();
        validateEnumSchema<Schema, T>(ctx, def);
        auto* str = def.find(src);
        if (str) {
            dst = *str;
        } else {
            using U = typename std::underlying_type<T>::type;
            ctx.addError(
                "Unhandled enum value during encode (underlying = " + std::to_string(U(src)) +
                ").");
        }

    } else if constexpr (HasVariantTag<Schema, T>::value) {
        auto& var = getVariantDef<Schema, T>();
        if (!validateVariantSchema<Schema, T>(ctx, var)) {
            return;
        }

        dst = Json::objectValue;
        std::visit(
            [&](const auto& alt) {
                using Alt = std::decay_t<decltype(alt)>;
                auto& obj = getObjectDef<Schema, Alt>();
                obj.encodeFields(alt, dst, ctx);
                dst[var.discriminator()] = obj.name();
            },
            src);

    } else if constexpr (IsOptional<T>::value) {
        using U = typename T::value_type;
        if (!src) {
            dst = Json::nullValue;
        } else {
            encodeValue<Schema, U>(*src, dst, ctx);
        }

    } else if constexpr (IsVector<T>::value) {
        using U = typename T::value_type;
        dst = Json::arrayValue;
        std::size_t index = 0;
        for (const auto& elem : src) {
            Context::PathGuard guard(ctx, index++);
            Json::Value v;
            encodeValue<Schema, U>(elem, v, ctx);
            dst.append(v);
        }

    } else if constexpr (std::is_same_v<T, bool>) {
        dst = src;

    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        dst = static_cast<std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>>(src);

    } else if constexpr (std::is_same_v<T, float>) {
        if (std::isnan(src)) {
            ctx.addError("NaN is not allowed here.");
            return;
        }
        dst = src;

    } else if constexpr (std::is_same_v<T, double>) {
        if (std::isnan(src)) {
            ctx.addError("NaN is not allowed here.");
            return;
        }
        dst = src;

    } else if constexpr (std::is_same_v<T, std::string>) {
        dst = src;

    } else if constexpr (std::is_enum_v<T>) {
        static_assert(DependentFalse<T>::value, "Enums must have an Schema::Enum<T> mapping.");

    } else if constexpr (std::is_pointer_v<T>) {
        static_assert(DependentFalse<T>::value, "Pointers are not supported.");

    } else {
        static_assert(
            DependentFalse<T>::value,
            "Unsupported type. Define Schema::Custom<T> to provide an encoder.");
    }
}

// Decode //////////////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& dst, DecodeContext<Schema>& ctx)
{
    if constexpr (HasCustomTag<Schema, T>::value) {
        using CustomSpec = typename Schema::template Custom<T>;
        static_assert(
            HasCustomDecode<Schema, CustomSpec, T>::value,
            "Schema::Custom<T> must implement decode(const Json::Value&, T&, DecodeContext&).");
        auto& custom = getCustomDef<Schema, T>();
        validateCustomSchema<Schema, T>(ctx, custom);
        custom.decode(src, dst, ctx);

    } else if constexpr (HasObjectTag<Schema, T>::value) {
        if (!src.isObject()) {
            ctx.addError("Expected object.");
            return;
        }

        const auto& obj = getObjectDef<Schema, T>();
        validateObjectSchema<Schema, T>(ctx, obj);
        obj.decodeFields(src, dst, ctx);

    } else if constexpr (HasEnumTag<Schema, T>::value) {
        if (!src.isString()) {
            ctx.addError("Expected string for enum.");
            return;
        }

        auto& def = getEnumDef<Schema, T>();
        validateEnumSchema<Schema, T>(ctx, def);
        auto* value = def.find(src.asString());
        if (value) {
            dst = *value;
        } else {
            ctx.addError("Unknown enum value '" + src.asString() + "'.");
        }

    } else if constexpr (HasVariantTag<Schema, T>::value) {
        VariantDecoder<Schema, T>::decode(src, dst, ctx);

    } else if constexpr (IsOptional<T>::value) {
        using U = typename T::value_type;
        if (src.isNull()) {
            dst.reset();
        } else {
            U tmp{};
            decodeValue<Schema, U>(src, tmp, ctx);
            dst = std::move(tmp);
        }

    } else if constexpr (IsVector<T>::value) {
        using U = typename T::value_type;
        dst.clear();
        if (!src.isArray()) {
            ctx.addError("Expected array.");
            return;
        }
        for (Json::ArrayIndex i = 0; i < src.size(); ++i) {
            auto guard = ctx.guard(i);
            U elem{};
            decodeValue<Schema, U>(src[i], elem, ctx);
            dst.push_back(std::move(elem));
        }

    } else if constexpr (std::is_same_v<T, bool>) {
        if (!src.isBool()) {
            ctx.addError("Expected bool.");
            return;
        }
        dst = src.asBool();

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
            dst = static_cast<T>(v);
        } else {
            auto v = src.asUInt64();
            if (v > std::numeric_limits<T>::max()) {
                ctx.addError("Unsigned integer value out of range.");
                return;
            }
            dst = static_cast<T>(v);
        }
    } else if constexpr (std::is_same_v<T, float>) {
        if (!src.isDouble() && !src.isInt()) {
            ctx.addError("Expected float.");
            return;
        }
        dst = static_cast<float>(src.asDouble());
    } else if constexpr (std::is_same_v<T, double>) {
        if (!src.isDouble() && !src.isInt()) {
            ctx.addError("Expected double.");
            return;
        }
        dst = src.asDouble();
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!src.isString()) {
            ctx.addError("Expected string.");
            return;
        }
        dst = src.asString();

    } else if constexpr (std::is_enum_v<T>) {
        static_assert(DependentFalse<T>::value, "Enums must have an Schema::Enum<T> mapping.");

    } else if constexpr (std::is_pointer_v<T>) {
        static_assert(DependentFalse<T>::value, "Pointers are not supported.");

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

    static void decode(const Json::Value& src, VariantType& dst, DecodeContext<Schema>& ctx)
    {
        auto& var = getVariantDef<Schema, VariantType>();
        if (!validateVariantSchema<Schema, VariantType>(ctx, var)) {
            return;
        }

        if (!src.isObject()) {
            ctx.addError("Expected object for variant.");
            return;
        }

        auto& tag = var.discriminator();
        std::string tagValue;

        {
            auto discGuard = ctx.guard(tag);
            if (!src.isMember(tag)) {
                ctx.addError("Missing discriminator field.");
                return;
            }

            const Json::Value& tagNode = src[tag];
            if (!tagNode.isString()) {
                ctx.addError("Expected string.");
                return;
            }
            tagValue = tagNode.asString();
        }

        bool matched = false;
        (tryAlternative<Ts>(tag, tagValue, src, dst, ctx, matched), ...);
        if (!matched) {
            auto discGuard = ctx.guard(tag);
            ctx.addError("Unknown discriminator value for variant.");
        }
    }

    template<typename Alt>
    static void tryAlternative(
        const std::string& tag,
        const std::string& tagValue,
        const Json::Value& src,
        VariantType& dst,
        DecodeContext<Schema>& ctx,
        bool& matched)
    {
        using ObjectSpec = typename Schema::template Object<Alt>;
        const auto& objectDef = getObjectDef<Schema, Alt>();
        if (matched || tagValue != objectDef.variantTag()) {
            return;
        }

        matched = true;

        Alt alt{};
        objectDef.decodeFields(src, alt, ctx);
        dst = std::move(alt);
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

template<typename Schema, typename = void>
struct HasSchemaEnableEncode : std::false_type {};

template<typename Schema>
struct HasSchemaEnableEncode<Schema, std::void_t<decltype(Schema::enableEncode)>>
    : std::true_type {};

template<typename Schema, typename = void>
struct HasSchemaEnableDecode : std::false_type {};

template<typename Schema>
struct HasSchemaEnableDecode<Schema, std::void_t<decltype(Schema::enableDecode)>>
    : std::true_type {};

template<typename Schema, typename = void>
struct SchemaEnableEncode {
    constexpr static bool get() { return SchemaDefaults::enableEncode; }
};

template<typename Schema>
struct SchemaEnableEncode<Schema, std::enable_if_t<HasSchemaEnableEncode<Schema>::value>> {
    constexpr static bool get()
    {
        using Type = std::decay_t<decltype(Schema::enableEncode)>;
        static_assert(std::is_same_v<Type, bool>, "Schema::enableEncode must be bool.");
        return Schema::enableEncode;
    }
};

template<typename Schema, typename = void>
struct SchemaEnableDecode {
    constexpr static bool get() { return SchemaDefaults::enableDecode; }
};

template<typename Schema>
struct SchemaEnableDecode<Schema, std::enable_if_t<HasSchemaEnableDecode<Schema>::value>> {
    constexpr static bool get()
    {
        using Type = std::decay_t<decltype(Schema::enableDecode)>;
        static_assert(std::is_same_v<Type, bool>, "Schema::enableDecode must be bool.");
        return Schema::enableDecode;
    }
};

template<typename Schema>
constexpr bool getEncodeEnabled()
{
    return SchemaEnableEncode<Schema>::get();
}

template<typename Schema>
constexpr bool getDecodeEnabled()
{
    return SchemaEnableDecode<Schema>::get();
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
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Object name cannot be empty.");
            }
            return;
        }
        if (hasName_) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Object name already set.");
            }
            return;
        }
        hasName_ = true;
        name_ = std::string(value);
        if constexpr (getEnableIntrospection<Schema>()) {
            setTypeName<Schema, Owner>(name_);
            getIntrospectionRegistry<Schema>().setObjectName(detail::getTypeId<Owner>(), name_);
        }
    }

    template<typename T>
    void add(T Owner::* member, std::string_view name)
    {
        static_assert(
            isSupportedFieldType<Schema, T>(),
            "Unsupported field type in Schema::Object<T>::add(...). Provide a Schema::Object / "
            "Enum / Custom / Variant mapping or use supported scalar/collection types.");
        // Check if member or name is already mapped
        auto accessorId = getFieldAccessorId<Owner, T>();
        for (const auto& field : fields_) {
            using Ctx = FieldAccessor<Owner, T>;
            if (field.accessorId == accessorId &&
                reinterpret_cast<const Ctx*>(field.accessor.get())->member == member)
            {
                if constexpr (getEnableAssert<Schema>()) {
                    assert(false && "Same member is mapped multiple times in Schema::Object.");
                }
                return;
            }

            if (field.name == name) {
                if constexpr (getEnableAssert<Schema>()) {
                    assert(false && "Duplicate field name in Schema::Object.");
                }
                return;
            }
        }

        auto& field = fields_.emplace_back(makeFieldAccessor(member));

        field.name = std::string(name);
        field.accessorId = accessorId;
        field.isOptional = IsOptional<T>::value;

        if constexpr (getEncodeEnabled<Schema>()) {
            field.encode = &encodeFieldThunk<Schema, Owner, T>;
        }

        if constexpr (getDecodeEnabled<Schema>()) {
            field.decode = &decodeFieldThunk<Schema, Owner, T>;
        }

        if constexpr (getEnableIntrospection<Schema>()) {
            FieldInfo fi;
            fi.name = std::string(name);
            fi.type = &makeTypeInfo<Schema, T>();
            getIntrospectionRegistry<Schema>().addObjectField(getTypeId<Owner>(), std::move(fi));
            ensureTypeRegistration<Schema, Owner, T>();
        }
    }

    template<typename S = Schema, typename = std::enable_if_t<getEncodeEnabled<S>()>>
    void encodeFields(const Owner& src, Json::Value& dst, EncodeContext<Schema>& ctx) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            Context::PathGuard guard(ctx, field.name);
            Json::Value node;
            field.encode(src, node, ctx, field.accessor.get());
            if (!getStrictOptional<Schema>() && field.isOptional && node.isNull()) {
                continue;
            }
            dst[field.name] = std::move(node);
        }
    }

    template<typename S = Schema, typename = std::enable_if_t<getDecodeEnabled<S>()>>
    void decodeFields(const Json::Value& src, Owner& dst, DecodeContext<Schema>& ctx) const
    {
        for (const auto& field : fields_) {
            const auto& key = field.name;
            if (!src.isMember(key)) {
                if (!getStrictOptional<Schema>() && field.isOptional) {
                    auto guard = ctx.guard(key);
                    field.decode(Json::nullValue, dst, ctx, field.accessor.get());
                    continue;
                }
                ctx.addError(std::string("Missing required field '") + key + "'.");
                continue;
            }
            const Json::Value& node = src[key];
            auto guard = ctx.guard(key);
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
    explicit IntrospectionImpl(std::vector<IntrospectError>& errors)
        : errors_(&errors)
    {
        validateSchemaDefinition<Schema>();
        static_assert(
            detail::getEnableIntrospection<Schema>(),
            "Introspection is disabled for this schema. Set `static constexpr bool "
            "enableIntrospection = true;` in your Schema to use Introspection.");
    }

    template<typename T>
    void add()
    {
        using U = std::decay_t<T>;
        if constexpr (std::is_enum_v<U>) {
            validateEnumSpec<Schema, U>();
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
            reportMissingName(typeId, "Enum must have name() when introspection is enabled.");
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
            reportMissingName(typeId, "Object must have name() when introspection is enabled.");
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
            reportMissingName(typeId, "Variant must have name() when introspection is enabled.");
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
    void reportMissingName(TypeId typeId, const std::string& message)
    {
        if (!errors_) return;
        std::string typeName;
        auto& reg = getIntrospectionRegistry<Schema>();
        auto objIt = reg.objects().find(typeId);
        if (objIt != reg.objects().end() && !objIt->second.name.empty()) {
            typeName = objIt->second.name;
        } else {
            typeName = "#" + std::to_string(reinterpret_cast<std::uintptr_t>(typeId));
        }
        errors_->push_back(IntrospectError{std::move(typeName), message});
    }

    ObjectInfoMap objects_;
    VariantInfoMap variants_;
    EnumInfoMap enums_;
    std::vector<IntrospectError>* errors_ = nullptr;
};

}  // namespace aison::detail

namespace aison {

// Object / Enum / Variant / Custom  ///////////////////////////////////////////////////////////

template<typename Schema, typename T>
struct Object : detail::ObjectImpl<Schema, T> {
    using ObjectTag = void;
    using Impl = detail::ObjectImpl<Schema, T>;

    using Impl::add;
    using Impl::name;

    Object() { detail::validateObjectSpec<Schema, T>(); }
};

template<typename Schema, typename T>
struct Enum : detail::EnumImpl<Schema, T> {
    using EnumTag = void;
    using Impl = detail::EnumImpl<Schema, T>;
    using Base = Enum;

    using Impl::add;
    using Impl::name;

    Enum() { detail::validateEnumSpec<Schema, T>(); }
};

template<typename Schema, typename T>
struct Variant : detail::VariantImpl<Schema, T> {
    using VariantTag = void;
    using Impl = detail::VariantImpl<Schema, T>;

    using Impl::discriminator;
    using Impl::name;

    Variant() { detail::validateVariantSpec<Schema, T>(); }
};

template<typename Schema, typename T>
class Custom
{
public:
    using CustomTag = void;
    using ConfigType = typename Schema::ConfigType;
    using EncodeContext = detail::EncodeContext<Schema>;
    using DecodeContext = detail::DecodeContext<Schema>;

    Custom() { detail::validateCustomSpec<Schema, T>(); }

    void name(std::string_view value)
    {
        if (value.empty()) {
            if constexpr (detail::getEnableAssert<Schema>()) {
                assert(false && "Custom name cannot be empty.");
            }
            return;
        }
        if (hasName_) {
            if constexpr (detail::getEnableAssert<Schema>()) {
                assert(false && "Custom name already set.");
            }
            return;
        }
        hasName_ = true;
        name_ = std::string(value);
        if constexpr (detail::getEnableIntrospection<Schema>()) {
            detail::setTypeName<Schema, T>(name_);
        }
    }

    const std::string& name() const { return name_; }

    bool hasName() const { return hasName_; }

private:
    std::string name_;
    bool hasName_ = false;
};

// API functions //////////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
Result encode(const T& value, Json::Value& dst, const typename Schema::ConfigType& config = {})
{
    detail::validateSchemaDefinition<Schema>();
    detail::EncodeContext<Schema> ctx(config);
    ctx.encode(value, dst);
    return Result{ctx.takeErrors()};
}

template<typename Schema, typename T>
Result decode(const Json::Value& src, T& value, const typename Schema::ConfigType& config = {})
{
    detail::validateSchemaDefinition<Schema>();
    detail::DecodeContext<Schema> ctx(config);
    ctx.decode(src, value);
    return Result{ctx.takeErrors()};
}

template<typename Schema, typename... Ts>
IntrospectResult introspect()
{
    detail::validateSchemaDefinition<Schema>();
    IntrospectResult result;
    detail::IntrospectionImpl<Schema> impl(result.errors);
    (impl.template add<Ts>(), ...);
    result.objects = impl.objects();
    result.enums = impl.enums();
    result.variants = impl.variants();
    return result;
}

}  // namespace aison
