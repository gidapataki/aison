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
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace aison {

// Declarations //////////////////////////////////////////////////////////////////////////////////

struct Result;
struct IntrospectResult;

using TypeId = const void*;

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

template<typename T>
TypeId getTypeId();

template<typename Schema, typename T>
Result encode(const T& value, Json::Value& dst, const typename Schema::ConfigType& config = {});

template<typename Schema, typename T>
Result decode(const Json::Value& src, T& value, const typename Schema::ConfigType& config = {});

template<typename Schema, typename... Ts>
IntrospectResult introspect();

// Definitions ///////////////////////////////////////////////////////////////////////////////////

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
    static constexpr auto enableEncode = true;
    static constexpr auto enableDecode = true;
    static constexpr auto enableIntrospect = false;
};

struct UnknownInfo {};
struct BoolInfo {};
struct StringInfo {};

struct OptionalInfo {
    TypeId type = nullptr;
};

struct VectorInfo {
    TypeId type = nullptr;
};

struct IntegralInfo {
    int size = 0;
    bool isSigned = false;
};

struct FloatingInfo {
    int size = 0;
};

struct FieldInfo {
    std::string name;
    TypeId type = nullptr;
    bool isRequired = true;
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
    TypeId type = nullptr;
};

struct VariantInfo {
    std::string name;
    std::string discriminator;
    std::vector<AlternativeInfo> alternatives;
};

struct CustomInfo {
    std::string name;
};

using TypeInfo = std::variant<
    UnknownInfo,
    BoolInfo,
    StringInfo,
    IntegralInfo,
    FloatingInfo,
    OptionalInfo,
    VectorInfo,
    ObjectInfo,
    EnumInfo,
    VariantInfo,
    CustomInfo>;

struct IntrospectResult {
    std::unordered_map<TypeId, TypeInfo> types;
    std::vector<Error> errors;

    explicit operator bool() const { return errors.empty(); }
};

}  // namespace aison

namespace aison::detail {

// Declarations (detail) /////////////////////////////////////////////////////////////////////////

using FieldAccessorDeleter = void (*)(void*);
using FieldAccessorStorage = std::unique_ptr<void, FieldAccessorDeleter>;
using FieldAccessorPtr = const void*;
using FieldAccessorId = const void*;

template<typename Schema, typename T>
using SchemaObject = typename Schema::template Object<T>;

template<typename Schema, typename T>
using SchemaEnum = typename Schema::template Enum<T>;

template<typename Schema, typename T>
using SchemaVariant = typename Schema::template Variant<T>;

template<typename Schema, typename T>
using SchemaCustom = typename Schema::template Custom<T>;

class Context;

template<typename Schema>
class EncodeContext;

template<typename Schema>
class DecodeContext;

template<typename Schema>
class IntrospectContext;

template<typename Schema, typename T>
class EnumImpl;

template<typename Schema, typename T>
class ObjectImpl;

template<typename Schema, typename T>
class VariantImpl;

template<typename Schema, typename T>
class CustomImpl;

template<typename Schema, typename T>
struct VariantDecoder;

template<typename Owner, typename T>
struct FieldAccessor;

template<typename T>
struct IsOptional;

template<typename T>
struct IsVector;

template<typename T>
struct IsVariant;

template<typename Schema, typename = void>
struct HasSchemaTag;

template<typename Schema, typename T, typename = void>
struct HasEnumTag;

template<typename Schema, typename T, typename = void>
struct HasObjectTag;

template<typename Schema, typename Variant, typename = void>
struct HasVariantTag;

template<typename Schema, typename T, typename = void>
struct HasCustomTag;

template<typename Schema, typename CustomSpec, typename T, typename = void>
struct HasCustomEncode;

template<typename Schema, typename CustomSpec, typename T, typename = void>
struct HasCustomDecode;

template<typename Schema, typename = void>
struct SchemaEnableAssert;

template<typename Schema, typename = void>
struct SchemaStrictOptional;

template<typename Schema, typename = void>
struct SchemaEnableIntrospect;

template<typename...>
struct DependentFalse;

template<typename Schema, typename T>
void encodeValue(const T& value, Json::Value& dst, EncodeContext<Schema>& ctx);

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& value, DecodeContext<Schema>& ctx);

template<typename Schema, typename T>
void encodeDefault(const T& value, Json::Value& dst, EncodeContext<Schema>& ctx);

template<typename Schema, typename T>
void decodeDefault(const Json::Value& src, T& value, DecodeContext<Schema>& ctx);

template<typename Schema, typename Owner, typename T>
void encodeFieldThunk(
    const Owner& owner, Json::Value& dst, EncodeContext<Schema>& ctx, FieldAccessorPtr ptr);

template<typename Schema, typename Owner, typename T>
void decodeFieldThunk(
    const Json::Value& src, Owner& owner, DecodeContext<Schema>& ctx, FieldAccessorPtr ptr);

template<typename Schema, typename T>
void introspectFieldThunk(IntrospectContext<Schema>& ctx);

template<typename Schema>
constexpr bool getEncodeEnabled();

template<typename Schema>
constexpr bool getDecodeEnabled();

template<typename Schema>
constexpr bool getEnableAssert();

template<typename Schema>
constexpr bool getStrictOptional();

template<typename Schema>
constexpr bool getIntrospectEnabled();

template<typename Schema, typename T>
constexpr void validateEnumSpec();

template<typename Schema, typename T>
constexpr void validateObjectSpec();

template<typename Schema, typename T>
constexpr void validateCustomSpec();

template<typename Schema, typename Variant>
constexpr void validateVariantSpec();

template<typename Schema, typename T>
std::string schemaTypeName();

template<typename Schema, typename T>
constexpr std::string_view getObjectName();

template<typename Schema, typename T>
constexpr std::string_view getEnumName();

template<typename Schema, typename T>
constexpr std::string_view getVariantName();

template<typename Schema, typename T>
constexpr std::string_view getVariantDiscriminator();

template<typename Schema, typename T>
constexpr std::string_view getCustomName();

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

template<typename Schema, typename T>
void introspectType(IntrospectContext<Schema>& ctx);

// Definitions (detail) //////////////////////////////////////////////////////////////////////////

// == Schema ==

template<typename Schema, typename>
struct HasSchemaTag : std::false_type {};

template<typename Schema>
struct HasSchemaTag<Schema, std::void_t<typename Schema::SchemaTag>> : std::true_type {};

template<typename Schema, typename T>
std::string schemaTypeName()
{
    using Type = std::decay_t<T>;
    if constexpr (HasObjectTag<Schema, Type>::value) {
        return std::string(getObjectName<Schema, Type>());
    } else if constexpr (HasEnumTag<Schema, Type>::value) {
        return std::string(getEnumName<Schema, Type>());
    } else if constexpr (HasVariantTag<Schema, Type>::value) {
        return std::string(getVariantName<Schema, Type>());
    } else if constexpr (HasCustomTag<Schema, Type>::value) {
        return std::string(getCustomName<Schema, Type>());
    }
    return "#" + std::to_string(reinterpret_cast<std::uintptr_t>(getTypeId<T>()));
}

template<typename...>
struct SchemaErrorKeyTag {};

template<typename Key>
bool shouldReportSchemaError()
{
    // fallback when context does not provide per-context dedup
    static bool reported = false;
    if (reported) {
        return false;
    }
    reported = true;
    return true;
}

template<typename Ctx, typename = void>
struct HasSchemaErrorMark : std::false_type {};

template<typename Ctx>
struct HasSchemaErrorMark<
    Ctx,
    std::void_t<decltype(std::declval<Ctx&>().markSchemaError(std::declval<std::size_t>()))>>
    : std::true_type {};

template<typename Key, typename Ctx>
void addSchemaErrorOnce(Ctx& ctx, const std::string& message)
{
    if constexpr (HasSchemaErrorMark<Ctx>::value) {
        const std::size_t key = typeid(Key).hash_code();
        if (ctx.markSchemaError(key)) {
            ctx.addError(message);
        }
    } else {
        if (shouldReportSchemaError<Key>()) {
            ctx.addError(message);
        }
    }
}

// StrictOptional

template<typename Schema, typename>
struct SchemaStrictOptional {
    constexpr static bool get() { return SchemaDefaults::strictOptional; }
};

template<typename Schema>
struct SchemaStrictOptional<Schema, std::void_t<decltype(Schema::strictOptional)>> {
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
// EnableAssert

template<typename Schema, typename>
struct SchemaEnableAssert {
    constexpr static bool get() { return SchemaDefaults::enableAssert; }
};

template<typename Schema>
struct SchemaEnableAssert<Schema, std::void_t<decltype(Schema::enableAssert)>> {
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

// EnableIntrospect

template<typename Schema, typename>
struct SchemaEnableIntrospect {
    constexpr static bool get() { return SchemaDefaults::enableIntrospect; }
};

template<typename Schema>
struct SchemaEnableIntrospect<Schema, std::void_t<decltype(Schema::enableIntrospect)>> {
    constexpr static bool get()
    {
        using Type = std::decay_t<decltype(Schema::enableIntrospect)>;
        static_assert(std::is_same_v<Type, bool>, "Schema::enableIntrospect must be bool.");
        return Schema::enableIntrospect;
    }
};

template<typename Schema>
constexpr bool getIntrospectEnabled()
{
    return SchemaEnableIntrospect<Schema>::get();
}

// EnableEncode

template<typename Schema, typename = void>
struct SchemaEnableEncode {
    constexpr static bool get() { return SchemaDefaults::enableEncode; }
};

template<typename Schema>
struct SchemaEnableEncode<Schema, std::void_t<decltype(Schema::enableEncode)>> {
    constexpr static bool get()
    {
        using Type = std::decay_t<decltype(Schema::enableEncode)>;
        static_assert(std::is_same_v<Type, bool>, "Schema::enableEncode must be bool.");
        return Schema::enableEncode;
    }
};

template<typename Schema>
constexpr bool getEncodeEnabled()
{
    return SchemaEnableEncode<Schema>::get();
}

// EnableDecode

template<typename Schema, typename = void>
struct SchemaEnableDecode {
    constexpr static bool get() { return SchemaDefaults::enableDecode; }
};

template<typename Schema>
struct SchemaEnableDecode<Schema, std::void_t<decltype(Schema::enableDecode)>> {
    constexpr static bool get()
    {
        using Type = std::decay_t<decltype(Schema::enableDecode)>;
        static_assert(std::is_same_v<Type, bool>, "Schema::enableDecode must be bool.");
        return Schema::enableDecode;
    }
};

template<typename Schema>
constexpr bool getDecodeEnabled()
{
    return SchemaEnableDecode<Schema>::get();
}

//

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

// == Schema names and discriminators ==

template<typename T, typename = void>
struct HasStaticNameMember : std::false_type {};

template<typename T>
struct HasStaticNameMember<T, std::void_t<decltype(T::name)>> : std::true_type {};

template<typename T, typename = void>
struct HasStaticDiscriminatorMember : std::false_type {};

template<typename T>
struct HasStaticDiscriminatorMember<T, std::void_t<decltype(T::discriminator)>> : std::true_type {};

constexpr std::string_view toStringView(std::string_view value)
{
    return value;
}

template<std::size_t N>
constexpr std::string_view toStringView(const char (&value)[N])
{
    return std::string_view{value, N - 1};
}

constexpr std::string_view toStringView(const char* value)
{
    return value ? std::string_view{value} : std::string_view{};
}

template<typename Spec>
constexpr std::string_view getStaticName()
{
    if constexpr (HasStaticNameMember<Spec>::value) {
        return toStringView(Spec::name);
    }
    return {};
}

template<typename Spec>
constexpr std::string_view getStaticDiscriminator()
{
    if constexpr (HasStaticDiscriminatorMember<Spec>::value) {
        return toStringView(Spec::discriminator);
    }
    return {};
}

template<typename Schema, typename T>
constexpr std::string_view getObjectName()
{
    using Spec = SchemaObject<Schema, std::decay_t<T>>;
    constexpr auto name = getStaticName<Spec>();
    if constexpr (getIntrospectEnabled<Schema>()) {
        static_assert(
            HasStaticNameMember<Spec>::value,
            "Schema::Object<T> must declare `static constexpr auto name = \"...\";` "
            "when introspection is enabled.");
        static_assert(name.size() > 0, "Schema::Object<T>::name must be non-empty.");
    }
    return name;
}

template<typename Schema, typename T>
constexpr std::string_view getEnumName()
{
    using Spec = SchemaEnum<Schema, std::decay_t<T>>;
    constexpr auto name = getStaticName<Spec>();
    if constexpr (getIntrospectEnabled<Schema>()) {
        static_assert(
            HasStaticNameMember<Spec>::value,
            "Schema::Enum<T> must declare `static constexpr auto name = \"...\";` "
            "when introspection is enabled.");
        static_assert(name.size() > 0, "Schema::Enum<T>::name must be non-empty.");
    }
    return name;
}

template<typename Schema, typename T>
constexpr std::string_view getVariantName()
{
    using Spec = SchemaVariant<Schema, std::decay_t<T>>;
    constexpr auto name = getStaticName<Spec>();
    if constexpr (getIntrospectEnabled<Schema>()) {
        static_assert(
            HasStaticNameMember<Spec>::value,
            "Schema::Variant<V> must declare `static constexpr auto name = \"...\";` "
            "when introspection is enabled.");
        static_assert(name.size() > 0, "Schema::Variant<V>::name must be non-empty.");
    }
    return name;
}

template<typename Schema, typename T>
constexpr std::string_view getVariantDiscriminator()
{
    using Spec = SchemaVariant<Schema, std::decay_t<T>>;
    static_assert(
        HasStaticDiscriminatorMember<Spec>::value,
        "Schema::Variant<V> must declare `static constexpr auto discriminator = \"...\";`.");
    constexpr auto disc = getStaticDiscriminator<Spec>();
    static_assert(disc.size() > 0, "Schema::Variant<V>::discriminator must be non-empty.");
    return disc;
}

template<typename Schema, typename T>
constexpr std::string_view getCustomName()
{
    using Spec = SchemaCustom<Schema, std::decay_t<T>>;
    constexpr auto name = getStaticName<Spec>();
    if constexpr (getIntrospectEnabled<Schema>()) {
        static_assert(
            HasStaticNameMember<Spec>::value,
            "Schema::Custom<T> must declare `static constexpr auto name = \"...\";` "
            "when introspection is enabled.");
        static_assert(name.size() > 0, "Schema::Custom<T>::name must be non-empty.");
    }
    return name;
}

// == Object ==

template<typename Schema, typename Owner>
class ObjectImpl
{
public:
    struct FieldDef {
        using EncodeFn =
            void (*)(const Owner&, Json::Value&, EncodeContext<Schema>&, FieldAccessorPtr ptr);
        using DecodeFn =
            void (*)(const Json::Value&, Owner&, DecodeContext<Schema>&, FieldAccessorPtr ptr);
        using IntrospectFn = void (*)(IntrospectContext<Schema>&);

        FieldDef(FieldAccessorStorage&& accessor)
            : accessor(std::move(accessor))
        {}

        FieldAccessorStorage accessor;
        FieldAccessorId accessorId = nullptr;  // Note: this is needed to avoid UB casts
        IntrospectFn introspect = nullptr;
        EncodeFn encode = nullptr;
        DecodeFn decode = nullptr;

        std::string name;
        TypeId typeId = nullptr;
        bool isRequired = true;
    };

    template<typename T>
    void add(T Owner::* member, std::string_view name)
    {
        static_assert(
            isSupportedFieldType<Schema, T>(),
            "Unsupported field type in Schema::Object<T>::add(...). Provide a Schema::Object / "
            "Enum / Custom / Variant mapping or use supported scalar/collection types.");

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
        field.isRequired = getStrictOptional<Schema>() || !IsOptional<T>::value;

        if constexpr (getEncodeEnabled<Schema>()) {
            field.encode = &encodeFieldThunk<Schema, Owner, T>;
        }

        if constexpr (getDecodeEnabled<Schema>()) {
            field.decode = &decodeFieldThunk<Schema, Owner, T>;
        }

        if constexpr (getIntrospectEnabled<Schema>()) {
            field.introspect = &introspectFieldThunk<Schema, T>;
            field.typeId = getTypeId<std::decay_t<T>>();
        }
    }

    template<typename S = Schema, typename = std::enable_if_t<getEncodeEnabled<S>()>>
    void encodeFields(const Owner& src, Json::Value& dst, EncodeContext<Schema>& ctx) const
    {
        dst = Json::objectValue;
        for (const auto& field : fields_) {
            auto guard = ctx.guard(field.name);
            Json::Value node;
            field.encode(src, node, ctx, field.accessor.get());
            if (!field.isRequired && node.isNull()) {
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
                if (!field.isRequired) {
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

    static constexpr std::string_view name() { return getObjectName<Schema, Owner>(); }
    static constexpr bool hasName()
    {
        return HasStaticNameMember<SchemaObject<Schema, Owner>>::value &&
               !getObjectName<Schema, Owner>().empty();
    }
    static constexpr bool hasVariantTag() { return hasName(); }
    static constexpr std::string_view variantTag() { return name(); }
    const std::vector<FieldDef>& fields() const { return fields_; }

private:
    std::vector<FieldDef> fields_;
};

template<typename Schema, typename T>
constexpr void validateObjectType()
{
    validateObjectSpec<Schema, T>();
}

template<typename Schema, typename T>
constexpr void validateObjectSpec()
{
    using Type = std::decay_t<T>;
    static_assert(
        HasObjectTag<Schema, Type>::value,
        "No schema object mapping for this type. Define `template<> struct Schema::Object<T> : "
        "aison::Object<Schema, T>` and map its fields.");
    using ObjectSpec = SchemaObject<Schema, Type>;
    static_assert(
        std::is_base_of_v<aison::Object<Schema, Type>, ObjectSpec>,
        "Schema::Object<T> must inherit from aison::Object<Schema, T>.");
    if constexpr (getIntrospectEnabled<Schema>()) {
        (void)getObjectName<Schema, Type>();
    }
}

template<typename Schema, typename T>
auto& getObjectDef()
{
    using Type = SchemaObject<Schema, T>;
    static Type instance{};
    return instance;
}

// == Enum ==

template<typename Schema, typename E>
class EnumImpl
{
public:
    using Entry = std::pair<E, std::string>;
    using EntryVec = std::vector<Entry>;

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

    static constexpr std::string_view name() { return getEnumName<Schema, E>(); }
    static constexpr bool hasName()
    {
        return HasStaticNameMember<SchemaEnum<Schema, E>>::value &&
               !getEnumName<Schema, E>().empty();
    }
    const EntryVec& entries() const { return entries_; }

private:
    std::vector<Entry> entries_;
};

template<typename Schema, typename T>
auto& getEnumDef()
{
    using Type = SchemaEnum<Schema, T>;
    static Type instance{};
    return instance;
}

template<typename Schema, typename T>
constexpr void validateEnumSpec()
{
    using Type = std::decay_t<T>;
    static_assert(
        HasEnumTag<Schema, Type>::value,
        "No schema enum mapping for this type. Define `template<> struct Schema::Enum<T> : "
        "aison::Enum<Schema, T>` and list all enum values.");
    using EnumSpec = SchemaEnum<Schema, Type>;
    static_assert(
        std::is_base_of_v<aison::Enum<Schema, Type>, EnumSpec>,
        "Schema::Enum<T> must inherit from aison::Enum<Schema, T>.");
    if constexpr (getIntrospectEnabled<Schema>()) {
        (void)getEnumName<Schema, Type>();
    }
}

// == Variant ==

template<typename Schema, typename... Ts>
class VariantImpl<Schema, std::variant<Ts...>>
{
public:
    using VariantType = std::variant<Ts...>;
    using DecodeFn =
        void (*)(const Json::Value&, VariantType&, DecodeContext<Schema>&);
    using IntrospectFn = void (*)(IntrospectContext<Schema>&);

    struct Alternative {
        std::string tag;
        DecodeFn decode = nullptr;
        IntrospectFn introspect = nullptr;
        TypeId typeId = nullptr;
    };

    template<typename Alt>
    void add(std::string_view tag)
    {
        static_assert(
            (std::is_same_v<Alt, Ts> || ...),
            "Variant::add<T>(...) must add an alternative that is present in the std::variant.");
        if (tag.empty()) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Variant alternative tag cannot be empty.");
            }
            return;
        }
        const auto tagStr = std::string(tag);
        if (tagToIndex_.count(tagStr) > 0 || typeToIndex_.count(getTypeId<Alt>()) > 0) {
            if constexpr (getEnableAssert<Schema>()) {
                assert(false && "Duplicate variant alternative tag or type.");
            }
            return;
        }

        auto idx = alts_.size();
        alts_.push_back(
            Alternative{tagStr, &decodeAlt<Alt>, &introspectAlt<Alt>, getTypeId<Alt>()});
        tagToIndex_[tagStr] = idx;
        typeToIndex_[getTypeId<Alt>()] = idx;
    }

    bool validateCompleteness(Context& ctx) const
    {
        if (alts_.size() == sizeof...(Ts)) {
            return true;
        }
        using Key = SchemaErrorKeyTag<Schema, VariantType, struct VariantIncompleteMapping>;
        addSchemaErrorOnce<Key>(ctx, "(Schema error) Variant mapping missing alternatives.");
        return false;
    }

    template<typename Alt>
    bool ensureTagPresent(Context& ctx) const
    {
        if (!tagFor<Alt>().empty()) {
            return true;
        }
        using Key = SchemaErrorKeyTag<Schema, VariantType, Alt, struct VariantAltMissing>;
        addSchemaErrorOnce<Key>(ctx, "(Schema error) Variant alternative missing tag mapping.");
        return false;
    }

    template<typename Alt>
    std::string_view tagFor() const
    {
        auto it = typeToIndex_.find(getTypeId<Alt>());
        if (it == typeToIndex_.end()) {
            return {};
        }
        return alts_[it->second].tag;
    }

    const Alternative* findByTag(const std::string& tag) const
    {
        auto it = tagToIndex_.find(tag);
        if (it == tagToIndex_.end()) {
            return nullptr;
        }
        return &alts_[it->second];
    }

    void introspectAlternatives(IntrospectContext<Schema>& ctx, std::vector<AlternativeInfo>& out)
        const
    {
        for (const auto& alt : alts_) {
            out.push_back({alt.tag, alt.typeId});
            alt.introspect(ctx);
        }
    }

    static constexpr std::string_view name() { return getVariantName<Schema, VariantType>(); }
    static constexpr std::string_view discriminator()
    {
        return getVariantDiscriminator<Schema, VariantType>();
    }
    static constexpr bool hasName()
    {
        return HasStaticNameMember<SchemaVariant<Schema, VariantType>>::value &&
               !getVariantName<Schema, VariantType>().empty();
    }
    static constexpr bool hasDiscriminator() { return true; }

private:
    template<typename Alt>
    static void decodeAlt(const Json::Value& src, VariantType& dst, DecodeContext<Schema>& ctx)
    {
        using ObjectSpec = SchemaObject<Schema, Alt>;
        const auto& objectDef = getObjectDef<Schema, Alt>();
        Alt alt{};
        objectDef.getImpl().decodeFields(src, alt, ctx);
        dst = std::move(alt);
    }

    template<typename Alt>
    static void introspectAlt(IntrospectContext<Schema>& ctx)
    {
        introspectType<Schema, Alt>(ctx);
    }

    std::vector<Alternative> alts_;
    std::unordered_map<std::string, std::size_t> tagToIndex_;
    std::unordered_map<TypeId, std::size_t> typeToIndex_;
};

template<typename Schema, typename T>
auto& getVariantDef()
{
    using Type = SchemaVariant<Schema, T>;
    static Type instance{};
    return instance;
}

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
            using ObjectSpec = SchemaObject<Schema, T>;
            static_assert(
                std::is_base_of_v<Object<Schema, T>, ObjectSpec>,
                "Schema::Object<T> must inherit from aison::Object<Schema, T>.");
            if constexpr (getIntrospectEnabled<Schema>()) {
                (void)getObjectName<Schema, T>();
            }
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
        using VariantSpec = SchemaVariant<Schema, std::variant<Ts...>>;
        static_assert(
            std::is_base_of_v<Variant<Schema, std::variant<Ts...>>, VariantSpec>,
            "Schema::Variant<V> must inherit from aison::Variant<Schema, V>.");
        static_assert(sizeof...(Ts) > 0, "std::variant must have at least one alternative.");
        if constexpr (getIntrospectEnabled<Schema>()) {
            (void)getVariantName<Schema, std::variant<Ts...>>();
        }
        (void)getVariantDiscriminator<Schema, std::variant<Ts...>>();
        // Each alternative must have an object mapping.
        (VariantAltCheck<Schema, Ts>::check(), ...);
    }
};

template<typename Schema, typename T>
constexpr void validateVariantSpec()
{
    using Type = std::decay_t<T>;
    static_assert(
        IsVariant<Type>::value,
        "Schema::Variant<T> must map a std::variant of object-mapped alternatives.");
    VariantValidator<Schema, Type>::validate();
}

template<typename Schema, typename T, typename ObjectDef>
bool validateObject(Context& ctx, const ObjectDef& objectDef)
{
    validateObjectSpec<Schema, T>();
    (void)ctx;
    (void)objectDef;
    return true;
}

template<typename Schema, typename T, typename EnumDef>
bool validateEnum(Context& ctx, const EnumDef& enumDef)
{
    validateEnumSpec<Schema, T>();
    (void)ctx;
    (void)enumDef;
    return true;
}

template<typename Schema, typename T, typename CustomDef>
bool validateCustom(Context& ctx, const CustomDef& customDef)
{
    validateCustomSpec<Schema, T>();
    (void)ctx;
    (void)customDef;
    return true;
}

template<typename Schema, typename Variant, typename VariantDef>
bool validateVariant(Context& ctx, const VariantDef& variantDef)
{
    using Type = std::decay_t<Variant>;
    validateVariantSpec<Schema, Type>();
    (void)ctx;
    (void)variantDef;
    return true;
}

// == Custom ==

template<typename Schema, typename T>
class CustomImpl
{
public:
    static constexpr std::string_view name() { return detail::getCustomName<Schema, T>(); }
    static constexpr bool hasName()
    {
        return HasStaticNameMember<SchemaCustom<Schema, T>>::value &&
               !detail::getCustomName<Schema, T>().empty();
    }
};

template<typename Schema, typename T>
auto& getCustomDef()
{
    static SchemaCustom<Schema, T> instance{};
    return instance;
}

template<typename Schema, typename T>
constexpr void validateCustomSpec()
{
    using Type = std::decay_t<T>;
    static_assert(
        HasCustomTag<Schema, Type>::value,
        "No schema custom mapping for this type. Define `template<> struct Schema::Custom<T> : "
        "aison::Custom<Schema, T>`.");
    static_assert(
        std::is_base_of_v<aison::Custom<Schema, Type>, SchemaCustom<Schema, Type>>,
        "Schema::Custom<T> must inherit from aison::Custom<Schema, T>.");
    if constexpr (getIntrospectEnabled<Schema>()) {
        (void)getCustomName<Schema, Type>();
    }
}

// == Context ==

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
    const std::vector<Error>& errors() const { return errors_; }

    std::vector<Error> takeErrors() { return std::move(errors_); }

private:
    std::vector<PathSegment> pathStack_;
    std::vector<Error> errors_;
};

template<typename Schema>
class EncodeContext : public Context
{
public:
    using Config = typename Schema::ConfigType;

    EncodeContext(const Config& cfg)
        : config_(cfg)
    {
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

template<typename Schema>
class IntrospectContext : public Context
{
public:
    IntrospectContext()
    {
        static_assert(
            getIntrospectEnabled<Schema>(),
            "IntrospectContext<Schema> cannot be used when Schema::enableIntrospect is false.");
    }

    bool isVisited(TypeId id) const { return types_.count(id) > 0; }
    void markVisited(TypeId id) { types_[id] = UnknownInfo{}; }
    void add(TypeId id, TypeInfo info) { types_[id] = std::move(info); }

    IntrospectResult takeResult()
    {
        IntrospectResult result;
        result.errors = takeErrors();
        result.types = std::move(types_);
        return result;
    }

private:
    std::unordered_map<TypeId, TypeInfo> types_;
};

// == Encode / Decode ==

template<typename Schema, typename T>
void encodeValue(const T& src, Json::Value& dst, EncodeContext<Schema>& ctx)
{
    if constexpr (HasCustomTag<Schema, T>::value) {
        using CustomSpec = SchemaCustom<Schema, T>;
        static_assert(
            HasCustomEncode<Schema, CustomSpec, T>::value,
            "Schema::Custom<T> must implement encode(const T&, Json::Value&, EncodeContext&).");
        auto& custom = getCustomDef<Schema, T>();
        validateCustom<Schema, T>(ctx, custom);
        custom.encode(src, dst, ctx);

    } else if constexpr (HasObjectTag<Schema, T>::value) {
        const auto& def = getObjectDef<Schema, T>();
        validateObject<Schema, T>(ctx, def);
        def.getImpl().encodeFields(src, dst, ctx);

    } else if constexpr (HasEnumTag<Schema, T>::value) {
        using EnumSpec = SchemaEnum<Schema, T>;
        auto& def = getEnumDef<Schema, T>();
        validateEnum<Schema, T>(ctx, def);
        auto* str = def.getImpl().find(src);
        if (str) {
            dst = *str;
        } else {
            ctx.addError(
                "Unhandled enum value during encode (underlying = " +
                std::to_string(std::underlying_type_t<T>(src)) + ").");
        }

    } else if constexpr (HasVariantTag<Schema, T>::value) {
        auto& var = getVariantDef<Schema, T>();
        auto& impl = var.getImpl();
        if (!validateVariant<Schema, T>(ctx, var) || !impl.validateCompleteness(ctx)) {
            return;
        }

        dst = Json::objectValue;
        std::visit(
            [&](const auto& alt) {
                using Alt = std::decay_t<decltype(alt)>;
                if (!impl.template ensureTagPresent<Alt>(ctx)) {
                    return;
                }
                const auto tag = impl.template tagFor<Alt>();
                auto& obj = getObjectDef<Schema, Alt>();
                obj.getImpl().encodeFields(alt, dst, ctx);
                dst[std::string(impl.discriminator())] = std::string(tag);
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
            auto guard = ctx.guard(index++);
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

// Decode

template<typename Schema, typename T>
void decodeValue(const Json::Value& src, T& dst, DecodeContext<Schema>& ctx)
{
    if constexpr (HasCustomTag<Schema, T>::value) {
        using CustomSpec = SchemaCustom<Schema, T>;
        static_assert(
            HasCustomDecode<Schema, CustomSpec, T>::value,
            "Schema::Custom<T> must implement decode(const Json::Value&, T&, DecodeContext&).");
        auto& custom = getCustomDef<Schema, T>();
        validateCustom<Schema, T>(ctx, custom);
        custom.decode(src, dst, ctx);

    } else if constexpr (HasObjectTag<Schema, T>::value) {
        if (!src.isObject()) {
            ctx.addError("Expected object.");
            return;
        }

        const auto& obj = getObjectDef<Schema, T>();
        validateObject<Schema, T>(ctx, obj);
        obj.getImpl().decodeFields(src, dst, ctx);

    } else if constexpr (HasEnumTag<Schema, T>::value) {
        if (!src.isString()) {
            ctx.addError("Expected string for enum.");
            return;
        }

        auto& def = getEnumDef<Schema, T>();
        validateEnum<Schema, T>(ctx, def);
        auto* value = def.getImpl().find(src.asString());
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

// Variant decoding

template<typename Schema, typename... Ts>
struct VariantDecoder<Schema, std::variant<Ts...>> {
    using VariantType = std::variant<Ts...>;

    static void decode(const Json::Value& src, VariantType& dst, DecodeContext<Schema>& ctx)
    {
        auto& var = getVariantDef<Schema, VariantType>();
        auto& def = var.getImpl();
        if (!validateVariant<Schema, VariantType>(ctx, var) || !def.validateCompleteness(ctx)) {
            return;
        }

        if (!src.isObject()) {
            ctx.addError("Expected object for variant.");
            return;
        }

        const auto tag = std::string(def.discriminator());
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

        if (const auto* alt = def.findByTag(tagValue)) {
            alt->decode(src, dst, ctx);
        } else {
            auto discGuard = ctx.guard(tag);
            ctx.addError("Unknown discriminator value for variant.");
        }
    }
};

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

template<typename Schema, typename T>
void introspectFieldThunk(IntrospectContext<Schema>& ctx)
{
    introspectType<Schema, T>(ctx);
}

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

// Traits

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
struct HasEnumTag<Schema, T, std::void_t<typename SchemaEnum<Schema, T>::EnumTag>>
    : std::true_type {};

template<typename Schema, typename T, typename>
struct HasObjectTag : std::false_type {};

template<typename Schema, typename T>
struct HasObjectTag<Schema, T, std::void_t<typename SchemaObject<Schema, T>::ObjectTag>>
    : std::true_type {};

template<typename Schema, typename T, typename>
struct HasVariantTag : std::false_type {};

template<typename Schema, typename T>
struct HasVariantTag<Schema, T, std::void_t<typename SchemaVariant<Schema, T>::VariantTag>>
    : std::true_type {};

template<typename Schema, typename T, typename>
struct HasCustomTag : std::false_type {};

template<typename Schema, typename T>
struct HasCustomTag<Schema, T, std::void_t<typename SchemaCustom<Schema, T>::CustomTag>>
    : std::true_type {};

template<typename Schema, typename CustomSpec, typename T, typename>
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

template<typename Schema, typename CustomSpec, typename T, typename>
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

// == Introspection ==

template<typename Schema, typename V>
struct VariantInfoBuilder;

template<typename Schema, typename... Ts>
struct VariantInfoBuilder<Schema, std::variant<Ts...>> {
    static void build(IntrospectContext<Schema>& ctx, std::vector<AlternativeInfo>& alts)
    {
        auto& var = getVariantDef<Schema, std::variant<Ts...>>();
        var.getImpl().introspectAlternatives(ctx, alts);
    }
};

template<typename Schema, typename Type>
void introspectType(IntrospectContext<Schema>& ctx)
{
    using U = std::decay_t<Type>;
    const TypeId id = getTypeId<U>();

    if (ctx.isVisited(id)) {
        return;
    }

    ctx.markVisited(id);

    if constexpr (IsOptional<U>::value) {
        using Inner = typename U::value_type;
        ctx.add(id, OptionalInfo{getTypeId<Inner>()});
        introspectType<Schema, Inner>(ctx);

    } else if constexpr (IsVector<U>::value) {
        using Inner = typename U::value_type;
        ctx.add(id, VectorInfo{getTypeId<Inner>()});
        introspectType<Schema, Inner>(ctx);

    } else if constexpr (HasCustomTag<Schema, U>::value) {
        ctx.add(id, CustomInfo{schemaTypeName<Schema, U>()});

    } else if constexpr (HasObjectTag<Schema, U>::value) {
        auto& def = getObjectDef<Schema, U>();

        ObjectInfo info;
        info.name = schemaTypeName<Schema, U>();

        for (const auto& field : def.getImpl().fields()) {
            FieldInfo fi;
            fi.name = field.name;
            fi.type = field.typeId;
            fi.isRequired = field.isRequired;
            info.fields.push_back(std::move(fi));
            field.introspect(ctx);
        }
        ctx.add(id, std::move(info));

    } else if constexpr (HasEnumTag<Schema, U>::value) {
        auto& def = getEnumDef<Schema, U>();

        EnumInfo info;
        info.name = schemaTypeName<Schema, U>();

        for (const auto& entry : def.getImpl().entries()) {
            info.values.push_back(entry.second);
        }

        ctx.add(id, std::move(info));

    } else if constexpr (HasVariantTag<Schema, U>::value) {
        auto& def = getVariantDef<Schema, U>();
        if (!validateVariant<Schema, U>(ctx, def) || !def.getImpl().validateCompleteness(ctx)) {
            return;
        }

        VariantInfo info;
        info.name = schemaTypeName<Schema, U>();
        info.discriminator = def.getImpl().discriminator();

        VariantInfoBuilder<Schema, U>::build(ctx, info.alternatives);
        ctx.add(id, std::move(info));

    } else if constexpr (std::is_same_v<U, bool>) {
        ctx.add(id, BoolInfo{});

    } else if constexpr (std::is_same_v<U, std::string>) {
        ctx.add(id, StringInfo{});

    } else if constexpr (std::is_integral_v<U>) {
        ctx.add(id, IntegralInfo{int(sizeof(U)), std::is_signed_v<U>});

    } else if constexpr (std::is_floating_point_v<U>) {
        ctx.add(id, FloatingInfo{int(sizeof(U))});

    } else {
        static_assert(
            DependentFalse<U>::value,
            "Unsupported type. Define Schema::Object / Enum / Variant / Custom for this type.");
    }
}

}  // namespace aison::detail

namespace aison {

// Definitions (CRTP base types) /////////////////////////////////////////////////////////////////

template<typename Derived, typename Config = EmptyConfig>
struct Schema {
    using SchemaTag = void;
    using ConfigType = Config;
};

template<typename Schema, typename T>
struct Object {
    using ObjectTag = void;
    using Impl = detail::ObjectImpl<Schema, T>;

    Object() { detail::validateObjectSpec<Schema, T>(); }

    template<typename U>
    void add(U T::* member, std::string_view name)
    {
        impl_.add(member, name);
    }

    Impl& getImpl() { return impl_; }
    const Impl& getImpl() const { return impl_; }

private:
    Impl impl_;
};

template<typename Schema, typename T>
struct Enum {
    using EnumTag = void;
    using Impl = detail::EnumImpl<Schema, T>;

    Enum() { detail::validateEnumSpec<Schema, T>(); }

    void add(T value, std::string_view name) { impl_.add(value, name); }

    Impl& getImpl() { return impl_; }
    const Impl& getImpl() const { return impl_; }

private:
    Impl impl_;
};

template<typename Schema, typename T>
struct Variant {
    using VariantTag = void;
    using Impl = detail::VariantImpl<Schema, T>;

    Variant() { detail::validateVariantSpec<Schema, T>(); }

    template<typename Alt>
    void add(std::string_view tag)
    {
        impl_.template add<Alt>(tag);
    }

    Impl& getImpl() { return impl_; }
    const Impl& getImpl() const { return impl_; }

private:
    Impl impl_;
};

template<typename Schema, typename T>
struct Custom {
    using CustomTag = void;
    using ConfigType = typename Schema::ConfigType;
    using EncodeContext = detail::EncodeContext<Schema>;
    using DecodeContext = detail::DecodeContext<Schema>;

    using Impl = detail::CustomImpl<Schema, T>;

    Custom() { detail::validateCustomSpec<Schema, T>(); }

    Impl& getImpl() { return impl_; }
    const Impl& getImpl() const { return impl_; }

private:
    Impl impl_;
};

// Definitions (API functions) ///////////////////////////////////////////////////////////////////

template<typename T>
TypeId getTypeId()
{
    static int id = 0x71931d;
    return &id;
}

template<typename Schema, typename T>
Result encode(const T& src, Json::Value& dst, const typename Schema::ConfigType& config)
{
    detail::validateSchemaDefinition<Schema>();
    detail::EncodeContext<Schema> ctx(config);
    ctx.encode(src, dst);
    return Result{ctx.takeErrors()};
}

template<typename Schema, typename T>
Result decode(const Json::Value& src, T& dst, const typename Schema::ConfigType& config)
{
    detail::validateSchemaDefinition<Schema>();
    detail::DecodeContext<Schema> ctx(config);
    ctx.decode(src, dst);
    return Result{ctx.takeErrors()};
}

template<typename Schema, typename... Ts>
IntrospectResult introspect()
{
    detail::validateSchemaDefinition<Schema>();
    detail::IntrospectContext<Schema> ctx;
    (detail::introspectType<Schema, Ts>(ctx), ...);
    return ctx.takeResult();
}

}  // namespace aison
