#pragma once

#include <json/json.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace aison {

// Forward declarations ////////////////////////////////////////////////////////////////////////////

enum class Facet;

namespace detail {

class Context;
struct PathSegment;
struct PathScope;
struct EnumBase;

template<typename Schema, typename T>
struct SchemaConfig;

template<typename Schema, typename T>
struct SchemaFacet;

template<typename Schema>
class EncoderImpl;

template<typename Schema>
class DecoderImpl;

template<typename Schema, typename T>
class EnumImpl;

template<typename Schema, typename Owner, Facet F>
class ObjectImpl;

template<typename Owner, typename Schema>
struct IEncodeOnlyField;

template<typename Owner, typename Schema>
struct IDecodeOnlyField;

template<typename Owner, typename Schema>
struct IEncodeDecodeField;

template<typename Owner, typename T, typename Schema>
struct EncodeOnlyField;

template<typename Owner, typename T, typename Schema>
struct DecodeOnlyField;

template<typename Owner, typename T, typename Schema>
struct EncodeDecodeField;

template<typename T>
struct IsOptional;

template<typename T>
struct IsVector;

template<typename Schema, typename T, typename U>
struct HasEnumTag;

template<typename Schema, typename T, typename U>
struct HasObjectTag;

template<typename Schema, typename T, typename U>
struct HasEncoderTag;

template<typename Schema, typename T, typename U>
struct HasDecoderTag;

template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder);

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder);

template<typename Schema, typename T>
void encodeValueDefault(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder);

template<typename Schema, typename T>
void decodeValueDefault(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder);

template<typename Schema>
constexpr void validateConfig();

template<typename Schema, typename T>
constexpr void validateEnumType();

template<typename T>
T& getSchemaObject();

}  // namespace detail

struct Error;
struct Result;
struct Config;

template<typename Schema, typename T>
struct Object;

template<typename Schema, typename T>
struct Enum;

template<typename Derived, Facet F>
struct Schema;

template<typename Schema, typename T>
struct Encoder;

template<typename Schema, typename T>
struct Decoder;

// Implementation //////////////////////////////////////////////////////////////////////////////////

enum class Facet { EncodeOnly, DecodeOnly, EncodeDecode };

struct Error {
    std::string path;
    std::string message;
};

struct Result {
    std::vector<Error> errors;

    explicit operator bool() const { return errors.empty(); }
};

struct Config {
    using ConfigTag = void;
};

using EmptyConfig = aison::Config;

template<typename Derived, Facet F = Facet::EncodeDecode>
struct Schema {
    using SchemaTag = void;
    static constexpr Facet facet = F;

    template<typename T>
    struct Object;

    template<typename E>
    struct Enum;

    template<typename T>
    struct CustomEncoder;

    template<typename T>
    struct CustomDecoder;
};

namespace detail {

// Path tracking

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

class Context {
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

// Config detection

template<typename Schema, typename = void>
struct SchemaConfig {
    using type = EmptyConfig;
    static constexpr bool isEmpty = true;
};

template<typename Schema>
struct SchemaConfig<Schema, std::void_t<typename Schema::Config>> {
    using type = typename Schema::Config;
    static constexpr bool isEmpty = false;
};

template<typename Schema>
constexpr void validateConfig()
{
    using C = typename SchemaConfig<Schema>::type;
    if constexpr (!std::is_same_v<EmptyConfig, C>) {
        static_assert(
            std::is_base_of_v<aison::Config, C>,
            "Schema::Config (when defined) must derive from aison::Config");
    }
}

// Facet detection (fallback to EncodeDecode if not present)
template<typename Schema, typename = void>
struct SchemaFacet {
    static constexpr Facet value = Facet::EncodeDecode;
};

template<typename Schema>
struct SchemaFacet<Schema, std::void_t<decltype(Schema::facet)>> {
    static constexpr Facet value = Schema::facet;
};

// Traits

template<typename T>
struct IsOptional : std::false_type {};

template<typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template<typename T>
struct IsVector : std::false_type {};

template<typename T, typename A>
struct IsVector<std::vector<T, A>> : std::true_type {};

template<typename Schema, typename T, typename = void>
struct HasEnumTag : std::false_type {};

template<typename Schema, typename T>
struct HasEnumTag<Schema, T, std::void_t<typename Schema::template Enum<T>::EnumTag>>
    : std::true_type {};

template<typename Schema, typename T, typename = void>
struct HasObjectTag : std::false_type {};

template<typename Schema, typename T>
struct HasObjectTag<Schema, T, std::void_t<typename Schema::template Object<T>::ObjectTag>>
    : std::true_type {};

template<typename Schema, typename T, typename = void>
struct HasEncodeValue : std::false_type {};

template<typename Schema, typename T>
struct HasEncodeValue<
    Schema, T,
    std::void_t<decltype(Schema::encodeValue(
        std::declval<const T&>(), std::declval<Json::Value&>(),
        std::declval<EncoderImpl<Schema>&>()))>> : std::true_type {};

template<typename Schema, typename T, typename = void>
struct HasDecodeValue : std::false_type {};

template<typename Schema, typename T>
struct HasDecodeValue<
    Schema, T,
    std::void_t<decltype(Schema::decodeValue(
        std::declval<const Json::Value&>(), std::declval<T&>(),
        std::declval<DecoderImpl<Schema>&>()))>> : std::true_type {};

template<typename Schema, typename T, typename = void>
struct HasEncoderTag : std::false_type {};

template<typename Schema, typename T>
struct HasEncoderTag<
    Schema, T, std::void_t<typename Schema::template CustomEncoder<T>::CustomEncoderTag>>
    : std::true_type {};

template<typename Schema, typename T, typename = void>
struct HasDecoderTag : std::false_type {};

template<typename Schema, typename T>
struct HasDecoderTag<
    Schema, T, std::void_t<typename Schema::template CustomDecoder<T>::CustomDecoderTag>>
    : std::true_type {};

// Enum impl + validation

struct EnumBase {};

template<typename Schema, typename E>
class EnumImpl : public EnumBase {
    using Entry = std::pair<E, std::string_view>;
    std::vector<Entry> entries_;

public:
    std::size_t size() const { return entries_.size(); }

    auto begin() { return entries_.begin(); }
    auto end() { return entries_.end(); }
    auto begin() const { return entries_.begin(); }
    auto end() const { return entries_.end(); }

    void add(E value, std::string_view name) { entries_.emplace_back(value, name); }
};

template<typename Schema, typename T>
constexpr void validateEnumType()
{
    static_assert(
        HasEnumTag<Schema, T>::value, "Missing Schema::Enum<T> definition for this enum type.");

    using EnumDef = typename Schema::template Enum<T>;
    static_assert(
        std::is_base_of_v<EnumBase, EnumDef>,
        "Schema::Enum<T> must inherit from aison::Enum<Schema, T>");
}

template<typename T>
T& getSchemaObject()
{
    static T instance{};
    return instance;
}

// Encoder / Decoder

template<typename Schema>
class EncoderImpl : public Context {
public:
    using Config = typename SchemaConfig<Schema>::type;

    explicit EncoderImpl(const Config& cfg)
        : config(cfg)
    {
        validateConfig<Schema>();
        static_assert(
            SchemaFacet<Schema>::value == Facet::EncodeDecode ||
                SchemaFacet<Schema>::value == Facet::EncodeOnly,
            "Encoder<Schema>: Schema facet must not be DecodeOnly");
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
class DecoderImpl : public Context {
public:
    using Config = typename SchemaConfig<Schema>::type;

    explicit DecoderImpl(const Config& cfg)
        : config(cfg)
    {
        validateConfig<Schema>();
        static_assert(
            SchemaFacet<Schema>::value == Facet::EncodeDecode ||
                SchemaFacet<Schema>::value == Facet::DecodeOnly,
            "Decoder<Schema>: Schema facet must not be EncodeOnly");
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

// Custom encoder/decoder dispatch

template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder)
{
    if constexpr (HasEncoderTag<Schema, T>::value) {
        using CE = typename Schema::template CustomEncoder<T>;
        CE custom;
        custom.setEncoder(encoder);
        custom(value, dst);
        // } else if constexpr (HasEncodeValue<Schema, T>::value) {
        //     Schema::encodeValue(value, dst, encoder);
    } else {
        encodeValueDefault<Schema, T>(value, dst, encoder);
    }
}

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder)
{
    if constexpr (HasDecoderTag<Schema, T>::value) {
        using CD = typename Schema::template CustomDecoder<T>;
        CD custom;
        custom.setDecoder(decoder);
        custom(src, value);
    } else if constexpr (HasDecodeValue<Schema, T>::value) {
        Schema::decodeValue(src, value, decoder);
    } else {
        decodeValueDefault<Schema, T>(src, value, decoder);
    }
}

// Encode defaults

template<typename Schema, typename T>
void encodeValueDefault(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder)
{
    if constexpr (std::is_same_v<T, bool>) {
        dst = value;
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        dst = static_cast<std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>>(value);
    } else if constexpr (std::is_same_v<T, float>) {
        if (std::isnan(value)) {
            encoder.addError("Invalid float value - NaN");
            return;
        }
        dst = value;
    } else if constexpr (std::is_same_v<T, double>) {
        if (std::isnan(value)) {
            encoder.addError("Invalid double value - NaN");
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
        encoder.addError("Unhandled enum value during encode: " + std::to_string(U(value)));
    } else if constexpr (IsOptional<T>::value) {
        using U = typename T::value_type;
        if (!value) {
            dst = Json::nullValue;
        } else {
            encodeValue<Schema, U>(*value, dst, encoder);
        }
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
            "Unsupported type - Either a Schema::Object<T> (inherited from aison::Object) "
            "OR a custom encoder (Schema::encodeValue or Schema::CustomEncoder<T>) "
            "needs to be defined.");

        const auto& objectDef = getSchemaObject<typename Schema::template Object<T>>();
        objectDef.encodeFields(value, dst, encoder);
    } else if constexpr (std::is_pointer_v<T>) {
        static_assert(!std::is_pointer_v<T>, "Pointers are not supported");
    } else {
        static_assert(
            HasEncodeValue<Schema, T>::value,
            "Unsupported type - a custom encoder (Schema::encodeValue or "
            "Schema::CustomEncoder<T>) needs to be defined.");
    }
}

// Decode defaults

template<typename Schema, typename T>
void decodeValueDefault(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder)
{
    if constexpr (std::is_same_v<T, bool>) {
        if (!src.isBool()) {
            decoder.addError("Expected bool");
            return;
        }
        value = src.asBool();
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        if (!src.isIntegral()) {
            decoder.addError("Expected integral value");
            return;
        }
        if constexpr (std::is_signed_v<T>) {
            value = static_cast<T>(src.asInt64());
        } else {
            auto v = src.asUInt64();
            if (v > std::numeric_limits<T>::max()) {
                decoder.addError("Unsigned integer out of range");
                return;
            }
            value = static_cast<T>(v);
        }
    } else if constexpr (std::is_same_v<T, float>) {
        if (!src.isDouble() && !src.isInt()) {
            decoder.addError("Expected float");
            return;
        }
        value = static_cast<float>(src.asDouble());
    } else if constexpr (std::is_same_v<T, double>) {
        if (!src.isDouble() && !src.isInt()) {
            decoder.addError("Expected double");
            return;
        }
        value = src.asDouble();
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!src.isString()) {
            decoder.addError("Expected string");
            return;
        }
        value = src.asString();
    } else if constexpr (std::is_enum_v<T>) {
        validateEnumType<Schema, T>();

        if (!src.isString()) {
            decoder.addError("Expected string for enum");
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
        decoder.addError("Unknown enum value: " + s);
    } else if constexpr (IsOptional<T>::value) {
        using U = typename T::value_type;
        if (src.isNull()) {
            value.reset();
        } else {
            U tmp{};
            decodeValue<Schema, U>(src, tmp, decoder);
            value = std::move(tmp);
        }
    } else if constexpr (IsVector<T>::value) {
        using U = typename T::value_type;
        value.clear();
        if (!src.isArray()) {
            decoder.addError("Expected array");
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
            "Unsupported type - Either a Schema::Object<T> (inherited from aison::Object) "
            "OR a custom decoder (Schema::decodeValue or Schema::CustomDecoder<T>) "
            "needs to be defined.");

        if (!src.isObject()) {
            decoder.addError("Expected object");
            return;
        }

        const auto& objectDef = getSchemaObject<typename Schema::template Object<T>>();
        objectDef.decodeFields(src, value, decoder);
    } else if constexpr (std::is_pointer_v<T>) {
        static_assert(!std::is_pointer_v<T>, "Pointers are not supported");
    } else {
        static_assert(
            HasDecodeValue<Schema, T>::value,
            "Unsupported type - a custom decoder (Schema::decodeValue or "
            "Schema::CustomDecoder<T>) needs to be defined.");
    }
}

// Facet-specific field interfaces

template<typename Owner, typename Schema>
struct IEncodeOnlyField {
    virtual ~IEncodeOnlyField() = default;
    virtual const char* getName() const = 0;
    virtual void encodeField(
        const Owner& owner, Json::Value& dst, EncoderImpl<Schema>& encoder) const = 0;
};

template<typename Owner, typename Schema>
struct IDecodeOnlyField {
    virtual ~IDecodeOnlyField() = default;
    virtual const char* getName() const = 0;
    virtual void decodeField(
        const Json::Value& src, Owner& owner, DecoderImpl<Schema>& decoder) const = 0;
};

template<typename Owner, typename Schema>
struct IEncodeDecodeField {
    virtual ~IEncodeDecodeField() = default;
    virtual const char* getName() const = 0;
    virtual void encodeField(
        const Owner& owner, Json::Value& dst, EncoderImpl<Schema>& encoder) const = 0;
    virtual void decodeField(
        const Json::Value& src, Owner& owner, DecoderImpl<Schema>& decoder) const = 0;
};

// Field implementations

template<typename Owner, typename T, typename Schema>
struct EncodeOnlyField : IEncodeOnlyField<Owner, Schema> {
    const char* name;
    T Owner::* member;

    EncodeOnlyField(const char* name, T Owner::* member)
        : name(name)
        , member(member)
    {}

    const char* getName() const override { return name; }

    void encodeField(
        const Owner& owner, Json::Value& dst, EncoderImpl<Schema>& encoder) const override
    {
        const T& src = owner.*member;
        encodeValue<Schema, T>(src, dst, encoder);
    }
};

template<typename Owner, typename T, typename Schema>
struct DecodeOnlyField : IDecodeOnlyField<Owner, Schema> {
    const char* name;
    T Owner::* member;

    DecodeOnlyField(const char* name, T Owner::* member)
        : name(name)
        , member(member)
    {}

    const char* getName() const override { return name; }

    void decodeField(
        const Json::Value& src, Owner& owner, DecoderImpl<Schema>& decoder) const override
    {
        T& dst = owner.*member;
        decodeValue<Schema, T>(src, dst, decoder);
    }
};

template<typename Owner, typename T, typename Schema>
struct EncodeDecodeField : IEncodeDecodeField<Owner, Schema> {
    const char* name;
    T Owner::* member;

    EncodeDecodeField(const char* name, T Owner::* member)
        : name(name)
        , member(member)
    {}

    const char* getName() const override { return name; }

    void encodeField(
        const Owner& owner, Json::Value& dst, EncoderImpl<Schema>& encoder) const override
    {
        const T& src = owner.*member;
        encodeValue<Schema, T>(src, dst, encoder);
    }

    void decodeField(
        const Json::Value& src, Owner& owner, DecoderImpl<Schema>& decoder) const override
    {
        T& dst = owner.*member;
        decodeValue<Schema, T>(src, dst, decoder);
    }
};

// Object implementations per facet

// Encode-only
template<typename Schema, typename Owner>
class ObjectImpl<Schema, Owner, Facet::EncodeOnly> {
    using EncoderType = EncoderImpl<Schema>;
    using IFieldType = IEncodeOnlyField<Owner, Schema>;
    std::vector<std::unique_ptr<IFieldType>> fields_;

public:
    template<typename T>
    void add(T Owner::* member, const char* name)
    {
        fields_.push_back(std::make_unique<EncodeOnlyField<Owner, T, Schema>>(name, member));
    }

    void encodeFields(const Owner& src, Json::Value& dst, EncoderType& encoder) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            PathScope guard(encoder, field->getName());
            Json::Value& node = dst[field->getName()];
            field->encodeField(src, node, encoder);
        }
    }
};

// Decode-only
template<typename Schema, typename Owner>
class ObjectImpl<Schema, Owner, Facet::DecodeOnly> {
    using DecoderType = DecoderImpl<Schema>;
    using IFieldType = IDecodeOnlyField<Owner, Schema>;
    std::vector<std::unique_ptr<IFieldType>> fields_;

public:
    template<typename T>
    void add(T Owner::* member, const char* name)
    {
        fields_.push_back(std::make_unique<DecodeOnlyField<Owner, T, Schema>>(name, member));
    }

    void decodeFields(const Json::Value& src, Owner& dst, DecoderType& decoder) const
    {
        for (const auto& field : fields_) {
            const char* key = field->getName();
            if (!src.isMember(key)) {
                decoder.addError(std::string("Missing required field: ") + key);
                continue;
            }
            const Json::Value& node = src[key];
            PathScope guard(decoder, key);
            field->decodeField(node, dst, decoder);
        }
    }
};

// Encode+Decode
template<typename Schema, typename Owner>
class ObjectImpl<Schema, Owner, Facet::EncodeDecode> {
    using EncoderType = EncoderImpl<Schema>;
    using DecoderType = DecoderImpl<Schema>;
    using IFieldType = IEncodeDecodeField<Owner, Schema>;
    std::vector<std::unique_ptr<IFieldType>> fields_;

public:
    template<typename T>
    void add(T Owner::* member, const char* name)
    {
        fields_.push_back(std::make_unique<EncodeDecodeField<Owner, T, Schema>>(name, member));
    }

    void encodeFields(const Owner& src, Json::Value& dst, EncoderType& encoder) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            PathScope guard(encoder, field->getName());
            Json::Value& node = dst[field->getName()];
            field->encodeField(src, node, encoder);
        }
    }

    void decodeFields(const Json::Value& src, Owner& dst, DecoderType& decoder) const
    {
        for (const auto& field : fields_) {
            const char* key = field->getName();
            if (!src.isMember(key)) {
                decoder.addError(std::string("Missing required field: ") + key);
                continue;
            }
            const Json::Value& node = src[key];
            PathScope guard(decoder, key);
            field->decodeField(node, dst, decoder);
        }
    }
};

}  // namespace detail

// Object / Enum wrappers

template<typename Schema, typename Owner>
struct Object : detail::ObjectImpl<Schema, Owner, detail::SchemaFacet<Schema>::value> {
    using ObjectTag = void;
    using Base = detail::ObjectImpl<Schema, Owner, detail::SchemaFacet<Schema>::value>;
    using Base::add;
};

template<typename Schema, typename E>
struct Enum : detail::EnumImpl<Schema, E> {
    using EnumTag = void;
    using Base = detail::EnumImpl<Schema, E>;
    using Base::add;
};

// CustomEncoder / CustomDecoder bases (with setEncoder / setDecoder)

template<typename Schema, typename T>
struct Encoder {
public:
    using CustomEncoderTag = void;
    using EncoderType = aison::detail::EncoderImpl<Schema>;

    Encoder() = default;

    void setEncoder(EncoderType& enc) { encoder_ = &enc; }
    void addError(const std::string& msg) { encoder_->addError(msg); }
    const auto& config() const { return encoder_->config; }

private:
    EncoderType* encoder_ = nullptr;
};

template<typename Schema, typename T>
struct Decoder {
public:
    using CustomDecoderTag = void;
    using DecoderType = aison::detail::DecoderImpl<Schema>;

    Decoder() = default;

    void setDecoder(DecoderType& dec) { decoder_ = &dec; }
    void addError(const std::string& msg) { decoder_->addError(msg); }
    const auto& config() const { return decoder_->config; }

private:
    DecoderType* decoder_ = nullptr;
};

// Free encode/decode helpers

template<typename Schema, typename T>
Result encode(const T& value, Json::Value& dst)
{
    if constexpr (!detail::SchemaConfig<Schema>::isEmpty) {
        static_assert(
            detail::SchemaConfig<Schema>::isEmpty,
            "When Schema::Config{} is defined, a config object must be passed to encode()");
    } else {
        EmptyConfig cfg;
        return detail::EncoderImpl<Schema>(cfg).encode(value, dst);
    }
}

template<typename Schema, typename T>
Result decode(const Json::Value& src, T& value)
{
    if constexpr (!detail::SchemaConfig<Schema>::isEmpty) {
        static_assert(
            detail::SchemaConfig<Schema>::isEmpty,
            "When Schema::Config{} is defined, a config object must be passed to decode()");
    } else {
        EmptyConfig cfg;
        return detail::DecoderImpl<Schema>(cfg).decode(src, value);
    }
}

template<typename Schema, typename T>
Result encode(
    const T& value, Json::Value& dst, const typename detail::SchemaConfig<Schema>::type& config)
{
    return detail::EncoderImpl<Schema>(config).encode(value, dst);
}

template<typename Schema, typename T>
Result decode(
    const Json::Value& src, T& value, const typename detail::SchemaConfig<Schema>::type& config)
{
    return detail::DecoderImpl<Schema>(config).decode(src, value);
}

}  // namespace aison
