#pragma once

#include <json/json.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace aison {

struct Error {
    std::string path;
    std::string message;
};

struct Result {
    std::vector<Error> errors;

    explicit operator bool() const { return errors.empty(); }
};

// Facet tags
struct EncodeOnly {};
struct DecodeOnly {};
struct EncodeDecode {};

// Forward declarations
template<typename Schema, typename Owner, typename Facet = EncodeDecode> struct Object;
template<typename E, size_t N> using EnumMap = std::array<std::pair<E, std::string_view>, N>;

namespace detail {

struct PathScope;

struct PathSegment {
    enum class Kind { kKey, kIndex } kind = {};

    union Data {
        const char* key;    // valid if kind == Key
        std::size_t index;  // valid if kind == Index

        constexpr Data() : key(nullptr) {}
        constexpr Data(const char* k) : key(k) {}
        constexpr Data(std::size_t i) : index(i) {}
    } data;

    static PathSegment makeKey(const char* k)
    {
        PathSegment segment;
        segment.kind = Kind::kKey;
        segment.data = Data{k};
        return segment;
    }

    static PathSegment makeIndex(std::size_t i)
    {
        PathSegment segment;
        segment.kind = Kind::kIndex;
        segment.data = Data{i};
        return segment;
    }
};

class Context {
public:
    std::string buildPath() const
    {
        std::string result = "$";
        result.reserve(64);

        for (const auto& seg : pathStack_) {
            if (seg.kind == PathSegment::Kind::kKey) {
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

    PathScope(Context& context, const char* key) : ctx(&context)
    {
        ctx->pathStack_.push_back(PathSegment::makeKey(key));
    }

    PathScope(Context& context, std::size_t index) : ctx(&context)
    {
        ctx->pathStack_.push_back(PathSegment::makeIndex(index));
    }

    PathScope(const PathScope&) = delete;
    PathScope& operator=(const PathScope&) = delete;

    PathScope(PathScope&& other) noexcept : ctx(other.ctx) { other.ctx = nullptr; }
    PathScope& operator=(PathScope&&) = delete;

    ~PathScope()
    {
        if (ctx) {
            ctx->pathStack_.pop_back();
        }
    }
};

template<typename Schema> class Encoder : public Context {
public:
    template<typename T> Result encode(const T& value, Json::Value& dst);
};

template<typename Schema> class Decoder : public Context {
public:
    template<typename T> Result decode(const Json::Value& src, T& value);
};

// Traits

template<typename T> struct IsOptional : std::false_type {};

template<typename T> struct IsOptional<std::optional<T>> : std::true_type {};

template<typename T> struct IsVector : std::false_type {};

template<typename T, typename A> struct IsVector<std::vector<T, A>> : std::true_type {};

template<typename Schema, typename T, typename = void> struct HasEncodeValue : std::false_type {};

template<typename Schema, typename T>
struct HasEncodeValue<
    Schema, T,
    std::void_t<decltype(Schema::encodeValue(
        std::declval<const T&>(), std::declval<Json::Value&>(), std::declval<Encoder<Schema>&>()))>>
    : std::true_type {};

template<typename Schema, typename T, typename = void> struct HasDecodeValue : std::false_type {};

template<typename Schema, typename T>
struct HasDecodeValue<
    Schema, T,
    std::void_t<decltype(Schema::decodeValue(
        std::declval<const Json::Value&>(), std::declval<T&>(), std::declval<Decoder<Schema>&>()))>>
    : std::true_type {};

// Detect Schema::Enum<E> mapping
template<typename Schema, typename E, typename = void> struct HasEnumT : std::false_type {};

template<typename Schema, typename E>
struct HasEnumT<Schema, E, std::void_t<typename Schema::template Enum<E>>> : std::true_type {};

// Encode / decode forward declarations

template<typename Schema, typename T>
void encodeValueDefault(const T& value, Json::Value& dst, Encoder<Schema>& encoder);

template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, Encoder<Schema>& encoder);

template<typename Schema, typename T>
void decodeValueDefault(const Json::Value& src, T& value, Decoder<Schema>& decoder);

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, Decoder<Schema>& decoder);

// Encode defaults

template<typename Schema, typename T>
void encodeValueDefault(const T& value, Json::Value& dst, Encoder<Schema>& encoder)
{
    if constexpr (std::is_same_v<T, int>) {
        dst = value;
    } else if constexpr (std::is_same_v<T, float>) {
        dst = value;
    } else if constexpr (std::is_same_v<T, double>) {
        dst = value;
    } else if constexpr (std::is_same_v<T, bool>) {
        dst = value;
    } else if constexpr (std::is_same_v<T, std::string>) {
        dst = value;
    } else if constexpr (std::is_enum_v<T> && HasEnumT<Schema, T>::value) {
        using EnumSpec = typename Schema::template Enum<T>;
        const auto& mapping = EnumSpec::mapping;
        for (const auto& entry : mapping) {
            if (entry.first == value) {
                dst = Json::Value(std::string(entry.second));
                return;
            }
        }
        encoder.addError("Unhandled enum value during encode");
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
    } else {
        typename Schema::template Fields<T> fields;
        fields.encodeObject(value, dst, encoder);
    }
}

// Decode defaults

template<typename Schema, typename T>
void decodeValueDefault(const Json::Value& src, T& value, Decoder<Schema>& decoder)
{
    if constexpr (std::is_same_v<T, int>) {
        if (!src.isInt()) {
            decoder.addError("Expected integer");
            return;
        }
        value = src.asInt();
    } else if constexpr (std::is_same_v<T, float>) {
        if (!src.isDouble() && !src.isInt()) {
            decoder.addError("Expected float/double");
            return;
        }
        value = (float)src.asDouble();
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
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!src.isBool()) {
            decoder.addError("Expected bool");
            return;
        }
        value = src.asBool();
    } else if constexpr (std::is_enum_v<T> && HasEnumT<Schema, T>::value) {
        if (!src.isString()) {
            decoder.addError("Expected string for enum");
            return;
        }
        const std::string s = src.asString();
        using EnumSpec = typename Schema::template Enum<T>;
        const auto& mapping = EnumSpec::mapping;
        for (const auto& entry : mapping) {
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
    } else {
        if (!src.isObject()) {
            decoder.addError("Expected object");
            return;
        }
        typename Schema::template Fields<T> fields;
        fields.decodeObject(src, value, decoder);
    }
}

// Encode / decode dispatcher

template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, Encoder<Schema>& encoder)
{
    if constexpr (HasEncodeValue<Schema, T>::value) {
        Schema::encodeValue(value, dst, encoder);
    } else {
        encodeValueDefault<Schema, T>(value, dst, encoder);
    }
}

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, Decoder<Schema>& decoder)
{
    if constexpr (HasDecodeValue<Schema, T>::value) {
        Schema::decodeValue(src, value, decoder);
    } else {
        decodeValueDefault<Schema, T>(src, value, decoder);
    }
}

// Encode / decode implementation

template<typename Schema>
template<typename T>
inline Result Encoder<Schema>::encode(const T& value, Json::Value& dst)
{
    this->errors_.clear();
    encodeValue<Schema, T>(value, dst, *this);
    return Result{std::move(this->errors_)};
}

template<typename Schema>
template<typename T>
inline Result Decoder<Schema>::decode(const Json::Value& src, T& value)
{
    this->errors_.clear();
    decodeValue<Schema, T>(src, value, *this);
    return Result{std::move(this->errors_)};
}

// Field facet interfaces

template<typename Owner, typename Schema> struct IEncodeOnlyField {
    virtual ~IEncodeOnlyField() = default;
    virtual const char* getName() const = 0;
    virtual void encodeField(
        const Owner& owner, Json::Value& dst, Encoder<Schema>& encoder) const = 0;
};

template<typename Owner, typename Schema> struct IDecodeOnlyField {
    virtual ~IDecodeOnlyField() = default;
    virtual const char* getName() const = 0;
    virtual void decodeField(
        const Json::Value& src, Owner& owner, Decoder<Schema>& decoder) const = 0;
};

template<typename Owner, typename Schema> struct IEncodeDecodeField {
    virtual ~IEncodeDecodeField() = default;
    virtual const char* getName() const = 0;
    virtual void encodeField(
        const Owner& owner, Json::Value& dst, Encoder<Schema>& encoder) const = 0;
    virtual void decodeField(
        const Json::Value& src, Owner& owner, Decoder<Schema>& decoder) const = 0;
};

// Field facet implementations

template<typename Owner, typename T, typename Schema>
struct EncodeOnlyField : IEncodeOnlyField<Owner, Schema> {
    const char* name;
    T Owner::* member;

    EncodeOnlyField(const char* name, T Owner::* member) : name(name), member(member) {}

    const char* getName() const override { return name; }

    void encodeField(const Owner& owner, Json::Value& dst, Encoder<Schema>& encoder) const override
    {
        const T& src = owner.*member;
        encodeValue<Schema, T>(src, dst, encoder);
    }
};

template<typename Owner, typename T, typename Schema>
struct DecodeOnlyField : IDecodeOnlyField<Owner, Schema> {
    const char* name;
    T Owner::* member;

    DecodeOnlyField(const char* name, T Owner::* member) : name(name), member(member) {}

    const char* getName() const override { return name; }

    void decodeField(const Json::Value& src, Owner& owner, Decoder<Schema>& decoder) const override
    {
        T& dst = owner.*member;
        decodeValue<Schema, T>(src, dst, decoder);
    }
};

template<typename Owner, typename T, typename Schema>
struct EncodeDecodeField : IEncodeDecodeField<Owner, Schema> {
    const char* name;
    T Owner::* member;

    EncodeDecodeField(const char* name, T Owner::* member) : name(name), member(member) {}

    const char* getName() const override { return name; }

    void encodeField(const Owner& owner, Json::Value& dst, Encoder<Schema>& encoder) const override
    {
        const T& src = owner.*member;
        encodeValue<Schema, T>(src, dst, encoder);
    }

    void decodeField(const Json::Value& src, Owner& owner, Decoder<Schema>& decoder) const override
    {
        T& dst = owner.*member;
        decodeValue<Schema, T>(src, dst, decoder);
    }
};

// Object facets

template<typename Schema, typename Owner, typename Facet> class ObjectImpl;

template<typename Schema, typename Owner> class ObjectImpl<Schema, Owner, EncodeOnly> {
    using Encoder = Encoder<Schema>;
    using IField = IEncodeOnlyField<Owner, Schema>;

    std::vector<std::unique_ptr<IField>> fields_;

public:
    template<typename T> void add(T Owner::* member, const char* name)
    {
        fields_.push_back(std::make_unique<EncodeOnlyField<Owner, T, Schema>>(name, member));
    }

    std::size_t size() const { return fields_.size(); }

    auto begin() { return fields_.begin(); }
    auto end() { return fields_.end(); }
    auto begin() const { return fields_.begin(); }
    auto end() const { return fields_.end(); }

    void encodeObject(const Owner& src, Json::Value& dst, Encoder& encoder) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            PathScope guard(encoder, field->getName());
            Json::Value& node = dst[field->getName()];
            field->encodeField(src, node, encoder);
        }
    }
};

template<typename Schema, typename Owner> class ObjectImpl<Schema, Owner, DecodeOnly> {
    using Decoder = Decoder<Schema>;
    using IField = IDecodeOnlyField<Owner, Schema>;

    std::vector<std::unique_ptr<IField>> fields_;

public:
    template<typename T> void add(T Owner::* member, const char* name)
    {
        fields_.push_back(std::make_unique<DecodeOnlyField<Owner, T, Schema>>(name, member));
    }

    std::size_t size() const { return fields_.size(); }

    auto begin() { return fields_.begin(); }
    auto end() { return fields_.end(); }
    auto begin() const { return fields_.begin(); }
    auto end() const { return fields_.end(); }

    void decodeObject(const Json::Value& src, Owner& dst, Decoder& decoder) const
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

template<typename Schema, typename Owner> class ObjectImpl<Schema, Owner, EncodeDecode> {
    using Encoder = Encoder<Schema>;
    using Decoder = Decoder<Schema>;
    using IField = IEncodeDecodeField<Owner, Schema>;

    std::vector<std::unique_ptr<IField>> fields_;

public:
    template<typename T> void add(T Owner::* member, const char* name)
    {
        fields_.push_back(std::make_unique<EncodeDecodeField<Owner, T, Schema>>(name, member));
    }

    std::size_t size() const { return fields_.size(); }

    auto begin() { return fields_.begin(); }
    auto end() { return fields_.end(); }
    auto begin() const { return fields_.begin(); }
    auto end() const { return fields_.end(); }

    void encodeObject(const Owner& src, Json::Value& dst, Encoder& encoder) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            PathScope guard(encoder, field->getName());
            Json::Value& node = dst[field->getName()];
            field->encodeField(src, node, encoder);
        }
    }

    void decodeObject(const Json::Value& src, Owner& dst, Decoder& decoder) const
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

template<typename Schema> using Encoder = detail::Encoder<Schema>;

template<typename Schema> using Decoder = detail::Decoder<Schema>;

template<typename Schema, typename Owner, typename Facet>
struct Object : detail::ObjectImpl<Schema, Owner, Facet> {
    using Base = detail::ObjectImpl<Schema, Owner, Facet>;
    using Base::add;
};

template<typename Schema, typename T> Result encode(const T& value, Json::Value& dst)
{
    return detail::Encoder<Schema>().encode(value, dst);
}

template<typename Schema, typename T> Result decode(const Json::Value& src, T& value)
{
    return detail::Decoder<Schema>().decode(src, value);
}

}  // namespace aison
