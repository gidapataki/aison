#include <aison/aison.h>

#include <cstdint>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

// Minimal demo types ---------------------------------------------------------

enum class Flavor { kVanilla, kChocolate };

struct Topping {
    std::string name;
    bool crunchy = false;
};

struct Cone {
    int scoops = 1;
    Flavor flavor = Flavor::kVanilla;
    std::vector<Topping> toppings;
};

struct Cup {
    bool sprinkles = false;
    std::optional<Topping> drizzle;
};

using Dessert = std::variant<Cone, Cup>;

struct Order {
    std::string customer;
    std::optional<Dessert> dessert;
    std::vector<Topping> extras;
};

// Schema ---------------------------------------------------------------------

struct DemoSchema : aison::Schema<DemoSchema> {
    static constexpr bool enableIntrospection = true;
    template<typename T>
    struct Object;
    template<typename T>
    struct Enum;
};

template<>
struct DemoSchema::Enum<Flavor> : aison::Enum<DemoSchema, Flavor> {
    Enum()
    {
        add(Flavor::kVanilla, "vanilla");
        add(Flavor::kChocolate, "chocolate");
    }
};

template<>
struct DemoSchema::Object<Topping> : aison::Object<DemoSchema, Topping> {
    Object()
    {
        add(&Topping::name, "name");
        add(&Topping::crunchy, "crunchy");
    }
};

template<>
struct DemoSchema::Object<Cone> : aison::Object<DemoSchema, Cone> {
    Object()
    {
        discriminator("cone", "kind");
        add(&Cone::scoops, "scoops");
        add(&Cone::flavor, "flavor");
        add(&Cone::toppings, "toppings");
    }
};

template<>
struct DemoSchema::Object<Cup> : aison::Object<DemoSchema, Cup> {
    Object()
    {
        discriminator("cup", "kind");
        add(&Cup::sprinkles, "sprinkles");
        add(&Cup::drizzle, "drizzle");
    }
};

template<>
struct DemoSchema::Object<Order> : aison::Object<DemoSchema, Order> {
    Object()
    {
        add(&Order::customer, "customer");
        add(&Order::dessert, "dessert");
        add(&Order::extras, "extras");
    }
};

// Introspection utility ------------------------------------------------------

void printType(const aison::detail::TypeInfo& info, int indent = 0)
{
    auto pad = [&](int extra = 0) {
        for (int i = 0; i < indent + extra; ++i) std::cout << ' ';
    };

    auto basicToStr = [](aison::detail::BasicType b) {
        using BT = aison::detail::BasicType;
        switch (b) {
            case BT::Bool:
                return "bool";
            case BT::Int8:
                return "int8";
            case BT::UInt8:
                return "uint8";
            case BT::Int16:
                return "int16";
            case BT::UInt16:
                return "uint16";
            case BT::Int32:
                return "int32";
            case BT::UInt32:
                return "uint32";
            case BT::Int64:
                return "int64";
            case BT::UInt64:
                return "uint64";
            case BT::Float:
                return "float";
            case BT::Double:
                return "double";
            case BT::String:
                return "string";
            case BT::Enum:
                return "enum";
            case BT::Object:
                return "object";
            case BT::Other:
                return "other";
            default:
                return "unknown";
        }
    };

    using FK = aison::detail::FieldKind;
    switch (info.kind) {
        case FK::Plain:
            pad();
            std::cout << basicToStr(info.basic);
            if (info.basic == aison::detail::BasicType::Enum ||
                info.basic == aison::detail::BasicType::Object)
            {
                std::cout << " (typeId=" << info.typeId << ")";
            }
            std::cout << "\n";
            break;
        case FK::Optional:
            pad();
            std::cout << "optional<\n";
            if (info.element) {
                printType(*info.element, indent + 2);
            }
            pad();
            std::cout << ">\n";
            break;
        case FK::Vector:
            pad();
            std::cout << "vector<\n";
            if (info.element) {
                printType(*info.element, indent + 2);
            }
            pad();
            std::cout << ">\n";
            break;
        case FK::Variant:
            pad();
            std::cout << "variant<\n";
            for (std::size_t i = 0; i < info.variantCount; ++i) {
                if (info.variants && info.variants[i]) {
                    printType(*info.variants[i], indent + 2);
                }
            }
            pad();
            std::cout << ">\n";
            break;
    }
}

std::string renderType(const aison::detail::TypeInfo& info)
{
    using namespace aison::detail;
    auto basicToStr = [](BasicType b) -> std::string {
        switch (b) {
            case BasicType::Bool:
                return "bool";
            case BasicType::Int8:
                return "int8";
            case BasicType::UInt8:
                return "uint8";
            case BasicType::Int16:
                return "int16";
            case BasicType::UInt16:
                return "uint16";
            case BasicType::Int32:
                return "int32";
            case BasicType::UInt32:
                return "uint32";
            case BasicType::Int64:
                return "int64";
            case BasicType::UInt64:
                return "uint64";
            case BasicType::Float:
                return "float";
            case BasicType::Double:
                return "double";
            case BasicType::String:
                return "string";
            case BasicType::Enum:
                return "enum";
            case BasicType::Object:
                return "object";
            case BasicType::Other:
                return "other";
            default:
                return "unknown";
        }
    };

    switch (info.kind) {
        case FieldKind::Plain: {
            std::string out = basicToStr(info.basic);
            if (info.basic == BasicType::Enum || info.basic == BasicType::Object) {
                out += "(typeId=" +
                       std::to_string(reinterpret_cast<std::uintptr_t>(info.typeId)) + ")";
            }
            return out;
        }
        case FieldKind::Optional:
            return "optional<" +
                   (info.element ? renderType(*info.element) : std::string("unknown")) + ">";
        case FieldKind::Vector:
            return "vector<" + (info.element ? renderType(*info.element) : std::string("unknown")) +
                   ">";
        case FieldKind::Variant: {
            std::string out = "variant<";
            for (std::size_t i = 0; i < info.variantCount; ++i) {
                if (i) out += " | ";
                out +=
                    info.variants && info.variants[i] ? renderType(*info.variants[i]) : "unknown";
            }
            out += ">";
            return out;
        }
    }
    return "unknown";
}

template<typename Schema, typename T, typename... Rest>
void dispatchObject(
    const void* typeId, std::set<const void*>& seenObjs, std::set<const void*>& seenEnums);

template<typename Schema>
void dispatchEnum(const void* typeId, std::set<const void*>& seenEnums)
{
    if (typeId == aison::detail::typeId<Flavor>()) {
        if (seenEnums.insert(typeId).second) {
            const auto& en = aison::detail::getSchemaObject<typename Schema::template Enum<Flavor>>();
            std::cout << "enum (typeId=" << reinterpret_cast<std::uintptr_t>(typeId) << "):";
            for (const auto& entry : en) {
                std::cout << " " << entry.second;
            }
            std::cout << "\n";
        }
    }
}

template<typename Schema, typename Root, typename... Objs>
void traverseType(
    const aison::detail::TypeInfo& info,
    std::set<const void*>& seenObjs,
    std::set<const void*>& seenEnums)
{
    using namespace aison::detail;
    switch (info.kind) {
        case FieldKind::Plain:
            if (info.basic == BasicType::Enum && info.typeId) {
                dispatchEnum<Schema>(info.typeId, seenEnums);
            } else if (info.basic == BasicType::Object && info.typeId) {
                dispatchObject<Schema, Root, Objs...>(info.typeId, seenObjs, seenEnums);
            }
            break;
        case FieldKind::Optional:
        case FieldKind::Vector:
            if (info.element) {
                traverseType<Schema, Root, Objs...>(*info.element, seenObjs, seenEnums);
            }
            break;
        case FieldKind::Variant:
            for (std::size_t i = 0; i < info.variantCount; ++i) {
                if (info.variants && info.variants[i]) {
                    traverseType<Schema, Root, Objs...>(*info.variants[i], seenObjs, seenEnums);
                }
            }
            break;
    }
}

template<typename Schema, typename Root, typename... Objs>
void dumpObject(std::set<const void*>& seenObjs, std::set<const void*>& seenEnums)
{
    const void* tid = aison::detail::typeId<Root>();
    if (seenObjs.count(tid)) {
        return;
    }
    seenObjs.insert(tid);

    const auto& obj = aison::detail::getSchemaObject<typename Schema::template Object<Root>>();
    std::cout << "type (typeId=" << reinterpret_cast<std::uintptr_t>(tid) << "):\n";
    for (const auto& field : obj.fields()) {
        std::cout << " - " << field.name << ": ";
        const auto* info = field.typeInfo;
        if (info) {
            std::cout << renderType(*info) << "\n";
            traverseType<Schema, Root, Objs...>(*info, seenObjs, seenEnums);
        }
    }
    std::cout << "\n";
}

template<typename Schema, typename T, typename... Rest>
void dispatchObject(
    const void* typeId, std::set<const void*>& seenObjs, std::set<const void*>& seenEnums)
{
    if (typeId == aison::detail::typeId<T>()) {
        dumpObject<Schema, T, Rest...>(seenObjs, seenEnums);
        return;
    }
    if constexpr (sizeof...(Rest) > 0) {
        dispatchObject<Schema, Rest...>(typeId, seenObjs, seenEnums);
    }
}

int main()
{
    std::set<const void*> seenObjs;
    std::set<const void*> seenEnums;

    // Start from the root and discover reachable types.
    dumpObject<DemoSchema, Order, Cone, Cup, Topping>(seenObjs, seenEnums);
    return 0;
}
