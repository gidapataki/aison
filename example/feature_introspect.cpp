#include <aison/aison.h>

#include <cstdint>
#include <iostream>
#include <string>

#include "types.h"

namespace example {
namespace {

struct DemoSchema : aison::Schema<DemoSchema> {
    static constexpr bool enableIntrospect = true;
    template<typename T>
    struct Object;
    template<typename T>
    struct Enum;
    template<typename T>
    struct Variant;
};

template<>
struct DemoSchema::Enum<Flavor> : aison::Enum<DemoSchema, Flavor> {
    Enum()
    {
        name("Flavor");
        add(Flavor::kVanilla, "vanilla");
        add(Flavor::kChocolate, "chocolate");
    }
};

template<>
struct DemoSchema::Object<Topping> : aison::Object<DemoSchema, Topping> {
    Object()
    {
        name("Topping");
        add(&Topping::name, "name");
        add(&Topping::crunchy, "crunchy");
    }
};

template<>
struct DemoSchema::Variant<Dessert> : aison::Variant<DemoSchema, Dessert> {
    Variant()
    {
        name("Dessert");
        discriminator("kind");
    }
};

template<>
struct DemoSchema::Object<Cone> : aison::Object<DemoSchema, Cone> {
    Object()
    {
        name("Cone");
        add(&Cone::scoops, "scoops");
        add(&Cone::flavor, "flavor");
        add(&Cone::toppings, "toppings");
    }
};

template<>
struct DemoSchema::Object<Cup> : aison::Object<DemoSchema, Cup> {
    Object()
    {
        name("Cup");
        add(&Cup::sprinkles, "sprinkles");
        add(&Cup::drizzle, "drizzle");
    }
};

template<>
struct DemoSchema::Object<Order> : aison::Object<DemoSchema, Order> {
    Object()
    {
        name("Order");
        add(&Order::customer, "customer");
        add(&Order::dessert, "dessert");
        add(&Order::extras, "extras");
    }
};

//

std::string typeIdToString(aison::TypeId id)
{
    return "#" + std::to_string(reinterpret_cast<std::uintptr_t>(id));
}

const aison::TypeInfo* lookup(const aison::IntrospectResult& isp, aison::TypeId id)
{
    auto it = isp.types.find(id);
    return it == isp.types.end() ? nullptr : &it->second;
}

std::string renderType(const aison::IntrospectResult& isp, aison::TypeId id)
{
    const auto* info = lookup(isp, id);
    const auto fallback = typeIdToString(id);
    if (!info) {
        return fallback;
    }

    auto nameOrId = [&]() -> std::string { return info->name.empty() ? fallback : info->name; };

    switch (info->cls) {
        case aison::TypeClass::Bool:
            return "bool";
        case aison::TypeClass::Integral: {
            const auto& data = std::get<aison::IntegralInfo>(info->data);
            return (data.isSigned ? "int" : "uint") + std::to_string(data.size * 8);
        }
        case aison::TypeClass::Floating: {
            const auto& data = std::get<aison::FloatingInfo>(info->data);
            if (data.size == 4) return "float";
            if (data.size == 8) return "double";
            return "float" + std::to_string(data.size);
        }
        case aison::TypeClass::String:
            return "string";
        case aison::TypeClass::Enum:
            return "enum(" + nameOrId() + ")";
        case aison::TypeClass::Object:
            return "object(" + nameOrId() + ")";
        case aison::TypeClass::Custom:
            return "custom(" + nameOrId() + ")";
        case aison::TypeClass::Optional: {
            const auto& data = std::get<aison::OptionalInfo>(info->data);
            return "optional<" + renderType(isp, data.value) + ">";
        }
        case aison::TypeClass::Vector: {
            const auto& data = std::get<aison::VectorInfo>(info->data);
            return "vector<" + renderType(isp, data.value) + ">";
        }
        case aison::TypeClass::Variant: {
            const auto& data = std::get<aison::VariantInfo>(info->data);
            std::string out = "variant(" + nameOrId() + ")<";
            for (std::size_t i = 0; i < data.alternatives.size(); ++i) {
                if (i) out += " | ";
                out += renderType(isp, data.alternatives[i].type);
            }
            out += ">";
            return out;
        }
        default:
            return "unknown";
    }
}

void dump(const aison::IntrospectResult& isp)
{
    for (const auto& entry : isp.types) {
        const auto& info = entry.second;
        const auto displayName = info.name.empty() ? typeIdToString(info.typeId) : info.name;
        switch (info.cls) {
            case aison::TypeClass::Object: {
                const auto& obj = std::get<aison::ObjectInfo>(info.data);
                std::cout << "object: " << displayName << "\n";
                for (const auto& f : obj.fields) {
                    std::cout << " - " << f.name << ": " << renderType(isp, f.type) << "\n";
                }
                std::cout << "\n";
                break;
            }
            case aison::TypeClass::Variant: {
                const auto& var = std::get<aison::VariantInfo>(info.data);
                std::cout << "variant: " << displayName << "\n";
                for (const auto& alt : var.alternatives) {
                    std::cout << " - tag=\"" << alt.name << "\" type="
                              << renderType(isp, alt.type) << "\n";
                }
                std::cout << "\n";
                break;
            }
            case aison::TypeClass::Enum: {
                const auto& en = std::get<aison::EnumInfo>(info.data);
                std::cout << "enum: " << displayName << " [";
                for (std::size_t i = 0; i < en.values.size(); ++i) {
                    if (i) std::cout << ", ";
                    std::cout << en.values[i];
                }
                std::cout << "]\n";
                break;
            }
            default:
                break;
        }
    }
}

}  // namespace

void introspectExample1()
{
    Cone cone;
    Json::Value root;
    aison::encode<DemoSchema>(cone, root);

    std::cout << root.toStyledString() << "\n";

    auto isp = aison::introspect<DemoSchema, Flavor>();

    // aison::introspection<DemoSchema>().collect<Flavor, ...>

#if 1
    auto isp2 = aison::introspect<DemoSchema, Flavor, Order>();
    dump(isp2);
    std::cout << "--\n";
#endif

    dump(isp);
}

}  // namespace example
