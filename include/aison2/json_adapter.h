#pragma once

#include <json/json.h>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "aison2/aison2.h"

namespace aison2 {
namespace json {

namespace detail {

// Tuple iteration helpers ----------------------------------------------------
template<std::size_t I = 0, class Func, class... Ts>
constexpr void forEach(const std::tuple<Ts...>& tuple, Func&& func)
{
    if constexpr (I < sizeof...(Ts)) {
        func(std::get<I>(tuple));
        forEach<I + 1>(tuple, std::forward<Func>(func));
    }
}

template<class T, class Tuple>
struct FindInTuple;

template<class T, class... Ts>
struct FindInTuple<T, std::tuple<Ts...>> {
    static constexpr std::size_t value =
        aison2::detail::IndexOf<T, aison2::detail::TypeList<typename Ts::Type...>>::value;
};

template<class T, class DefsTuple, std::size_t I = 0>
constexpr const auto& getDef(const DefsTuple& defs)
{
    if constexpr (I >= std::tuple_size_v<DefsTuple>) {
        static_assert(I < std::tuple_size_v<DefsTuple>, "Definition not found");
        return defs;  // Unreachable, silences compiler.
    } else {
        using Def = std::tuple_element_t<I, DefsTuple>;
        if constexpr (
            std::is_same_v<typename Def::Type, T> && !aison2::detail::DefTraits<Def>::isDeclare)
        {
            return std::get<I>(defs);
        } else {
            return getDef<T, DefsTuple, I + 1>(defs);
        }
    }
}

// Encoder context ------------------------------------------------------------
template<class Schema>
class EncodeContext
{
public:
    explicit EncodeContext(const Schema& schema)
        : schema_(schema)
    {}

    template<class T>
    Json::Value encode(const T& value) const
    {
        return encodeValue(value);
    }

private:
    template<class T>
    Json::Value encodeValue(const T& value) const
    {
        if constexpr (std::is_same_v<T, std::string>) {
            return Json::Value(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return Json::Value(value);
        } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            return Json::Value(static_cast<Json::Int64>(value));
        } else if constexpr (std::is_floating_point_v<T>) {
            return Json::Value(value);
        } else if constexpr (isOptional<T>::value) {
            if (!value.has_value()) {
                return Json::Value();  // null
            }
            return encodeValue(*value);
        } else if constexpr (isVector<T>::value) {
            Json::Value arr(Json::arrayValue);
            for (const auto& elem : value) {
                arr.append(encodeValue(elem));
            }
            return arr;
        } else if constexpr (isVariant<T>::value) {
            return encodeVariant(value);
        } else {
            return encodeWithSchema(value);
        }
    }

    template<class Def, class T>
    struct IsObjectDef : std::false_type {};
    template<class TObj, class Fields>
    struct IsObjectDef<aison2::detail::ObjectDef<TObj, Fields>, TObj> : std::true_type {};

    template<class Def, class T>
    struct IsEnumDef : std::false_type {};
    template<class TEnum, class Values>
    struct IsEnumDef<aison2::detail::EnumDef<TEnum, Values>, TEnum> : std::true_type {};

    template<class Def, class T>
    struct IsVariantDef : std::false_type {};
    template<class TVariant, class Alternatives>
    struct IsVariantDef<aison2::detail::VariantDef<TVariant, Alternatives>, TVariant>
        : std::true_type {};

    template<class Def, class T>
    struct IsCustomDef : std::false_type {};
    template<class TCustom, class EncoderFn, class DecoderFn>
    struct IsCustomDef<aison2::detail::CustomDef<TCustom, EncoderFn, DecoderFn>, TCustom>
        : std::true_type {};

    template<class T>
    Json::Value encodeWithSchema(const T& value) const
    {
        const auto& def = getDef<T>(schema_.definitions());
        using DefType = std::decay_t<decltype(def)>;
        if constexpr (IsObjectDef<DefType, T>::value) {
            Json::Value obj(Json::objectValue);
            detail::forEach(def.fields.fields, [&](const auto& field) {
                obj[field.name] = encodeValue(value.*(field.ptr));
            });
            return obj;
        } else if constexpr (IsEnumDef<DefType, T>::value) {
            Json::Value str;
            bool found = false;
            detail::forEach(def.values.values, [&](const auto& ev) {
                if (!found && ev.value == value) {
                    str = Json::Value(ev.name);
                    found = true;
                }
            });
            assert(found && "Enum value not found");
            return str;
        } else if constexpr (IsVariantDef<DefType, T>::value) {
            return encodeVariantWithDef(value, def);
        } else if constexpr (IsCustomDef<DefType, T>::value) {
            return def.encoder(value, *this);
        } else {
            static_assert(!std::is_same_v<T, T>, "Unsupported def type");
        }
    }

    template<class T>
    Json::Value encodeVariant(const T& value) const
    {
        const auto& def = getDef<T>(schema_.definitions());
        return encodeVariantWithDef(value, def);
    }

    template<class T, class VariantDef>
    Json::Value encodeVariantWithDef(const T& value, const VariantDef& def) const
    {
        Json::Value obj(Json::objectValue);
        std::visit(
            [&](const auto& alt) {
                using AltType = std::decay_t<decltype(alt)>;
                constexpr std::size_t idx = aison2::detail::IndexOf<
                    AltType, typename aison2::detail::VariantTypeInfo<T>::AlternativesList>::value;
                const char* tag = std::get<idx>(def.alternatives.alternatives).tag;
                obj[def.tag] = Json::Value(tag);
                obj["value"] = encodeValue(alt);
            },
            value);
        return obj;
    }

    template<class U>
    struct isOptional : std::false_type {};
    template<class V>
    struct isOptional<std::optional<V>> : std::true_type {};

    template<class U>
    struct isVector : std::false_type {};
    template<class V, class Alloc>
    struct isVector<std::vector<V, Alloc>> : std::true_type {};

    template<class U>
    struct isVariant : std::false_type {};
    template<class... Alts>
    struct isVariant<std::variant<Alts...>> : std::true_type {};

    const Schema& schema_;
};

// Decoder context ------------------------------------------------------------
template<class Schema>
class DecodeContext
{
public:
    explicit DecodeContext(const Schema& schema)
        : schema_(schema)
    {}

    template<class T>
    T decode(const Json::Value& value) const
    {
        return decodeValue<T>(value);
    }

private:
    template<class Def, class T>
    struct IsObjectDef : std::false_type {};
    template<class TObj, class Fields>
    struct IsObjectDef<aison2::detail::ObjectDef<TObj, Fields>, TObj> : std::true_type {};

    template<class Def, class T>
    struct IsEnumDef : std::false_type {};
    template<class TEnum, class Values>
    struct IsEnumDef<aison2::detail::EnumDef<TEnum, Values>, TEnum> : std::true_type {};

    template<class Def, class T>
    struct IsVariantDef : std::false_type {};
    template<class TVariant, class Alternatives>
    struct IsVariantDef<aison2::detail::VariantDef<TVariant, Alternatives>, TVariant>
        : std::true_type {};

    template<class Def, class T>
    struct IsCustomDef : std::false_type {};
    template<class TCustom, class EncoderFn, class DecoderFn>
    struct IsCustomDef<aison2::detail::CustomDef<TCustom, EncoderFn, DecoderFn>, TCustom>
        : std::true_type {};

    template<class T>
    T decodeValue(const Json::Value& value) const
    {
        if constexpr (std::is_same_v<T, std::string>) {
            assert(value.isString());
            return value.asString();
        } else if constexpr (std::is_same_v<T, bool>) {
            assert(value.isBool());
            return value.asBool();
        } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            assert(value.isInt64() || value.isUInt64());
            return static_cast<T>(value.asInt64());
        } else if constexpr (std::is_floating_point_v<T>) {
            assert(value.isDouble());
            return static_cast<T>(value.asDouble());
        } else if constexpr (isOptional<T>::value) {
            if (value.isNull()) {
                return std::nullopt;
            }
            using Inner = typename T::value_type;
            return decodeValue<Inner>(value);
        } else if constexpr (isVector<T>::value) {
            assert(value.isArray());
            T out;
            for (const auto& elem : value) {
                out.push_back(decodeValue<typename T::value_type>(elem));
            }
            return out;
        } else if constexpr (isVariant<T>::value) {
            return decodeVariant<T>(value);
        } else {
            return decodeWithSchema<T>(value);
        }
    }

    template<class T>
    T decodeWithSchema(const Json::Value& value) const
    {
        const auto& def = getDef<T>(schema_.definitions());
        using DefType = std::decay_t<decltype(def)>;
        if constexpr (IsObjectDef<DefType, T>::value) {
            T out{};
            detail::forEach(def.fields.fields, [&](const auto& field) {
                out.*(field.ptr) = decodeValue<typename std::decay_t<decltype(out.*(field.ptr))>>(
                    value[field.name]);
            });
            return out;
        } else if constexpr (IsEnumDef<DefType, T>::value) {
            assert(value.isString());
            const std::string name = value.asString();
            bool found = false;
            T result{};
            detail::forEach(def.values.values, [&](const auto& ev) {
                if (!found && name == ev.name) {
                    result = ev.value;
                    found = true;
                }
            });
            assert(found && "Enum name not found");
            return result;
        } else if constexpr (IsVariantDef<DefType, T>::value) {
            return decodeVariantWithDef<T>(value, def);
        } else if constexpr (IsCustomDef<DefType, T>::value) {
            return def.decoder(value, *this);
        } else {
            static_assert(!std::is_same_v<T, T>, "Unsupported def type");
        }
    }

    template<class T>
    T decodeVariant(const Json::Value& value) const
    {
        const auto& def = getDef<T>(schema_.definitions());
        return decodeVariantWithDef<T>(value, def);
    }

    template<class T, class VariantDef>
    T decodeVariantWithDef(const Json::Value& value, const VariantDef& def) const
    {
        assert(value.isObject());
        const Json::Value& tagNode = value[def.tag];
        assert(tagNode.isString());
        const std::string tag = tagNode.asString();
        const Json::Value& payload = value["value"];

        T result{};
        bool matched = false;
        detail::forEach(def.alternatives.alternatives, [&](const auto& alt) {
            if (matched) {
                return;
            }
            if (tag == alt.tag) {
                using AltType = typename std::decay_t<decltype(alt)>::Type;
                result = T{decodeValue<AltType>(payload)};
                matched = true;
            }
        });
        assert(matched && "Variant tag not found");
        return result;
    }

    template<class U>
    struct isOptional : std::false_type {};
    template<class V>
    struct isOptional<std::optional<V>> : std::true_type {};

    template<class U>
    struct isVector : std::false_type {};
    template<class V, class Alloc>
    struct isVector<std::vector<V, Alloc>> : std::true_type {};

    template<class U>
    struct isVariant : std::false_type {};
    template<class... Alts>
    struct isVariant<std::variant<Alts...>> : std::true_type {};

    const Schema& schema_;
};

}  // namespace detail

template<class Schema, class T>
Json::Value encode(const Schema& schema, const T& value)
{
    detail::EncodeContext<Schema> ctx(schema);
    return ctx.encode(value);
}

template<class T, class Schema>
T decode(const Schema& schema, const Json::Value& value)
{
    detail::DecodeContext<Schema> ctx(schema);
    return ctx.template decode<T>(value);
}

}  // namespace json
}  // namespace aison2
