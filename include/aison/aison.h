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
using DiscriminatorType = std::string_view;

template<typename Schema>
class EncoderImpl;

template<typename Schema>
class DecoderImpl;

template<typename Schema, typename T>
class EnumImpl;

template<typename Schema, typename Owner, typename FacetTag>
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

// Functions
template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder);

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder);

template<typename Schema, typename T>
void encodeDefault(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder);

template<typename Schema, typename T>
void decodeDefault(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder);

template<typename Schema, typename T>
constexpr void validateEnumType();

template<typename Schema, typename Variant>
constexpr void validateVariant();

template<typename Owner, typename T>
const void* getFieldContextId();

template<typename Owner, typename T>
FieldContextPtr makeFieldContext(T Owner::* member);

template<typename Schema, typename Owner, typename T, typename Field>
bool checkAdd(T Owner::* member, std::string_view name, const std::vector<Field>& fields);

template<typename Schema>
std::string_view getDiscriminatorKey();

template<typename T>
T& getSchemaObject();

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

// CRTP schema base: facet + config are template parameters

template<typename Derived, typename Facet = EncodeDecode, typename Config = EmptyConfig>
struct Schema {
    using SchemaTag = void;
    using FacetType = Facet;
    using ConfigType = Config;
    using EnableAssert = std::true_type;

    // Default discriminator field name for variant-based polymorphism.
    // User schemas may override this with their own static constexpr member.
    static constexpr const char* discriminatorKey = "type";

    // template<typename T> struct Object;
    // template<typename T> struct Enum;
    // template<typename T> struct Encoder;
    // template<typename T> struct Decoder;
};

}  // namespace aison

namespace aison::detail {

// Path tracking /////////////////////////////////////////////////////////////////////////////

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
    using Entry = std::pair<E, std::string_view>;
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
                if constexpr (Schema::EnableAssert::value) {
                    assert(false && "Duplicate enum mapping in Schema::Enum.");
                }
                return;
            }
        }
        entries_.emplace_back(value, name);
    }

    void addAlias(E value, std::string_view name)
    {
        bool isDefined = false;
        for (const auto& entry : entries_) {
            if (entry.first == value) {
                isDefined = true;
            }
            if (entry.second == name) {
                if constexpr (Schema::EnableAssert::value) {
                    assert(false && "Duplicate enum name in Schema::Enum::addAlias.");
                }
                return;
            }
        }

        if (!isDefined) {
            if constexpr (Schema::EnableAssert::value) {
                assert(
                    false &&
                    "Alias refers to an enum value that was not added with Schema::Enum::add.");
            }
        }

        entries_.emplace_back(value, name);
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

template<typename Schema>
std::string_view getDiscriminatorKey()
{
    using KeyType = std::decay_t<decltype(Schema::discriminatorKey)>;

    static_assert(
        std::is_same_v<KeyType, const char*> || std::is_same_v<KeyType, std::string_view>,
        "Schema::discriminatorKey must be const either `char*` or `std::string_view`.");

    return std::string_view(Schema::discriminatorKey);
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
        dst = Json::objectValue;

        std::visit(
            [&](const auto& alt) {
                using Alt = std::decay_t<decltype(alt)>;

                const auto& objectDef = getSchemaObject<typename Schema::template Object<Alt>>();
                if (!objectDef.hasRuntimeDiscriminator()) {
                    PathScope guard(encoder, getDiscriminatorKey<Schema>().data());
                    encoder.addError("Variant alternative missing discriminator().");
                    return;
                }

                // Encode discriminator using a string payload.
                Json::Value tagJson;
                const std::string tagValue(objectDef.runtimeDiscriminator());
                encodeDefault<Schema, std::string>(tagValue, tagJson, encoder);

                // Encode variant-specific fields into the same object.
                objectDef.encodeFields(alt, dst, encoder);

                // Write discriminator field.
                dst[getDiscriminatorKey<Schema>().data()] = std::move(tagJson);
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

        auto* fieldName = getDiscriminatorKey<Schema>().data();

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
            detail::PathScope discGuard(decoder, fieldName);
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
        if (!objectDef.hasRuntimeDiscriminator()) {
            PathScope guard(decoder, getDiscriminatorKey<Schema>().data());
            decoder.addError("Variant alternative missing discriminator().");
            return;
        }
        if (matched || tagValue != objectDef.runtimeDiscriminator()) {
            return;
        }

        matched = true;

        Alt alt{};
        objectDef.decodeFields(src, alt, decoder);
        value = std::move(alt);
    }
};

// Field descriptors ///////////////////////////////////////////////////////////

// Encode-only descriptor
template<typename Schema, typename Owner>
struct EncodeOnlyFieldDesc {
    using EncodeFn =
        void (*)(const Owner&, Json::Value&, EncoderImpl<Schema>&, const void* context);

    EncodeFn encode;
    const char* name;
    const void* context;
    const void* contextId;
};

// Decode-only descriptor
template<typename Schema, typename Owner>
struct DecodeOnlyFieldDesc {
    using DecodeFn =
        void (*)(const Json::Value&, Owner&, DecoderImpl<Schema>&, const void* context);

    DecodeFn decode;
    const char* name;
    const void* context;
    const void* contextId;
};

// Encode+Decode descriptor
template<typename Schema, typename Owner>
struct EncodeDecodeFieldDesc {
    using EncodeFn =
        void (*)(const Owner&, Json::Value&, EncoderImpl<Schema>&, const void* context);
    using DecodeFn =
        void (*)(const Json::Value&, Owner&, DecoderImpl<Schema>&, const void* context);

    EncodeFn encode;
    DecodeFn decode;
    const char* name;
    const void* context;
    const void* contextId;
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

template<typename Schema, typename Owner, typename T, typename Field>
bool checkAdd(T Owner::* member, std::string_view name, const std::vector<Field>& fields)
{
    auto discName = getDiscriminatorKey<Schema>();
    if (discName == name) {
        if constexpr (Schema::EnableAssert::value) {
            assert(false && "Field name is reserved as discriminator for this type.");
        }
        return false;
    }

    using Ctx = FieldContext<Owner, T>;
    const auto* contextId = getFieldContextId<Owner, T>();

    for (const auto& field : fields) {
        if (field.contextId == contextId &&
            static_cast<const Ctx*>(field.context)->member == member)
        {
            if constexpr (Schema::EnableAssert::value) {
                assert(false && "Same member is mapped multiple times in Schema::Object.");
            }
            return false;
        }

        if (field.name == name) {
            if constexpr (Schema::EnableAssert::value) {
                assert(false && "Duplicate field name in Schema::Object.");
            }
            return false;
        }
    }

    return true;
}

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

// Encode-only
template<typename Schema, typename Owner>
class ObjectImpl<Schema, Owner, EncodeOnly>
{
    using EncoderType = EncoderImpl<Schema>;
    using Field = EncodeOnlyFieldDesc<Schema, Owner>;
    std::vector<FieldContextPtr> contexts_;
    std::vector<Field> fields_;
    bool hasRuntimeDiscriminator_ = false;
    std::string runtimeDiscriminator_;

public:
    template<typename T>
    void add(T Owner::* member, const char* name)
    {
        if (checkAdd<Schema>(member, name, fields_)) {
            auto& ctx = contexts_.emplace_back(makeFieldContext(member));

            Field f;
            f.name = name;
            f.encode = &encodeFieldThunk<Schema, Owner, T>;
            f.context = ctx.get();
            f.contextId = getFieldContextId<Owner, T>();

            fields_.push_back(f);
        }
    }

    void discriminator(detail::DiscriminatorType tag)
    {
        auto discKey = getDiscriminatorKey<Schema>();
        for (const auto& field : fields_) {
            if (discKey == field.name) {
                if constexpr (Schema::EnableAssert::value) {
                    assert(false && "Discriminator key conflicts with an existing field name.");
                }
                return;
            }
        }
        if (hasRuntimeDiscriminator_) {
            if constexpr (Schema::EnableAssert::value) {
                assert(false && "discriminator(...) already set for this object.");
            }
            return;
        }
        hasRuntimeDiscriminator_ = true;
        runtimeDiscriminator_ = std::string(tag);
    }

    bool hasRuntimeDiscriminator() const { return hasRuntimeDiscriminator_; }
    std::string_view runtimeDiscriminator() const { return runtimeDiscriminator_; }

    void encodeFields(const Owner& src, Json::Value& dst, EncoderType& encoder) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            PathScope guard(encoder, field.name);
            Json::Value& node = dst[field.name];
            field.encode(src, node, encoder, field.context);
        }
    }
};

// Decode-only
template<typename Schema, typename Owner>
class ObjectImpl<Schema, Owner, DecodeOnly>
{
    using DecoderType = DecoderImpl<Schema>;
    using Field = DecodeOnlyFieldDesc<Schema, Owner>;
    std::vector<FieldContextPtr> contexts_;
    std::vector<Field> fields_;
    bool hasRuntimeDiscriminator_ = false;
    std::string runtimeDiscriminator_;

public:
    template<typename T>
    void add(T Owner::* member, const char* name)
    {
        if (checkAdd<Schema>(member, name, fields_)) {
            auto& ctx = contexts_.emplace_back(makeFieldContext(member));

            Field f;
            f.name = name;
            f.decode = &decodeFieldThunk<Schema, Owner, T>;
            f.context = ctx.get();
            f.contextId = getFieldContextId<Owner, T>();

            fields_.push_back(f);
        }
    }

    void discriminator(detail::DiscriminatorType tag)
    {
        auto discKey = getDiscriminatorKey<Schema>();
        for (const auto& field : fields_) {
            if (discKey == field.name) {
                if constexpr (Schema::EnableAssert::value) {
                    assert(false && "Discriminator key conflicts with an existing field name.");
                }
                return;
            }
        }
        if (hasRuntimeDiscriminator_) {
            if constexpr (Schema::EnableAssert::value) {
                assert(false && "discriminator(...) already set for this object.");
            }
            return;
        }
        hasRuntimeDiscriminator_ = true;
        runtimeDiscriminator_ = std::string(tag);
    }

    bool hasRuntimeDiscriminator() const { return hasRuntimeDiscriminator_; }
    std::string_view runtimeDiscriminator() const { return runtimeDiscriminator_; }

    void decodeFields(const Json::Value& src, Owner& dst, DecoderType& decoder) const
    {
        for (const auto& field : fields_) {
            const char* key = field.name;
            if (!src.isMember(key)) {
                decoder.addError(std::string("Missing required field '") + key + "'.");
                continue;
            }
            const Json::Value& node = src[key];
            PathScope guard(decoder, key);
            field.decode(node, dst, decoder, field.context);
        }
    }
};

// Encode+Decode
template<typename Schema, typename Owner>
class ObjectImpl<Schema, Owner, EncodeDecode>
{
    using EncoderType = EncoderImpl<Schema>;
    using DecoderType = DecoderImpl<Schema>;
    using Field = EncodeDecodeFieldDesc<Schema, Owner>;
    std::vector<FieldContextPtr> contexts_;
    std::vector<Field> fields_;
    bool hasRuntimeDiscriminator_ = false;
    std::string runtimeDiscriminator_;

public:
    template<typename T>
    void add(T Owner::* member, const char* name)
    {
        if (checkAdd<Schema>(member, name, fields_)) {
            auto& ctx = contexts_.emplace_back(makeFieldContext(member));

            Field f;
            f.name = name;
            f.encode = &encodeFieldThunk<Schema, Owner, T>;
            f.decode = &decodeFieldThunk<Schema, Owner, T>;
            f.context = ctx.get();
            f.contextId = getFieldContextId<Owner, T>();

            fields_.push_back(f);
        }
    }

    void discriminator(detail::DiscriminatorType tag)
    {
        auto discKey = getDiscriminatorKey<Schema>();
        for (const auto& field : fields_) {
            if (discKey == field.name) {
                if constexpr (Schema::EnableAssert::value) {
                    assert(false && "Discriminator key conflicts with an existing field name.");
                }
                return;
            }
        }
        if (hasRuntimeDiscriminator_) {
            if constexpr (Schema::EnableAssert::value) {
                assert(false && "discriminator(...) already set for this object.");
            }
            return;
        }
        hasRuntimeDiscriminator_ = true;
        runtimeDiscriminator_ = std::string(tag);
    }

    bool hasRuntimeDiscriminator() const { return hasRuntimeDiscriminator_; }
    std::string_view runtimeDiscriminator() const { return runtimeDiscriminator_; }

    void encodeFields(const Owner& src, Json::Value& dst, EncoderType& encoder) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            PathScope guard(encoder, field.name);
            Json::Value& node = dst[field.name];
            field.encode(src, node, encoder, field.context);
        }
    }

    void decodeFields(const Json::Value& src, Owner& dst, DecoderType& decoder) const
    {
        for (const auto& field : fields_) {
            const char* key = field.name;
            if (!src.isMember(key)) {
                decoder.addError(std::string("Missing required field '") + key + "'.");
                continue;
            }
            const Json::Value& node = src[key];
            PathScope guard(decoder, key);
            field.decode(node, dst, decoder, field.context);
        }
    }
};

}  // namespace aison::detail

namespace aison {

// Object / Enum wrappers ///////////////////////////////////////////////////////////////////

template<typename Schema, typename Owner>
struct Object : detail::ObjectImpl<Schema, Owner, typename Schema::FacetType> {
    using ObjectTag = void;
    using Base = detail::ObjectImpl<Schema, Owner, typename Schema::FacetType>;
    using Base::add;
};

template<typename Schema, typename E>
struct Enum : detail::EnumImpl<Schema, E> {
    using EnumTag = void;
    using Base = detail::EnumImpl<Schema, E>;
    using Base::add;
    using Base::addAlias;
};

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
    EncoderType& getEncoder() { return *encoder_; }

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
    DecoderType& getDecoder() { return *decoder_; }

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
