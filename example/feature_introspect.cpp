#include <aison/aison.h>

#include <cstdint>
#include <iostream>
#include <string>

#include "types.h"

namespace example {
namespace {

struct DemoSchema : aison::Schema<DemoSchema> {
    static constexpr bool enableIntrospection = true;
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

std::string renderType(const aison::TypeInfo* info)
{
    if (!info) return "unknown";
    auto typeIdStr = std::to_string(reinterpret_cast<std::uintptr_t>(info->typeId));
    auto renderNamed = [&](std::string_view kind) {
        if (info->name && *info->name) {
            return std::string(kind) + "(" + info->name + ")";
        }
        return std::string(kind) + "(typeId=" + typeIdStr + ")";
    };

    switch (info->cls) {
        case aison::TypeClass::Bool:
            return "bool";
        case aison::TypeClass::Integral: {
            auto size = static_cast<int>(info->data.integral.size);
            return (info->data.integral.isSigned ? "int" : "uint") + std::to_string(size * 8);
        }
        case aison::TypeClass::Floating: {
            auto size = info->data.floating.size;
            if (size == 4) return "float";
            if (size == 8) return "double";
            return "float" + std::to_string(size);
        }
        case aison::TypeClass::String:
            return "string";
        case aison::TypeClass::Enum:
            return renderNamed("enum");
        case aison::TypeClass::Object:
            return renderNamed("object");
        case aison::TypeClass::Custom:
            return renderNamed("custom");
        case aison::TypeClass::Optional:
            return "optional<" + renderType(info->data.optional.type) + ">";
        case aison::TypeClass::Vector:
            return "vector<" + renderType(info->data.vector.type) + ">";
        case aison::TypeClass::Variant: {
            std::string out = info->name && *info->name ? std::string("variant ") + info->name + "<"
                                                        : "variant<";
            for (std::size_t i = 0; i < info->data.variant.count; ++i) {
                if (i) out += " | ";
                out += info->data.variant.types && info->data.variant.types[i]
                           ? renderType(info->data.variant.types[i])
                           : "unknown";
            }
            out += ">";
            return out;
        }
        default:
            return "unknown";
    }
}

template<typename Schema>
void dump(const aison::Introspection<Schema>& isp)
{
    for (const auto& entry : isp.objects()) {
        const auto& obj = entry.second;
        std::cout << "type: " << reinterpret_cast<std::uintptr_t>(entry.first) << "\n";
        if (!obj.name.empty()) {
            std::cout << " name: " << obj.name << "\n";
        }
        if (obj.hasDiscriminator) {
            std::cout << " discriminator: key=\"" << obj.discriminatorKey << "\" tag=\""
                      << obj.discriminatorTag << "\"\n";
        }
        for (const auto& f : obj.fields) {
            std::cout << " - " << f.name << ": " << renderType(f.type) << "\n";
        }
        std::cout << "\n";
    }

    for (const auto& entry : isp.variants()) {
        const auto& var = entry.second;
        std::cout << "variant: " << reinterpret_cast<std::uintptr_t>(entry.first);
        if (!var.name.empty()) {
            std::cout << " \"" << var.name << "\"";
        }
        std::cout << "\n";
        std::cout << " discriminator: key=\"" << var.discriminatorKey << "\"\n";
        for (const auto& alt : var.alternatives) {
            std::cout << " - tag=\"" << alt.tag << "\" type=" << renderType(alt.type) << "\n";
        }
        std::cout << "\n";
    }

    for (const auto& entry : isp.enums()) {
        const auto& en = entry.second;
        std::cout << "enum: ";
        if (!en.name.empty()) {
            std::cout << en.name << " (typeId=" << reinterpret_cast<std::uintptr_t>(entry.first)
                      << ")";
        } else {
            std::cout << reinterpret_cast<std::uintptr_t>(entry.first);
        }
        std::cout << " [";
        for (std::size_t i = 0; i < en.names.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << en.names[i];
        }
        std::cout << "]\n";
    }
}

}  // namespace

void introspectExample1()
{
    Cone cone;
    Json::Value root;
    aison::encode<DemoSchema>(cone, root);

    std::cout << root.toStyledString() << "\n";
    auto isp = aison::introspect<DemoSchema>();
    isp.add<Flavor>();

#if 1
    auto isp2 = isp;
    isp2.add<Order>();
    dump(isp2);
    std::cout << "--\n";
#endif

    dump(isp);
}

}  // namespace example
