#include <doctest.h>
#include <json/json.h>

#include <iostream>

#include "aison/aison.h"

namespace {

// Simple geometry types for variant tests
enum class ShapeTag {
    Circle,
    Rectangle,
};

struct Circle {
    double x = 0.0;
    double y = 0.0;
    double radius = 0.0;
};

struct Rectangle {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

using Shape = std::variant<Circle, Rectangle>;

struct ShapeScene {
    std::vector<Shape> shapes;
};

struct ShapeSchema : aison::Schema<ShapeSchema> {
    using EnableAssert = std::false_type;
    static constexpr std::string_view discriminatorKey = "kind";

    template<typename T>
    struct Object;
    template<typename T>
    struct Enum;
};

// Enum mapping for ShapeTag
template<>
struct ShapeSchema::Enum<ShapeTag> : aison::Enum<ShapeSchema, ShapeTag> {
    Enum()
    {
        add(ShapeTag::Circle, "circle");
        add(ShapeTag::Rectangle, "rect");
    }
};

// Object mappings
template<>
struct ShapeSchema::Object<Circle>
    : aison::Object<ShapeSchema, Circle>
    , aison::Discriminator<ShapeSchema, Circle, ShapeTag::Circle> {
    Object()
    {
        add(&Circle::x, "x");
        add(&Circle::y, "y");
        add(&Circle::radius, "radius");
    }
};

template<>
struct ShapeSchema::Object<Rectangle>
    : aison::Object<ShapeSchema, Rectangle>
    , aison::Discriminator<ShapeSchema, Rectangle, ShapeTag::Rectangle> {
    Object()
    {
        add(&Rectangle::x, "x");
        add(&Rectangle::y, "y");
        add(&Rectangle::width, "width");
        add(&Rectangle::height, "height");
    }
};

template<>
struct ShapeSchema::Object<ShapeScene> : aison::Object<ShapeSchema, ShapeScene> {
    Object() { add(&ShapeScene::shapes, "shapes"); }
};

TEST_CASE("variant: encode simple scene")
{
    ShapeScene scene;
    Circle c;
    c.x = 1.0;
    c.y = 2.0;
    c.radius = 3.0;
    Rectangle r;
    r.x = 4.0;
    r.y = 5.0;
    r.width = 6.0;
    r.height = 7.0;

    scene.shapes.push_back(c);
    scene.shapes.push_back(r);

    Json::Value root;
    auto result = aison::encode<ShapeSchema>(scene, root);
    CHECK(result);
    CHECK(result.errors.empty());

    REQUIRE(root.isObject());
    REQUIRE(root["shapes"].isArray());
    REQUIRE(root["shapes"].size() == 2U);

    const auto& jCircle = root["shapes"][0U];
    const auto& jRect = root["shapes"][1U];

    // Check discriminator and fields for circle
    CHECK(jCircle.isObject());
    CHECK(jCircle["kind"].isString());
    CHECK(jCircle["kind"].asString() == "circle");
    CHECK(jCircle["x"].asDouble() == doctest::Approx(1.0));
    CHECK(jCircle["y"].asDouble() == doctest::Approx(2.0));
    CHECK(jCircle["radius"].asDouble() == doctest::Approx(3.0));

    // Check discriminator and fields for rectangle
    CHECK(jRect.isObject());
    CHECK(jRect["kind"].isString());
    CHECK(jRect["kind"].asString() == "rect");
    CHECK(jRect["x"].asDouble() == doctest::Approx(4.0));
    CHECK(jRect["y"].asDouble() == doctest::Approx(5.0));
    CHECK(jRect["width"].asDouble() == doctest::Approx(6.0));
    CHECK(jRect["height"].asDouble() == doctest::Approx(7.0));
}

TEST_CASE("variant: round-trip encode/decode scene")
{
    ShapeScene scene;
    Circle c;
    c.x = 1.5;
    c.y = -2.25;
    c.radius = 10.0;
    Rectangle r;
    r.x = -1.0;
    r.y = 3.5;
    r.width = 8.0;
    r.height = 9.0;
    scene.shapes = {c, r};

    Json::Value root;
    auto encResult = aison::encode<ShapeSchema>(scene, root);
    REQUIRE(encResult);
    REQUIRE(encResult.errors.empty());

    ShapeScene decoded;
    auto decResult = aison::decode<ShapeSchema>(root, decoded);
    CHECK(decResult);
    CHECK(decResult.errors.empty());

    REQUIRE(decoded.shapes.size() == 2U);

    const auto* dc = std::get_if<Circle>(&decoded.shapes[0]);
    const auto* dr = std::get_if<Rectangle>(&decoded.shapes[1]);
    REQUIRE(dc);
    REQUIRE(dr);

    CHECK(dc->x == doctest::Approx(c.x));
    CHECK(dc->y == doctest::Approx(c.y));
    CHECK(dc->radius == doctest::Approx(c.radius));

    CHECK(dr->x == doctest::Approx(r.x));
    CHECK(dr->y == doctest::Approx(r.y));
    CHECK(dr->width == doctest::Approx(r.width));
    CHECK(dr->height == doctest::Approx(r.height));
}

TEST_CASE("variant: decode fails on missing discriminator")
{
    Json::Value root = Json::objectValue;
    root["shapes"] = Json::arrayValue;
    Json::Value jShape = Json::objectValue;
    jShape["x"] = 1.0;
    jShape["y"] = 2.0;
    jShape["radius"] = 3.0;
    // NOTE: no "kind" field
    root["shapes"].append(jShape);

    ShapeScene scene;
    auto decResult = aison::decode<ShapeSchema>(root, scene);
    CHECK_FALSE(decResult);
    REQUIRE_FALSE(decResult.errors.empty());

    // Error path should point to shapes[0].kind
    const auto& err = decResult.errors.front();
    CHECK(err.path == "$.shapes[0]");
}

TEST_CASE("variant: decode fails on unknown discriminator value")
{
    Json::Value root = Json::objectValue;
    root["shapes"] = Json::arrayValue;
    Json::Value jShape = Json::objectValue;
    jShape["kind"] = "triangle";  // unknown tag
    jShape["x"] = 0.0;
    jShape["y"] = 0.0;
    root["shapes"].append(jShape);

    ShapeScene scene;
    auto decResult = aison::decode<ShapeSchema>(root, scene);
    CHECK_FALSE(decResult);
    REQUIRE_FALSE(decResult.errors.empty());

    const auto& err = decResult.errors.front();
    CHECK(err.path == "$.shapes[0]");
}

}  // namespace
