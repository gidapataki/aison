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
    auto barDef = aison2::object<Bar>(aison2::Fields{
        aison2::field(&Bar::x, "x"),
    });

    auto fooDef = aison2::object<Foo>(aison2::Fields{
        aison2::field(&Foo::y, "y"),
        aison2::field(&Foo::bar, "bar"),
    });

    auto modeDef = aison2::enumeration<Mode>(aison2::Values{
        aison2::value(Mode::kDark, "dark"),
        aison2::value(Mode::kLight, "light"),
        aison2::value(Mode::kAutomatic, "auto"),
    });

    auto usesExternalDef = aison2::object<UsesExternal>(aison2::Fields{
        aison2::field(&UsesExternal::ext, "ext"),
    });

    auto withOptionalDef = aison2::object<WithOptional>(aison2::Fields{
        aison2::field(&WithOptional::maybe, "maybe"),
    });

    auto withVectorDef = aison2::object<WithVector>(aison2::Fields{
        aison2::field(&WithVector::bars, "bars"),
    });

    auto circleDef = aison2::object<Circle>(aison2::Fields{
        aison2::field(&Circle::radius, "radius"),
    });

    auto rectangleDef = aison2::object<Rectangle>(aison2::Fields{
        aison2::field(&Rectangle::width, "width"),
        aison2::field(&Rectangle::height, "height"),
    });

    auto shapeDef = aison2::variant<Shape>(aison2::Types{
        aison2::type<Circle>("circle"),
        aison2::type<Rectangle>("rectangle"),
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
    CHECK(std::string_view(storedShapeDef.config.tag) == "type");
}
