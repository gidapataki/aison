#pragma once

#include <json/json.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace aison {

// Forward declarations ////////////////////////////////////////////////////////////////////////////

struct EmptyConfig;
struct EncodeOnly;
struct DecodeOnly;
struct EncodeDecode;

namespace detail {

class Context;
struct PathSegment;
struct PathScope;
struct EnumBase;

template<typename Schema>
class EncoderImpl;

template<typename Schema>
class DecoderImpl;

template<typename Schema, typename T>
class EnumImpl;

template<typename Schema, typename Owner, typename FacetTag>
class ObjectImpl;

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
void encodeValueDefault(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder);

template<typename Schema, typename T>
void decodeValueDefault(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder);

template<typename Schema, typename T>
constexpr void validateEnumType();

template<typename T>
T& getSchemaObject();

}  // namespace detail

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

// Implementation //////////////////////////////////////////////////////////////////////////////////

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

    // template<typename T> struct Object;
    // template<typename T> struct Enum;
    // template<typename T> struct Encoder;
    // template<typename T> struct Decoder;
};

namespace detail {

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

// Traits ///////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct IsOptional : std::false_type {};

template<typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template<typename T>
struct IsVector : std::false_type {};

template<typename T, typename A>
struct IsVector<std::vector<T, A>> : std::true_type {};

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
class EnumImpl : public EnumBase {
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

template<typename T>
T& getSchemaObject()
{
    static T instance{};
    return instance;
}

// EncoderImpl / DecoderImpl ///////////////////////////////////////////////////////////////

template<typename Schema>
class EncoderImpl : public Context {
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
class DecoderImpl : public Context {
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
        encodeValueDefault<Schema, T>(value, dst, encoder);
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
        decodeValueDefault<Schema, T>(src, value, decoder);
    }
}

// Encode defaults ///////////////////////////////////////////////////////////////////////////

template<typename Schema, typename T>
void encodeValueDefault(const T& value, Json::Value& dst, EncoderImpl<Schema>& encoder)
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
void decodeValueDefault(const Json::Value& src, T& value, DecoderImpl<Schema>& decoder)
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
                decoder.addError("Unsigned integer out value of range.");
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

// Field descriptors (type-erased, no virtual) //////////////////////////////////////////////

// Encode-only descriptor
template<typename Schema, typename Owner>
struct EncodeOnlyFieldDesc {
    const char* name;
    using EncodeFn =
        void (*)(const Owner&, Json::Value&, EncoderImpl<Schema>&, const void* context);
    EncodeFn encode;
    const void* context;
};

// Decode-only descriptor
template<typename Schema, typename Owner>
struct DecodeOnlyFieldDesc {
    const char* name;
    using DecodeFn =
        void (*)(const Json::Value&, Owner&, DecoderImpl<Schema>&, const void* context);
    DecodeFn decode;
    const void* context;
};

// Encode+Decode descriptor
template<typename Schema, typename Owner>
struct EncodeDecodeFieldDesc {
    const char* name;
    using EncodeFn =
        void (*)(const Owner&, Json::Value&, EncoderImpl<Schema>&, const void* context);
    using DecodeFn =
        void (*)(const Json::Value&, Owner&, DecoderImpl<Schema>&, const void* context);
    EncodeFn encode;
    DecodeFn decode;
    const void* context;
};

template<typename Schema, typename Owner, typename T>
struct FieldContext {
    T Owner::* member;
};

template<typename Schema, typename Owner, typename T>
void encodeFieldThunk(
    const Owner& owner, Json::Value& dst, EncoderImpl<Schema>& encoder, const void* context)
{
    auto* ctx = static_cast<const FieldContext<Schema, Owner, T>*>(context);
    auto& member = ctx->member;
    const T& ref = owner.*member;
    encodeValue<Schema, T>(ref, dst, encoder);
}

template<typename Schema, typename Owner, typename T>
void decodeFieldThunk(
    const Json::Value& src, Owner& owner, DecoderImpl<Schema>& decoder, const void* context)
{
    auto* ctx = static_cast<const FieldContext<Schema, Owner, T>*>(context);
    auto& member = ctx->member;
    T& ref = owner.*member;
    decodeValue<Schema, T>(src, ref, decoder);
}

// Object implementations per facet /////////////////////////////////////////////////////////

// Encode-only
template<typename Schema, typename Owner>
class ObjectImpl<Schema, Owner, EncodeOnly> {
    using EncoderType = EncoderImpl<Schema>;
    using Field = EncodeOnlyFieldDesc<Schema, Owner>;
    std::vector<Field> fields_;

public:
    template<typename T>
    void add(T Owner::* member, const char* name)
    {
        using Ctx = FieldContext<Schema, Owner, T>;
        static const Ctx ctx{member};

        for (const auto& field : fields_) {
            if (std::strcmp(field.name, name) == 0 || field.context == &ctx) {
                if constexpr (Schema::EnableAssert::value) {
                    assert(false && "Duplicate field mapping in Schema::Object.");
                }
                return;
            }
        }

        Field f;
        f.name = name;
        f.encode = &encodeFieldThunk<Schema, Owner, T>;
        f.context = &ctx;

        fields_.push_back(f);
    }

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
class ObjectImpl<Schema, Owner, DecodeOnly> {
    using DecoderType = DecoderImpl<Schema>;
    using Field = DecodeOnlyFieldDesc<Schema, Owner>;
    std::vector<Field> fields_;

public:
    template<typename T>
    void add(T Owner::* member, const char* name)
    {
        using Ctx = FieldContext<Schema, Owner, T>;
        static const Ctx ctx{member};

        for (const auto& field : fields_) {
            if (std::strcmp(field.name, name) == 0 || field.context == &ctx) {
                if constexpr (Schema::EnableAssert::value) {
                    assert(false && "Duplicate field mapping in Schema::Object.");
                }
                return;
            }
        }

        Field f;
        f.name = name;
        f.decode = &decodeFieldThunk<Schema, Owner, T>;
        f.context = &ctx;

        fields_.push_back(f);
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

// Encode+Decode
template<typename Schema, typename Owner>
class ObjectImpl<Schema, Owner, EncodeDecode> {
    using EncoderType = EncoderImpl<Schema>;
    using DecoderType = DecoderImpl<Schema>;
    using Field = EncodeDecodeFieldDesc<Schema, Owner>;
    std::vector<Field> fields_;

public:
    template<typename T>
    void add(T Owner::* member, const char* name)
    {
        using Ctx = FieldContext<Schema, Owner, T>;
        static const Ctx ctx{member};

        for (const auto& field : fields_) {
            if (std::strcmp(field.name, name) == 0 || field.context == &ctx) {
                if constexpr (Schema::EnableAssert::value) {
                    assert(false && "Duplicate field mapping in Schema::Object.");
                }
                return;
            }
        }

        Field f;
        f.name = name;
        f.encode = &encodeFieldThunk<Schema, Owner, T>;
        f.decode = &decodeFieldThunk<Schema, Owner, T>;
        f.context = &ctx;

        fields_.push_back(f);
    }

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

}  // namespace detail

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

// Encoder / Decoder bases (with setEncoder / setDecoder) ///////////////////////

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
