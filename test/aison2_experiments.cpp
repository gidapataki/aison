#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest.h>

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "aison2/aison2.h"

namespace {

struct Bar {
    int x;
};

struct Foo {
    int y;
    Bar bar;
};

enum class Mode { kDark, kLight, kAutomatic };

struct External {
    int z;
};

struct UsesExternal {
    External ext;
};

struct WithOptional {
    std::optional<External> maybe;
};

struct WithVector {
    std::vector<Bar> bars;
};

struct Circle {
    int radius;
};

struct Rectangle {
    int width;
    int height;
};

using Shape = std::variant<Circle, Rectangle>;

}  // namespace

TEST_CASE("aison2: schema scaffolding captures definitions and declarations")
{
    auto barDef = aison2::object<Bar>(+[](aison2::detail::ObjectContext<Bar>& ctx) {
        return aison2::Fields{
            ctx.add(&Bar::x, "x"),
        };
    });

    auto fooDef = aison2::object<Foo>([](auto& ctx) {
        return aison2::Fields{
            ctx.add(&Foo::y, "y"),
            ctx.add(&Foo::bar, "bar"),
        };
    });

    auto modeDef = aison2::enumeration<Mode>(+[](aison2::detail::EnumContext<Mode>& ctx) {
        return aison2::EnumValues{
            ctx.value("dark", Mode::kDark),
            ctx.value("light", Mode::kLight),
            ctx.value("auto", Mode::kAutomatic),
        };
    });

    auto usesExternalDef =
        aison2::object<UsesExternal>(+[](aison2::detail::ObjectContext<UsesExternal>& ctx) {
            return aison2::Fields{
                ctx.add(&UsesExternal::ext, "ext"),
            };
        });

    auto withOptionalDef =
        aison2::object<WithOptional>(+[](aison2::detail::ObjectContext<WithOptional>& ctx) {
            return aison2::Fields{
                ctx.add(&WithOptional::maybe, "maybe"),
            };
        });

    auto withVectorDef =
        aison2::object<WithVector>(+[](aison2::detail::ObjectContext<WithVector>& ctx) {
            return aison2::Fields{
                ctx.add(&WithVector::bars, "bars"),
            };
        });

    auto circleDef = aison2::object<Circle>(+[](aison2::detail::ObjectContext<Circle>& ctx) {
        return aison2::Fields{
            ctx.add(&Circle::radius, "radius"),
        };
    });

    auto rectangleDef =
        aison2::object<Rectangle>(+[](aison2::detail::ObjectContext<Rectangle>& ctx) {
            return aison2::Fields{
                ctx.add(&Rectangle::width, "width"),
                ctx.add(&Rectangle::height, "height"),
            };
        });

    auto shapeDef = aison2::variant<Shape>(
        {.tag = "kind"}, +[](aison2::detail::VariantContext<Shape>& ctx) {
            ctx.template add<Circle>("circle");
            ctx.template add<Rectangle>("rectangle");
        });

    auto schema = aison2::schema(
        std::tuple{
            aison2::declare<External>(),
            barDef,
            fooDef,
            modeDef,
            usesExternalDef,
            withOptionalDef,
            withVectorDef,
            circleDef,
            rectangleDef,
            shapeDef,
        });

    static_assert(decltype(schema)::template defines<Bar>(), "Bar should be defined");
    static_assert(decltype(schema)::template defines<Foo>(), "Foo should be defined");
    static_assert(decltype(schema)::template defines<Mode>(), "Mode should be defined");
    static_assert(
        decltype(schema)::template defines<WithOptional>(), "WithOptional should be defined");
    static_assert(decltype(schema)::template defines<WithVector>(), "WithVector should be defined");
    static_assert(decltype(schema)::template defines<Circle>(), "Circle should be defined");
    static_assert(decltype(schema)::template defines<Rectangle>(), "Rectangle should be defined");
    static_assert(decltype(schema)::template defines<Shape>(), "Shape should be defined");
    static_assert(!decltype(schema)::template defines<External>(), "External is only declared");
    static_assert(decltype(schema)::template declares<External>(), "External should be declared");

    CHECK(schema.defines<Bar>());
    CHECK(schema.defines<Foo>());
    CHECK(schema.defines<Mode>());
    CHECK(schema.defines<WithOptional>());
    CHECK(schema.defines<WithVector>());
    CHECK(schema.defines<Circle>());
    CHECK(schema.defines<Rectangle>());
    CHECK(schema.defines<Shape>());
    CHECK(schema.declares<External>());

    using ShapeDef = decltype(shapeDef);
    const auto& storedShapeDef = std::get<ShapeDef>(schema.definitions());
    CHECK(std::string_view(storedShapeDef.config.tag) == "kind");
}
