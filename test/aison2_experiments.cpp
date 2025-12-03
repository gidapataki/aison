#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest.h>

#include "aison2/aison2.h"

namespace {

struct Bar {
    int x;
};

struct Foo {
    int y;
    Bar bar;
};

enum class Mode { Dark, Light, Automatic };

struct External {
    int z;
};

struct UsesExternal {
    External ext;
};

}  // namespace

TEST_CASE("aison2: schema scaffolding captures definitions and declarations")
{
    auto barDef = aison2::Object<Bar>([](auto& ctx) {
        return aison2::Fields{
            ctx.add(&Bar::x, "x"),
        };
    });

    auto fooDef = aison2::Object<Foo>(
        [](auto& ctx) { return aison2::Fields{ctx.add(&Foo::y, "y"), ctx.add(&Foo::bar, "bar")}; });

    auto modeDef = aison2::Enum<Mode>([](auto& ctx) {
        return aison2::EnumValues{
            ctx.value("dark", Mode::Dark),
            ctx.value("light", Mode::Light),
            ctx.value("auto", Mode::Automatic),
        };
    });

    auto usesExternalDef = aison2::Object<UsesExternal>([](auto& ctx) {
        return aison2::Fields{
            ctx.add(&UsesExternal::ext, "ext"),
        };
    });

    auto schema = aison2::Schema{
        aison2::Declare<External>(), barDef, fooDef, modeDef, usesExternalDef,
    };

    static_assert(decltype(schema)::template defines<Bar>(), "Bar should be defined");
    static_assert(decltype(schema)::template defines<Foo>(), "Foo should be defined");
    static_assert(decltype(schema)::template defines<Mode>(), "Mode should be defined");
    static_assert(!decltype(schema)::template defines<External>(), "External is only declared");
    static_assert(decltype(schema)::template declares<External>(), "External should be declared");

    CHECK(schema.defines<Bar>());
    CHECK(schema.defines<Foo>());
    CHECK(schema.defines<Mode>());
    CHECK(schema.declares<External>());
}
