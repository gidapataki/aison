#include <aison/aison.h>
#include <doctest.h>
#include <json/json.h>

#include <string>
#include <variant>
#include <vector>

namespace {

// --- Schema with discriminator key "kind" ---

struct ShapeA {
    double x = 0.0;
    double y = 0.0;
};

struct ShapeB {
    double radius = 0.0;
};

using ShapeVariantA = std::variant<ShapeA, ShapeB>;

struct SceneA {
    std::vector<ShapeVariantA> shapes;
};

struct SchemaKindKey : aison::Schema<SchemaKindKey> {
    template<typename T>
    struct Object;
    template<typename T>
    struct Variant;
};

template<>
struct SchemaKindKey::Variant<ShapeVariantA> : aison::Variant<SchemaKindKey, ShapeVariantA> {
    Variant()
    {
        name("ShapeVariantA");
        discriminator("kind");
    }
};

template<>
struct SchemaKindKey::Object<ShapeA> : aison::Object<SchemaKindKey, ShapeA> {
    Object()
    {
        name("shapeA");
        add(&ShapeA::x, "x");
        add(&ShapeA::y, "y");
    }
};

template<>
struct SchemaKindKey::Object<ShapeB> : aison::Object<SchemaKindKey, ShapeB> {
    Object()
    {
        name("shapeB");
        add(&ShapeB::radius, "radius");
    }
};

template<>
struct SchemaKindKey::Object<SceneA> : aison::Object<SchemaKindKey, SceneA> {
    Object() { add(&SceneA::shapes, "shapes"); }
};

// --- Schema with per-type discriminator key ---

struct Rect {
    double w = 0.0;
    double h = 0.0;
};

struct Ellipse {
    double rx = 0.0;
    double ry = 0.0;
    std::string color;
};

using ShapeVariantB = std::variant<Rect, Ellipse>;

struct SceneB {
    ShapeVariantB mainShape;
    std::vector<ShapeVariantB> extras;
};

struct SchemaExplicitKey : aison::Schema<SchemaExplicitKey> {
    template<typename T>
    struct Object;
    template<typename T>
    struct Variant;
};

template<>
struct SchemaExplicitKey::Variant<ShapeVariantB>
    : aison::Variant<SchemaExplicitKey, ShapeVariantB> {
    Variant()
    {
        name("ShapeVariantB");
        discriminator("type");
    }
};

template<>
struct SchemaExplicitKey::Object<Rect> : aison::Object<SchemaExplicitKey, Rect> {
    Object()
    {
        name("rect");
        add(&Rect::w, "w");
        add(&Rect::h, "h");
    }
};

template<>
struct SchemaExplicitKey::Object<Ellipse> : aison::Object<SchemaExplicitKey, Ellipse> {
    Object()
    {
        name("ellipse");
        add(&Ellipse::rx, "rx");
        add(&Ellipse::ry, "ry");
        add(&Ellipse::color, "color");
    }
};

template<>
struct SchemaExplicitKey::Object<SceneB> : aison::Object<SchemaExplicitKey, SceneB> {
    Object()
    {
        add(&SceneB::mainShape, "mainShape");
        add(&SceneB::extras, "extras");
    }
};

TEST_SUITE("Variant types")
{
    TEST_CASE("Round-trip with discriminator key \"kind\"")
    {
        SceneA in;
        ShapeA a;
        a.x = 1.5;
        a.y = -2.0;
        ShapeB b;
        b.radius = 3.25;
        in.shapes.push_back(a);
        in.shapes.push_back(b);

        Json::Value json;
        auto enc = aison::encode<SchemaKindKey>(in, json);
        REQUIRE(enc);
        REQUIRE(enc.errors.empty());

        REQUIRE(json["shapes"].isArray());
        CHECK(json["shapes"][0U]["kind"].asString() == "shapeA");
        CHECK(json["shapes"][1U]["kind"].asString() == "shapeB");

        SceneA out;
        auto dec = aison::decode<SchemaKindKey>(json, out);
        REQUIRE(dec);
        REQUIRE(dec.errors.empty());
        REQUIRE(out.shapes.size() == 2U);

        auto* outA = std::get_if<ShapeA>(&out.shapes[0]);
        auto* outB = std::get_if<ShapeB>(&out.shapes[1]);
        REQUIRE(outA);
        REQUIRE(outB);
        CHECK(outA->x == doctest::Approx(a.x));
        CHECK(outA->y == doctest::Approx(a.y));
        CHECK(outB->radius == doctest::Approx(b.radius));
    }

    TEST_CASE("Round-trip with discriminator key \"type\"")
    {
        SceneB in;
        Rect r;
        r.w = 10.0;
        r.h = 20.0;
        Ellipse e;
        e.rx = 4.0;
        e.ry = 6.0;
        e.color = "red";

        in.mainShape = e;
        in.extras = {r, e};

        Json::Value json;
        auto enc = aison::encode<SchemaExplicitKey>(in, json);
        REQUIRE(enc);
        REQUIRE(enc.errors.empty());

        // Ensure custom key name was used
        CHECK(json["mainShape"]["type"].asString() == "ellipse");
        CHECK(json["extras"][0U]["type"].asString() == "rect");
        CHECK(json["extras"][1U]["type"].asString() == "ellipse");

        SceneB out;
        auto dec = aison::decode<SchemaExplicitKey>(json, out);
        REQUIRE(dec);
        REQUIRE(dec.errors.empty());

        auto* mainEllipse = std::get_if<Ellipse>(&out.mainShape);
        REQUIRE(mainEllipse);
        CHECK(mainEllipse->color == e.color);
        CHECK(out.extras.size() == 2U);
        auto* extraRect = std::get_if<Rect>(&out.extras[0]);
        auto* extraEllipse = std::get_if<Ellipse>(&out.extras[1]);
        REQUIRE(extraRect);
        REQUIRE(extraEllipse);
        CHECK(extraRect->w == doctest::Approx(r.w));
        CHECK(extraEllipse->rx == doctest::Approx(e.rx));
    }
}

}  // namespace
