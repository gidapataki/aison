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
#include <tuple>
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
struct Enum;

template<typename Derived, typename FacetTag, typename Config>
struct Schema;

template<typename Schema, typename T>
struct Encoder;

template<typename Schema, typename T>
struct Decoder;

}  // namespace aison

namespace aison::detail {

class Context;
struct PathSegment;
struct PathScope;
struct EnumBase;

using FieldContextDeleter = void (*)(void*);
using FieldContextPtr = std::unique_ptr<void, FieldContextDeleter>;

template<typename Schema>
class EncoderImpl;

template<typename Schema>
class DecoderImpl;

template<typename Schema, typename T>
class EnumImpl;

template<typename Schema, typename Owner>
class ObjectImpl;

template<typename Schema, typename Variant, typename = void>
struct VariantDecoder;

// Traits
template<typename T>
struct IsOptional;

template<typename T>
struct IsVector;

template<typename Schema, typename T, typename = void>
struct HasEnumTag;

template<typename Schema, typename T, typename = void>
struct HasObjectTag;

template<typename Schema, typename T, typename = void>
struct HasEncoderTag;

template<typename Schema, typename T, typename = void>
struct HasDecoderTag;

template<typename Schema, typename = void>
struct HasSchemaDiscriminatorKey;

template<typename Schema, typename = void>
struct SchemaDiscriminatorKey;

template<typename Schema, typename = void>
struct HasSchemaEnableAssert;

template<typename Schema, typename = void>
struct HasSchemaEnableIntrospection;

template<typename Schema, typename = void>
struct SchemaEnableAssert;

template<typename Schema, typename = void>
struct HasSchemaStrictOptional;

template<typename Schema, typename = void>
struct SchemaStrictOptional;

template<typename Schema, typename = void>
struct SchemaEnableIntrospection;

// Functions
template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder);

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder);

template<typename Schema, typename T>
void encodeDefault(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder);

template<typename Schema, typename T>
void decodeDefault(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder);

template<typename Schema>
constexpr bool hasEncodeFacet();

template<typename Schema>
constexpr bool hasDecodeFacet();

template<typename Schema, typename T>
constexpr void validateEnumType();

template<typename Schema, typename Variant>
constexpr void validateVariant();

template<typename Owner, typename T>
const void* getFieldContextId();

template<typename Owner, typename T>
FieldContextPtr makeFieldContext(T Owner::* member);

template<typename T>
T& getSchemaObject();

template<typename Schema>
std::string_view getSchemaDiscriminatorKey();

template<typename Schema>
constexpr bool getSchemaEnableAssert();

template<typename Schema>
constexpr bool getSchemaStrictOptional();

template<typename Schema>
constexpr bool getSchemaEnableIntrospection();

template<typename T>
const void* typeId();

template<typename Schema, typename Owner, typename T>
void ensureTypeRegistration();

template<typename Schema, typename T>
void registerObjectMapping();

template<typename Schema, typename E>
void registerEnumMapping();

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
    static constexpr auto discriminatorKey = "";
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
    // template<typename T> struct Enum;
    // template<typename T> struct Encoder;
    // template<typename T> struct Decoder;
};

}  // namespace aison

namespace aison::detail {

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

protected:
    friend struct PathScope;
    std::vector<PathSegment> pathStack_;
    std::vector<Error> errors_;
};

struct PathScope {
    Context* ctx = nullptr;

    PathScope(Context& context, const std::string& key)
        : PathScope(context, key.c_str())
    {}

    PathScope(Context& context, const char* key)
        : ctx(&context)
    {
        ctx->pathStack_.push_back(PathSegment::makeKey(key));
    }

    PathScope(Context& context, std::size_t index)
        : ctx(&context)
    {
        ctx->pathStack_.push_back(PathSegment::makeIndex(index));
    }

    PathScope(const PathScope&) = delete;
    PathScope& operator=(const PathScope&) = delete;

    PathScope(PathScope&& other) noexcept
        : ctx(other.ctx)
    {
        other.ctx = nullptr;
    }
    PathScope& operator=(PathScope&&) = delete;

    ~PathScope()
    {
        if (ctx) {
            ctx->pathStack_.pop_back();
        }
    }
};

template<typename T>
const void* typeId()
{
    static int id;
    return &id;
}

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

enum class FieldKind { Plain, Optional, Vector, Variant };

enum class BasicType {
    Unknown,
    Bool,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float,
    Double,
    String,
    Enum,
    Object,
    Other,
};

struct TypeInfo {
    FieldKind kind = FieldKind::Plain;
    BasicType basic = BasicType::Unknown;
    const void* typeId = nullptr;
    const TypeInfo* element = nullptr;          // optional/vector element
    const TypeInfo* const* variants = nullptr;  // pointer to array of alternative TypeInfo*
    std::size_t variantCount = 0;
};

struct FieldInfo {
    std::string name;
    const TypeInfo* type = nullptr;
};

struct ObjectInfo {
    const void* typeId = nullptr;
    std::vector<FieldInfo> fields;
    std::string discriminatorKey;
    std::string discriminatorTag;
    bool hasDiscriminator = false;
};

struct EnumInfo {
    const void* typeId = nullptr;
    std::vector<std::string> names;
};

template<typename Schema, typename Owner, typename T>
void ensureTypeRegistration();

template<typename Schema, typename T>
const TypeInfo& makeTypeInfo();

template<typename Schema, typename Variant>
const TypeInfo& makeVariantTypeInfo();

template<typename Schema, typename Variant, std::size_t... Is>
inline const TypeInfo* const* makeVariantAlternatives(std::index_sequence<Is...>)
{
    static const TypeInfo* const arr[] = {
        &makeTypeInfo<Schema, std::variant_alternative_t<Is, Variant>>()...};
    return arr;
}

template<typename Schema, typename T>
void registerObjectMapping()
{
    if constexpr (HasObjectTag<Schema, T>::value) {
        (void)getSchemaObject<typename Schema::template Object<T>>();
    }
}

template<typename Schema, typename E>
void registerEnumMapping()
{
    if constexpr (HasEnumTag<Schema, E>::value) {
        (void)getSchemaObject<typename Schema::template Enum<E>>();
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
        ensureVariantAlternatives<Schema, Owner, VT>(
            std::make_index_sequence<std::variant_size_v<VT>>{});
    } else if constexpr (std::is_enum_v<T>) {
        registerEnumMapping<Schema, T>();
    } else if constexpr (HasObjectTag<Schema, T>::value && !std::is_same_v<Owner, T>) {
        registerObjectMapping<Schema, T>();
    }
}

template<typename Schema, typename T>
const TypeInfo& makeTypeInfo()
{
    if constexpr (IsOptional<T>::value) {
        using Inner = typename T::value_type;
        static const TypeInfo inner = makeTypeInfo<Schema, Inner>();
        static const TypeInfo info =
            TypeInfo{FieldKind::Optional, inner.basic, typeId<T>(), &inner, nullptr, 0};
        return info;
    } else if constexpr (IsVector<T>::value) {
        using Inner = typename T::value_type;
        static const TypeInfo inner = makeTypeInfo<Schema, Inner>();
        static const TypeInfo info =
            TypeInfo{FieldKind::Vector, inner.basic, typeId<T>(), &inner, nullptr, 0};
        return info;
    } else if constexpr (IsVariant<T>::value) {
        return makeVariantTypeInfo<Schema, T>();
    } else if constexpr (std::is_same_v<T, bool>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Bool, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, std::int8_t>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Int8, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, std::uint8_t>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::UInt8, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, std::int16_t>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Int16, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, std::uint16_t>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::UInt16, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, std::int32_t>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Int32, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, std::uint32_t>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::UInt32, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Int64, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, std::uint64_t>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::UInt64, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, float>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Float, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, double>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Double, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_same_v<T, std::string>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::String, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (std::is_enum_v<T>) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Enum, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else if constexpr (HasObjectTag<Schema, T>::value) {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Object, typeId<T>(), nullptr, nullptr, 0};
        return info;
    } else {
        static const TypeInfo info =
            TypeInfo{FieldKind::Plain, BasicType::Other, typeId<T>(), nullptr, nullptr, 0};
        return info;
    }
}

template<typename Schema, typename Variant>
const TypeInfo& makeVariantTypeInfo()
{
    using VariantType = Variant;
    constexpr auto count = std::variant_size_v<VariantType>;
    static const TypeInfo* const* altArray =
        makeVariantAlternatives<Schema, VariantType>(std::make_index_sequence<count>{});
    static const TypeInfo info =
        TypeInfo{FieldKind::Variant, BasicType::Other, typeId<Variant>(), nullptr, altArray, count};
    return info;
}

template<typename Schema>
class IntrospectionRegistry
{
public:
    void addObjectField(const void* typeId, FieldInfo field)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = objects_[typeId];
        entry.typeId = typeId;
        entry.fields.push_back(std::move(field));
    }

    void setObjectDiscriminator(const void* typeId, std::string key, std::string tag)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = objects_[typeId];
        entry.typeId = typeId;
        entry.discriminatorKey = std::move(key);
        entry.discriminatorTag = std::move(tag);
        entry.hasDiscriminator = true;
    }

    void addEnumName(const void* typeId, std::string_view name)
    {
        if (!getSchemaEnableIntrospection<Schema>()) {
            return;
        }
        auto& entry = enums_[typeId];
        entry.typeId = typeId;
        entry.names.push_back(std::string(name));
    }

    const std::unordered_map<const void*, ObjectInfo>& objects() const { return objects_; }
    const std::unordered_map<const void*, EnumInfo>& enums() const { return enums_; }

private:
    std::unordered_map<const void*, ObjectInfo> objects_;
    std::unordered_map<const void*, EnumInfo> enums_;
};

template<typename Schema>
inline IntrospectionRegistry<Schema>& introspectionRegistry()
{
    static IntrospectionRegistry<Schema> reg;
    return reg;
}

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
struct HasEncoderTag : std::false_type {};

template<typename Schema, typename T>
struct HasEncoderTag<Schema, T, std::void_t<typename Schema::template Encoder<T>::EncoderTag>>
    : std::true_type {};

template<typename Schema, typename T, typename>
struct HasDecoderTag : std::false_type {};

template<typename Schema, typename T>
struct HasDecoderTag<Schema, T, std::void_t<typename Schema::template Decoder<T>::DecoderTag>>
    : std::true_type {};

// Enum impl + validation //////////////////////////////////////////////////////////////////

struct EnumBase {};

template<typename Schema, typename E>
class EnumImpl : public EnumBase
{
    using Entry = std::pair<E, std::string>;
    std::vector<Entry> entries_;

public:
    std::size_t size() const { return entries_.size(); }

    auto begin() { return entries_.begin(); }
    auto end() { return entries_.end(); }
    auto begin() const { return entries_.begin(); }
    auto end() const { return entries_.end(); }

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
            introspectionRegistry<Schema>().addEnumName(typeId<E>(), name);
        }
    }
};

template<typename Schema, typename T>
constexpr void validateEnumType()
{
    static_assert(
        HasEnumTag<Schema, T>::value,
        "No schema enum mapping for this type. "
        "Define `template<> struct Schema::Enum<T> : aison::Enum<Schema, T>` and "
        "list all enum values.");
    using EnumDef = typename Schema::template Enum<T>;
    static_assert(
        std::is_base_of_v<EnumBase, EnumDef>,
        "Schema::Enum<T> must inherit from aison::Enum<Schema, T>.");
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
        // Each alternative must have an object mapping.
        (VariantAltCheck<Schema, Ts>::check(), ...);
    }
};

template<typename Schema, typename Context, typename... Ts>
struct VariantKeyValidator<Schema, Context, std::variant<Ts...>, void> {
    static bool validate(Context& ctx)
    {
        using FirstAlt = std::tuple_element_t<0, std::tuple<Ts...>>;
        const auto& firstObj = getSchemaObject<typename Schema::template Object<FirstAlt>>();
        auto firstKey = firstObj.discriminatorKey();
        if (firstKey.empty()) {
            ctx.addError("Discriminator key not set for variant.");
            return false;
        }

        bool emptyKey = false;
        bool mismatch = false;
        ((emptyKey =
              emptyKey ||
              getSchemaObject<typename Schema::template Object<Ts>>().discriminatorKey().empty()),
         ...);
        ((mismatch = mismatch ||
                     (!getSchemaObject<typename Schema::template Object<Ts>>()
                           .discriminatorKey()
                           .empty() &&
                      getSchemaObject<typename Schema::template Object<Ts>>().discriminatorKey() !=
                          firstKey)),
         ...);

        if (emptyKey) {
            ctx.addError("Discriminator key not set for variant.");
            return false;
        }

        if (mismatch) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Variant alternatives must use the same discriminator key.");
            }
            ctx.addError("Variant alternatives must use the same discriminator key.");
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

// DiscriminatorKey

template<typename Schema, typename>
struct HasSchemaDiscriminatorKey : std::false_type {};

template<typename Schema>
struct HasSchemaDiscriminatorKey<Schema, std::void_t<decltype(Schema::discriminatorKey)>>
    : std::true_type {};

template<typename Schema, typename>
struct SchemaDiscriminatorKey {
    static std::string_view get() { return SchemaDefaults::discriminatorKey; }
};

template<typename Schema>
struct SchemaDiscriminatorKey<Schema, std::enable_if_t<HasSchemaDiscriminatorKey<Schema>::value>> {
    static std::string_view get()
    {
        using KeyType = std::decay_t<decltype(Schema::discriminatorKey)>;

        static_assert(
            std::is_same_v<KeyType, const char*> || std::is_same_v<KeyType, std::string_view>,
            "Schema::discriminatorKey must be const either `char*` or `std::string_view`.");

        return std::string_view(Schema::discriminatorKey);
    }
};

template<typename Schema>
std::string_view getSchemaDiscriminatorKey()
{
    return SchemaDiscriminatorKey<Schema>::get();
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
class EncoderImpl : public Context
{
public:
    using Config = typename Schema::ConfigType;

    explicit EncoderImpl(const Config& cfg)
        : config(cfg)
    {
        using Facet = typename Schema::FacetType;
        static_assert(
            std::is_same_v<Facet, EncodeDecode> || std::is_same_v<Facet, EncodeOnly>,
            "EncoderImpl<Schema> cannot be used with a DecodeOnly schema facet.");
    }

    template<typename T>
    Result encode(const T& value, Json::Value& dst)
    {
        this->errors_.clear();
        encodeValue<Schema, T>(value, dst, *this);
        return Result{std::move(this->errors_)};
    }

    const Config& config;
};

template<typename Schema>
class DecoderImpl : public Context
{
public:
    using Config = typename Schema::ConfigType;

    explicit DecoderImpl(const Config& cfg)
        : config(cfg)
    {
        using Facet = typename Schema::FacetType;
        static_assert(
            std::is_same_v<Facet, EncodeDecode> || std::is_same_v<Facet, DecodeOnly>,
            "DecoderImpl<Schema> cannot be used with an EncodeOnly schema facet.");
    }

    template<typename T>
    Result decode(const Json::Value& src, T& value)
    {
        this->errors_.clear();
        decodeValue<Schema, T>(src, value, *this);
        return Result{std::move(this->errors_)};
    }

    const Config& config;
};

// Custom encoder/decoder dispatch /////////////////////////////////////////////////////////

template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder)
{
    if constexpr (HasEncoderTag<Schema, T>::value) {
        using EncodeFunc = typename Schema::template Encoder<T>;
        EncodeFunc encodeFunc;
        encodeFunc.setEncoder(encoder);
        encodeFunc(value, dst);
    } else {
        encodeDefault<Schema, T>(value, dst, encoder);
    }
}

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder)
{
    if constexpr (HasDecoderTag<Schema, T>::value) {
        using DecodeFunc = typename Schema::template Decoder<T>;
        DecodeFunc decodeFunc;
        decodeFunc.setDecoder(decoder);
        decodeFunc(src, value);
    } else {
        decodeDefault<Schema, T>(src, value, decoder);
    }
}

// Encode defaults ///////////////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
void encodeDefault(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder)
{
    if constexpr (std::is_same_v<T, bool>) {
        dst = value;
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        dst = static_cast<std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>>(value);
    } else if constexpr (std::is_same_v<T, float>) {
        if (std::isnan(value)) {
            encoder.addError("NaN is not allowed here.");
            return;
        }
        dst = value;
    } else if constexpr (std::is_same_v<T, double>) {
        if (std::isnan(value)) {
            encoder.addError("NaN is not allowed here.");
            return;
        }
        dst = value;
    } else if constexpr (std::is_same_v<T, std::string>) {
        dst = value;
    } else if constexpr (std::is_enum_v<T>) {
        validateEnumType<Schema, T>();

        using EnumSpec = typename Schema::template Enum<T>;
        const auto& entries = getSchemaObject<EnumSpec>();
        for (const auto& entry : entries) {
            if (entry.first == value) {
                dst = Json::Value(std::string(entry.second));
                return;
            }
        }
        using U = typename std::underlying_type<T>::type;
        encoder.addError(
            "Unhandled enum value during encode (underlying = " + std::to_string(U(value)) + ").");
    } else if constexpr (IsOptional<T>::value) {
        using U = typename T::value_type;
        if (!value) {
            dst = Json::nullValue;
        } else {
            encodeValue<Schema, U>(*value, dst, encoder);
        }
    } else if constexpr (IsVariant<T>::value) {
        // Discriminated polymorphic encoding for std::variant.
        validateVariant<Schema, T>();
        if (!VariantKeyValidator<Schema, EncoderImpl<Schema>, T>::validate(encoder)) {
            return;
        }
        dst = Json::objectValue;
        std::visit(
            [&](const auto& alt) {
                using Alt = std::decay_t<decltype(alt)>;

                const auto& objectDef = getSchemaObject<typename Schema::template Object<Alt>>();
                if (!objectDef.hasDiscriminatorTag()) {
                    PathScope guard(encoder, objectDef.discriminatorKey());
                    encoder.addError("Variant alternative missing discriminator().");
                    return;
                }

                // Encode discriminator using a string payload.
                Json::Value tagJson;
                const std::string tagValue(objectDef.discriminatorTag());
                encodeDefault<Schema, std::string>(tagValue, tagJson, encoder);

                // Encode variant-specific fields into the same object.
                objectDef.encodeFields(alt, dst, encoder);

                // Write discriminator field.
                dst[objectDef.discriminatorKey()] = std::move(tagJson);
            },
            value);
    } else if constexpr (IsVector<T>::value) {
        using U = typename T::value_type;
        dst = Json::arrayValue;
        std::size_t index = 0;
        for (const auto& elem : value) {
            PathScope guard(encoder, index++);
            Json::Value v;
            encodeValue<Schema, U>(elem, v, encoder);
            dst.append(v);
        }
    } else if constexpr (std::is_class_v<T>) {
        static_assert(
            HasObjectTag<Schema, T>::value,
            "Type is not mapped as an object. Either define "
            "`template<> struct Schema::Object<T> : aison::Object<Schema, T>` and call "
            "add(...) for its fields, or provide a custom encoder via Schema::Encoder<T>.");
        const auto& objectDef = getSchemaObject<typename Schema::template Object<T>>();
        objectDef.encodeFields(value, dst, encoder);
    } else if constexpr (std::is_pointer_v<T>) {
        static_assert(false && std::is_pointer_v<T>, "Pointers are not supported.");
    } else {
        static_assert(
            !HasEncoderTag<Schema, T>::value,
            "Unsupported type. Define a custom encoder as "
            "`template<> struct Schema::Encoder<T> : aison::Encoder<Schema, T>`.");
    }
}

// Decode defaults //////////////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
void decodeDefault(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder)
{
    if constexpr (std::is_same_v<T, bool>) {
        if (!src.isBool()) {
            decoder.addError("Expected bool.");
            return;
        }
        value = src.asBool();
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        if (!src.isIntegral()) {
            decoder.addError("Expected integral value.");
            return;
        }
        if constexpr (std::is_signed_v<T>) {
            auto v = src.asInt64();
            if (v < std::numeric_limits<T>::min() || v > std::numeric_limits<T>::max()) {
                decoder.addError("Integer value out of range.");
                return;
            }
            value = static_cast<T>(v);
        } else {
            auto v = src.asUInt64();
            if (v > std::numeric_limits<T>::max()) {
                decoder.addError("Unsigned integer value out of range.");
                return;
            }
            value = static_cast<T>(v);
        }
    } else if constexpr (std::is_same_v<T, float>) {
        if (!src.isDouble() && !src.isInt()) {
            decoder.addError("Expected float.");
            return;
        }
        value = static_cast<float>(src.asDouble());
    } else if constexpr (std::is_same_v<T, double>) {
        if (!src.isDouble() && !src.isInt()) {
            decoder.addError("Expected double.");
            return;
        }
        value = src.asDouble();
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!src.isString()) {
            decoder.addError("Expected string.");
            return;
        }
        value = src.asString();
    } else if constexpr (std::is_enum_v<T>) {
        validateEnumType<Schema, T>();

        if (!src.isString()) {
            decoder.addError("Expected string for enum.");
            return;
        }
        const std::string s = src.asString();
        using EnumSpec = typename Schema::template Enum<T>;
        const auto& entries = getSchemaObject<EnumSpec>();
        for (const auto& entry : entries) {
            if (s == entry.second) {
                value = entry.first;
                return;
            }
        }
        decoder.addError("Unknown enum value '" + s + "'.");
    } else if constexpr (IsOptional<T>::value) {
        using U = typename T::value_type;
        if (src.isNull()) {
            value.reset();
        } else {
            U tmp{};
            decodeValue<Schema, U>(src, tmp, decoder);
            value = std::move(tmp);
        }
    } else if constexpr (IsVariant<T>::value) {
        // Discriminated polymorphic decoding for std::variant.
        validateVariant<Schema, T>();
        VariantDecoder<Schema, T>::decode(src, value, decoder);
    } else if constexpr (IsVector<T>::value) {
        using U = typename T::value_type;
        value.clear();
        if (!src.isArray()) {
            decoder.addError("Expected array.");
            return;
        }
        for (Json::ArrayIndex i = 0; i < src.size(); ++i) {
            PathScope guard(decoder, i);
            U elem{};
            decodeValue<Schema, U>(src[i], elem, decoder);
            value.push_back(std::move(elem));
        }
    } else if constexpr (std::is_class_v<T>) {
        static_assert(
            HasObjectTag<Schema, T>::value,
            "Type is not mapped as an object. Either define "
            "`template<> struct Schema::Object<T> : aison::Object<Schema, T>` and call "
            "add(...) for its fields, or provide a custom decoder via Schema::Decoder<T>.");
        if (!src.isObject()) {
            decoder.addError("Expected object.");
            return;
        }

        const auto& objectDef = getSchemaObject<typename Schema::template Object<T>>();
        objectDef.decodeFields(src, value, decoder);
    } else if constexpr (std::is_pointer_v<T>) {
        static_assert(false && std::is_pointer_v<T>, "Pointers are not supported.");
    } else {
        static_assert(
            !HasDecoderTag<Schema, T>::value,
            "Unsupported type. Define a custom decoder as "
            "`template<> struct Schema::Decoder<T> : aison::Decoder<Schema, T>`.");
    }
}

// Variant decoding /////////////////////////////////////////////////////////////////////////

// Specialization for std::variant<Ts...>
template<typename Schema, typename... Ts>
struct VariantDecoder<Schema, std::variant<Ts...>, void> {
    using VariantType = std::variant<Ts...>;

    static void decode(const Json::Value& src, VariantType& value, DecoderImpl<Schema>& decoder)
    {
        // Ensure src is an object
        if (!src.isObject()) {
            decoder.addError("Expected object for discriminated variant.");
            return;
        }

        using FirstAlt = std::tuple_element_t<0, std::tuple<Ts...>>;
        const auto& firstObj = getSchemaObject<typename Schema::template Object<FirstAlt>>();
        const auto& fieldName = firstObj.discriminatorKey();
        if (!VariantKeyValidator<Schema, DecoderImpl<Schema>, std::variant<Ts...>>::validate(
                decoder))
        {
            return;
        }

        std::string tagValue;
        {
            PathScope discGuard(decoder, fieldName);
            if (!src.isMember(fieldName)) {
                decoder.addError("Missing discriminator field.");
                return;
            }

            const Json::Value& tagNode = src[fieldName];
            if (!tagNode.isString()) {
                decoder.addError("Expected string.");
                return;
            }
            tagValue = tagNode.asString();
        }

        bool matched = false;

        // Try each alternative in turn
        (tryAlternative<Ts>(src, tagValue, value, decoder, matched), ...);

        if (!matched) {
            PathScope discGuard(decoder, fieldName);
            decoder.addError("Unknown discriminator value for variant.");
        }
    }

private:
    template<typename Alt>
    static void tryAlternative(
        const Json::Value& src,
        const std::string& tagValue,
        VariantType& value,
        DecoderImpl<Schema>& decoder,
        bool& matched)
    {
        using ObjectSpec = typename Schema::template Object<Alt>;
        const auto& objectDef = getSchemaObject<ObjectSpec>();
        if (!objectDef.hasDiscriminatorTag()) {
            PathScope guard(decoder, objectDef.discriminatorKey());
            decoder.addError("Variant alternative missing discriminator().");
            return;
        }
        if (matched || tagValue != objectDef.discriminatorTag()) {
            return;
        }

        matched = true;

        Alt alt{};
        objectDef.decodeFields(src, alt, decoder);
        value = std::move(alt);
    }
};

// Field descriptor /////////////////////////////////////////////////////////////

template<typename Schema, typename Owner>
struct FieldDesc {
    using EncodeFn =
        void (*)(const Owner&, Json::Value&, EncoderImpl<Schema>&, const void* context);
    using DecodeFn =
        void (*)(const Json::Value&, Owner&, DecoderImpl<Schema>&, const void* context);
    EncodeFn encode = nullptr;
    DecodeFn decode = nullptr;
    std::string name;
    const void* context = nullptr;
    const void* contextId = nullptr;
    const TypeInfo* typeInfo = nullptr;
    bool isOptional = false;
};

template<typename Owner, typename T>
struct FieldContext {
    T Owner::* member;
};

template<typename Schema, typename Owner, typename T>
void encodeFieldThunk(
    const Owner& owner, Json::Value& dst, EncoderImpl<Schema>& encoder, const void* context)
{
    using Ctx = FieldContext<Owner, T>;
    auto* ctx = static_cast<const Ctx*>(context);
    auto& member = ctx->member;
    const T& ref = owner.*member;
    encodeValue<Schema, T>(ref, dst, encoder);
}

template<typename Schema, typename Owner, typename T>
void decodeFieldThunk(
    const Json::Value& src, Owner& owner, DecoderImpl<Schema>& decoder, const void* context)
{
    using Ctx = FieldContext<Owner, T>;
    auto* ctx = static_cast<const Ctx*>(context);
    auto& member = ctx->member;
    T& ref = owner.*member;
    decodeValue<Schema, T>(src, ref, decoder);
}

// Object implementations per facet /////////////////////////////////////////////////////////

template<typename Owner, typename T>
FieldContextPtr makeFieldContext(T Owner::* member)
{
    using Ctx = FieldContext<Owner, T>;
    auto* ctx = new Ctx{member};
    auto deleter = +[](void* p) { delete static_cast<Ctx*>(p); };
    return {ctx, deleter};
}

template<typename Owner, typename T>
const void* getFieldContextId()
{
    static int fieldId = 0xf1e1d1d;
    return &fieldId;
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
public:
    using Field = FieldDesc<Schema, Owner>;

    template<typename T>
    void add(T Owner::* member, std::string_view name)
    {
        // Check if name is used as discriminator key
        if (!discriminatorKey_.empty() && discriminatorKey_ == name) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Field name is reserved as discriminator for this type.");
            }
            return;
        }

        // Check if member or name is already mapped
        using Ctx = FieldContext<Owner, T>;
        const auto* contextId = getFieldContextId<Owner, T>();
        for (const auto& field : fields_) {
            if (field.contextId == contextId &&
                static_cast<const Ctx*>(field.context)->member == member)
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

        auto& context = contexts_.emplace_back(makeFieldContext(member));
        auto& field = fields_.emplace_back();

        field.name = std::string(name);
        field.context = context.get();
        field.contextId = contextId;
        field.typeInfo = &makeTypeInfo<Schema, T>();
        field.isOptional = IsOptional<T>::value;

        if constexpr (hasEncodeFacet<Schema>()) {
            field.encode = &encodeFieldThunk<Schema, Owner, T>;
        }

        if constexpr (hasDecodeFacet<Schema>()) {
            field.decode = &decodeFieldThunk<Schema, Owner, T>;
        }

        if constexpr (detail::getSchemaEnableIntrospection<Schema>()) {
            FieldInfo fi;
            fi.name = std::string(name);
            fi.type = field.typeInfo;
            introspectionRegistry<Schema>().addObjectField(detail::typeId<Owner>(), std::move(fi));
            detail::ensureTypeRegistration<Schema, Owner, T>();
            if (hasDiscriminatorTag_) {
                introspectionRegistry<Schema>().setObjectDiscriminator(
                    detail::typeId<Owner>(), discriminatorKey_, discriminatorTag_);
            }
        }
    }

    void discriminator(std::string_view tag, std::string_view key)
    {
        if (key.empty()) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "Discriminator key cannot be empty.");
            }
            return;
        }
        if (!checkDiscriminatorKey(key)) {
            return;
        }
        if (hasDiscriminatorTag_) {
            if constexpr (getSchemaEnableAssert<Schema>()) {
                assert(false && "discriminator(...) already set for this object.");
            }
            return;
        }

        hasDiscriminatorTag_ = true;
        discriminatorKey_ = std::string(key);
        discriminatorTag_ = std::string(tag);
        if constexpr (detail::getSchemaEnableIntrospection<Schema>()) {
            introspectionRegistry<Schema>().setObjectDiscriminator(
                detail::typeId<Owner>(), discriminatorKey_, discriminatorTag_);
        }
    }

    template<
        typename S = Schema,
        typename =
            std::enable_if_t<HasSchemaDiscriminatorKey<S>::value && std::is_same_v<S, Schema>>>
    void discriminator(std::string_view tag)
    {
        discriminator(tag, getSchemaDiscriminatorKey<Schema>());
    }

    template<typename S = Schema, typename = std::enable_if_t<hasEncodeFacet<S>()>>
    void encodeFields(const Owner& src, Json::Value& dst, EncoderImpl<Schema>& encoder) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            PathScope guard(encoder, field.name);
            Json::Value node;
            field.encode(src, node, encoder, field.context);
            if (!getSchemaStrictOptional<Schema>() && field.isOptional && node.isNull()) {
                continue;
            }
            dst[field.name] = std::move(node);
        }
    }

    template<typename S = Schema, typename = std::enable_if_t<hasDecodeFacet<S>()>>
    void decodeFields(const Json::Value& src, Owner& dst, DecoderImpl<Schema>& decoder) const
    {
        for (const auto& field : fields_) {
            const auto& key = field.name;
            if (!src.isMember(key)) {
                if (!getSchemaStrictOptional<Schema>() && field.isOptional) {
                    PathScope guard(decoder, key);
                    field.decode(Json::nullValue, dst, decoder, field.context);
                    continue;
                }
                decoder.addError(std::string("Missing required field '") + key + "'.");
                continue;
            }
            const Json::Value& node = src[key];
            PathScope guard(decoder, key);
            field.decode(node, dst, decoder, field.context);
        }
    }

    const std::string& discriminatorTag() const { return discriminatorTag_; }
    const std::string& discriminatorKey() const { return discriminatorKey_; }
    bool hasDiscriminatorTag() const { return hasDiscriminatorTag_; }
    const std::vector<Field>& fields() const { return fields_; }

private:
    bool checkDiscriminatorKey(std::string_view key)
    {
        for (const auto& field : fields_) {
            if (field.name == key) {
                if constexpr (getSchemaEnableAssert<Schema>()) {
                    assert(false && "Discriminator key conflicts with an existing field name.");
                }
                return false;
            }
        }
        return true;
    }

    std::vector<FieldContextPtr> contexts_;
    std::vector<Field> fields_;
    std::string discriminatorTag_;
    std::string discriminatorKey_ = std::string(getSchemaDiscriminatorKey<Schema>());
    bool hasDiscriminatorTag_ = false;
};

}  // namespace aison::detail

namespace aison {

// Object / Enum wrappers ///////////////////////////////////////////////////////////////////

template<typename Schema, typename Owner>
struct Object : detail::ObjectImpl<Schema, Owner> {
    using ObjectTag = void;
    using Base = detail::ObjectImpl<Schema, Owner>;

    using Base::add;
    using Base::discriminator;
};

template<typename Schema, typename E>
struct Enum : detail::EnumImpl<Schema, E> {
    using EnumTag = void;
    using Base = detail::EnumImpl<Schema, E>;

    using Base::add;
};

// Introspection access (enabled only when Schema::enableIntrospection == true) /////////////////

template<typename Schema, typename Enable = void>
struct IntrospectionView {};

template<typename Schema>
struct IntrospectionView<Schema, std::enable_if_t<detail::getSchemaEnableIntrospection<Schema>()>> {
    const std::unordered_map<const void*, detail::ObjectInfo>& objects;
    const std::unordered_map<const void*, detail::EnumInfo>& enums;
};

template<
    typename Schema,
    typename Enable = std::enable_if_t<detail::getSchemaEnableIntrospection<Schema>()>>
inline IntrospectionView<Schema> introspect()
{
    const auto& reg = detail::introspectionRegistry<Schema>();
    return IntrospectionView<Schema>{reg.objects(), reg.enums()};
}

template<
    typename Schema,
    typename Root,
    typename Enable = std::enable_if_t<detail::getSchemaEnableIntrospection<Schema>()>>
inline IntrospectionView<Schema> introspect()
{
    static_assert(
        detail::HasObjectTag<Schema, Root>::value,
        "Root type is not mapped as an object in this schema.");
    detail::registerObjectMapping<Schema, Root>();
    const auto& reg = detail::introspectionRegistry<Schema>();
    return IntrospectionView<Schema>{reg.objects(), reg.enums()};
}

// Introspection builder that collects only reachable types from chosen roots.
template<
    typename Schema,
    typename Enable = std::enable_if_t<detail::getSchemaEnableIntrospection<Schema>()>>
class Introspection
{
public:
    template<typename T>
    void add()
    {
        using U = std::decay_t<T>;
        if constexpr (std::is_enum_v<U>) {
            detail::registerEnumMapping<Schema, U>();
            collectEnum(detail::typeId<U>());
        } else if constexpr (detail::IsOptional<U>::value) {
            add<typename U::value_type>();
        } else if constexpr (detail::IsVector<U>::value) {
            add<typename U::value_type>();
        } else if constexpr (detail::IsVariant<U>::value) {
            detail::ensureVariantAlternatives<Schema, U, U>(
                std::make_index_sequence<std::variant_size_v<U>>{});
            addVariantAlternatives<U>(std::make_index_sequence<std::variant_size_v<U>>{});
        } else {
            static_assert(
                detail::HasObjectTag<Schema, U>::value,
                "add<T>() expects an enum, object mapping, or supported container of those.");
            detail::registerObjectMapping<Schema, U>();
            collectObject(detail::typeId<U>());
        }
    }

    const auto& objects() const { return objects_; }
    const auto& enums() const { return enums_; }

private:
    void collectEnum(const void* typeId)
    {
        if (enums_.count(typeId)) {
            return;
        }
        const auto& reg = detail::introspectionRegistry<Schema>();
        auto it = reg.enums().find(typeId);
        if (it == reg.enums().end()) {
            return;
        }
        enums_.emplace(it->first, it->second);
    }

    void collectObject(const void* typeId)
    {
        if (objects_.count(typeId)) {
            return;
        }
        const auto& reg = detail::introspectionRegistry<Schema>();
        auto it = reg.objects().find(typeId);
        if (it == reg.objects().end()) {
            return;
        }

        objects_.emplace(it->first, it->second);
        const auto& objInfo = objects_.find(typeId)->second;

        for (const auto& field : objInfo.fields) {
            traverseType(field.type);
        }
    }

    void traverseType(const detail::TypeInfo* info)
    {
        if (!info) return;
        using namespace detail;
        switch (info->kind) {
            case FieldKind::Plain:
                if (info->basic == BasicType::Object && info->typeId) {
                    collectObject(info->typeId);
                } else if (info->basic == BasicType::Enum && info->typeId) {
                    collectEnum(info->typeId);
                }
                break;
            case FieldKind::Optional:
            case FieldKind::Vector:
                traverseType(info->element);
                break;
            case FieldKind::Variant:
                for (std::size_t i = 0; i < info->variantCount; ++i) {
                    traverseType(info->variants ? info->variants[i] : nullptr);
                }
                break;
        }
    }

    template<typename Variant, std::size_t... Is>
    void addVariantAlternatives(std::index_sequence<Is...>)
    {
        (add<std::variant_alternative_t<Is, Variant>>(), ...);
    }

    std::unordered_map<const void*, detail::ObjectInfo> objects_;
    std::unordered_map<const void*, detail::EnumInfo> enums_;
};

template<
    typename Schema,
    typename Enable = std::enable_if_t<detail::getSchemaEnableIntrospection<Schema>()>>
inline Introspection<Schema> introspection()
{
    return Introspection<Schema>{};
}

/// Encoder / Decoder bases (with setEncoder / setDecoder) ///////////////////////

template<typename Schema, typename T>
struct Encoder {
public:
    using EncoderTag = void;
    using EncoderType = aison::detail::EncoderImpl<Schema>;

    Encoder() = default;
    void setEncoder(EncoderType& enc) { encoder_ = &enc; }

    void addError(const std::string& msg) { encoder_->addError(msg); }
    const auto& config() const { return encoder_->config; }

    template<typename U>
    void encode(const U& src, Json::Value& dst)
    {
        detail::encodeValue(src, dst, *encoder_);
    }

private:
    EncoderType* encoder_ = nullptr;
};

template<typename Schema, typename T>
struct Decoder {
public:
    using DecoderTag = void;
    using DecoderType = aison::detail::DecoderImpl<Schema>;

    Decoder() = default;

    void setDecoder(DecoderType& dec) { decoder_ = &dec; }
    void addError(const std::string& msg) { decoder_->addError(msg); }
    const auto& config() const { return decoder_->config; }

    template<typename U>
    void decode(const Json::Value& src, U& dst)
    {
        detail::decodeValue(src, dst, *decoder_);
    }

private:
    DecoderType* decoder_ = nullptr;
};

// Free encode/decode helpers ///////////////////////////////////////////////////////////////

template<typename Schema, typename T>
Result encode(const T& value, Json::Value& dst)
{
    using Config = typename Schema::ConfigType;
    if constexpr (!std::is_same_v<Config, EmptyConfig>) {
        static_assert(
            std::is_same_v<Config, EmptyConfig>,
            "Schema was declared with a non-empty ConfigType. "
            "Use aison::encode<Schema>(value, dst, config) instead.");
    } else {
        Config cfg{};
        return detail::EncoderImpl<Schema>(cfg).encode(value, dst);
    }
}

template<typename Schema, typename T>
Result decode(const Json::Value& src, T& value)
{
    using Config = typename Schema::ConfigType;
    if constexpr (!std::is_same_v<Config, EmptyConfig>) {
        static_assert(
            std::is_same_v<Config, EmptyConfig>,
            "Schema was declared with a non-empty ConfigType. "
            "Use aison::decode<Schema>(src, value, config) instead.");
    } else {
        Config cfg{};
        return detail::DecoderImpl<Schema>(cfg).decode(src, value);
    }
}

template<typename Schema, typename T>
Result encode(const T& value, Json::Value& dst, const typename Schema::ConfigType& config)
{
    return detail::EncoderImpl<Schema>(config).encode(value, dst);
}

template<typename Schema, typename T>
Result decode(const Json::Value& src, T& value, const typename Schema::ConfigType& config)
{
    return detail::DecoderImpl<Schema>(config).decode(src, value);
}

}  // namespace aison
