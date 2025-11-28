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

// Introspection demo ---------------------------------------------------------

using namespace aison::detail;

std::string renderType(const TypeInfo* info)
{
    if (!info) return "unknown";
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

    switch (info->kind) {
        case FieldKind::Plain: {
            std::string out = basicToStr(info->basic);
            if (info->basic == BasicType::Enum || info->basic == BasicType::Object) {
                out += "(typeId=" + std::to_string(reinterpret_cast<std::uintptr_t>(info->typeId)) +
                       ")";
            }
            return out;
        }
        case FieldKind::Optional:
            return "optional<" + renderType(info->element) + ">";
        case FieldKind::Vector:
            return "vector<" + renderType(info->element) + ">";
        case FieldKind::Variant: {
            std::string out = "variant<";
            for (std::size_t i = 0; i < info->variantCount; ++i) {
                if (i) out += " | ";
                out +=
                    info->variants && info->variants[i] ? renderType(info->variants[i]) : "unknown";
            }
            out += ">";
            return out;
        }
    }
    return "unknown";
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
    auto isp = aison::introspect<DemoSchema>();
    isp.add<Flavor>();
#if 0
    auto isp2 = isp;
    isp2.add<Order>();
    dump(isp2);
    std::cout << "--\n";
#endif

    dump(isp);
    return 0;
}
