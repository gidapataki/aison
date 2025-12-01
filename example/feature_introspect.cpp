#include <aison/aison.h>

#include <cstdint>
#include <iostream>
#include <string>

#include "types.h"

namespace example {
namespace {

struct DemoSchema : aison::Schema<DemoSchema> {
    static constexpr bool enableIntrospect = true;
    static constexpr bool strictOptional = false;

    template<typename T>
    struct Object;
    template<typename T>
    struct Enum;
    template<typename T>
    struct Variant;
};

template<>
struct DemoSchema::Enum<Flavor> : aison::Enum<DemoSchema, Flavor> {
    static constexpr auto name = "Flavor";

    Enum()
    {
        add(Flavor::kVanilla, "vanilla");
        add(Flavor::kChocolate, "chocolate");
    }
};

template<>
struct DemoSchema::Object<Topping> : aison::Object<DemoSchema, Topping> {
    static constexpr auto name = "Topping";

    Object()
    {
        add(&Topping::name, "name");
        add(&Topping::crunchy, "crunchy");
    }
};

template<>
struct DemoSchema::Variant<Dessert> : aison::Variant<DemoSchema, Dessert> {
    static constexpr auto name = "Dessert";
    static constexpr auto discriminator = "kind";
};

template<>
struct DemoSchema::Object<Cone> : aison::Object<DemoSchema, Cone> {
    static constexpr auto name = "Cone";

    Object()
    {
        add(&Cone::scoops, "scoops");
        add(&Cone::flavor, "flavor");
        add(&Cone::toppings, "toppings");
    }
};

template<>
struct DemoSchema::Object<Cup> : aison::Object<DemoSchema, Cup> {
    static constexpr auto name = "Cup";

    Object()
    {
        add(&Cup::sprinkles, "sprinkles");
        add(&Cup::drizzle, "drizzle");
    }
};

template<>
struct DemoSchema::Object<Order> : aison::Object<DemoSchema, Order> {
    static constexpr auto name = "Order";

    Object()
    {
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
    if (!info) {
        return typeIdToString(id);
    }

    return std::visit(
        [&](const auto& info) -> std::string {
            using T = std::decay_t<decltype(info)>;
            if constexpr (std::is_same_v<T, aison::BoolInfo>) {
                return "bool";

            } else if constexpr (std::is_same_v<T, aison::StringInfo>) {
                return "string";

            } else if constexpr (std::is_same_v<T, aison::IntegralInfo>) {
                return (info.isSigned ? "int" : "uint") + std::to_string(info.size * 8);

            } else if constexpr (std::is_same_v<T, aison::FloatingInfo>) {
                if (info.size == 4) return "float";
                if (info.size == 8) return "double";
                return "float" + std::to_string(info.size * 8);

            } else if constexpr (std::is_same_v<T, aison::OptionalInfo>) {
                return "optional<" + renderType(isp, info.type) + ">";

            } else if constexpr (std::is_same_v<T, aison::VectorInfo>) {
                return "vector<" + renderType(isp, info.type) + ">";

            } else if constexpr (std::is_same_v<T, aison::VariantInfo>) {
                return "variant(" + info.name + ")";

            } else if constexpr (std::is_same_v<T, aison::ObjectInfo>) {
                return "object(" + info.name + ")";

            } else if constexpr (std::is_same_v<T, aison::EnumInfo>) {
                return "enum(" + info.name + ")";

            } else if constexpr (std::is_same_v<T, aison::CustomInfo>) {
                return "custom(" + info.name + ")";
            }

            return "unknown";
        },
        *info);
}

void dump(const aison::IntrospectResult& isp)
{
    if (!isp) {
        std::cerr << "== Introspect errors ==\n";
        for (auto& err : isp.errors) {
            std::cerr << err.path << ": " << err.message << "\n";
        }
        return;
    }

    for (const auto& entry : isp.types) {
        std::visit(
            [&](const auto& info) {
                using T = std::decay_t<decltype(info)>;
                if constexpr (std::is_same_v<T, aison::ObjectInfo>) {
                    std::cout << "object: " << info.name << "\n";
                    for (const auto& f : info.fields) {
                        std::cout << " - " << f.name << ": " << renderType(isp, f.type);
                        if (!f.isRequired) std::cout << " (optional)";
                        std::cout << "\n";
                    }
                    std::cout << "\n";

                } else if constexpr (std::is_same_v<T, aison::EnumInfo>) {
                    std::cout << "enum: " << info.name << " [";
                    for (std::size_t i = 0; i < info.values.size(); ++i) {
                        if (i) std::cout << ", ";
                        std::cout << info.values[i];
                    }
                    std::cout << "]\n\n";

                } else if constexpr (std::is_same_v<T, aison::VariantInfo>) {
                    std::cout << "variant: " << info.name << " | discriminator=\""
                              << info.discriminator << "\"\n";
                    for (const auto& alt : info.alternatives) {
                        std::cout << " - tag=\"" << alt.name
                                  << "\" type=" << renderType(isp, alt.type) << "\n";
                    }
                    std::cout << "\n";
                } else if constexpr (std::is_same_v<T, aison::CustomInfo>) {
                    std::cout << "custom: " << info.name << "\n\n";
                }
            },
            entry.second);
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

#if 1
    auto isp2 = aison::introspect<DemoSchema, Flavor, Order>();
    dump(isp2);
    std::cout << "--\n";
#endif

    dump(isp);
}

}  // namespace example
