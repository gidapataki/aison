#include <aison/aison.h>

#include <cstdint>
#include <iostream>
#include <optional>
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

// Introspection demo ---------------------------------------------------------

using aison::TypeClass;
using aison::TypeInfo;

std::string renderType(const TypeInfo* info)
{
    if (!info) return "unknown";
    switch (info->cls) {
        case TypeClass::Bool:
            return "bool";
        case TypeClass::Integral: {
            auto size = static_cast<int>(info->data.integral.size);
            return (info->data.integral.isSigned ? "int" : "uint") + std::to_string(size * 8);
        }
        case TypeClass::Floating: {
            auto size = info->data.floating.size;
            if (size == 4) return "float";
            if (size == 8) return "double";
            return "float" + std::to_string(size);
        }
        case TypeClass::String:
            return "string";
        case TypeClass::Enum:
            return "enum(typeId=" + std::to_string(reinterpret_cast<std::uintptr_t>(info->typeId)) +
                   ")";
        case TypeClass::Object:
            return "object(typeId=" +
                   std::to_string(reinterpret_cast<std::uintptr_t>(info->typeId)) + ")";
        case TypeClass::Optional:
            return "optional<" + renderType(info->data.optional.type) + ">";
        case TypeClass::Vector:
            return "vector<" + renderType(info->data.vector.type) + ">";
        case TypeClass::Variant: {
            std::string out = "variant<";
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
        if (obj.hasDiscriminator) {
            std::cout << " discriminator: key=\"" << obj.discriminatorKey << "\" tag=\""
                      << obj.discriminatorTag << "\"\n";
        }
        for (const auto& f : obj.fields) {
            std::cout << " - " << f.name << ": " << renderType(f.type) << "\n";
        }
        std::cout << "\n";
    }

    for (const auto& entry : isp.enums()) {
        const auto& en = entry.second;
        std::cout << "enum: " << reinterpret_cast<std::uintptr_t>(entry.first) << " [";
        for (std::size_t i = 0; i < en.names.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << en.names[i];
        }
        std::cout << "]\n";
    }
}

int main()
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
    return 0;
}
