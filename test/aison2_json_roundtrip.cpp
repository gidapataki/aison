#include <doctest.h>
#include <json/json.h>

#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "aison2/aison2.h"
#include "aison2/json_adapter.h"

namespace {

struct Point {
    int x;
    int y;
};

enum class Color { kRed, kGreen };

struct Label {
    std::string text;
};

struct Circle {
    int radius;
};

struct Rectangle {
    int width;
    int height;
};

using Shape = std::variant<Circle, Rectangle>;

struct Scene {
    Point origin;
    std::vector<Shape> shapes;
    std::optional<Color> color;
    Label label;
};

struct LabelDto {
    std::string value;
};

}  // namespace

TEST_CASE("aison2: JSON roundtrip with custom type and variant")
{
    auto pointDef = aison2::object<Point>(
        aison2::Fields{aison2::field(&Point::x, "x"), aison2::field(&Point::y, "y")});

    auto colorDef = aison2::enumeration<Color>(
        aison2::EnumValues{aison2::value(Color::kRed, "red"), aison2::value(Color::kGreen, "green")});

    auto circleDef =
        aison2::object<Circle>(aison2::Fields{aison2::field(&Circle::radius, "radius")});

    auto rectangleDef = aison2::object<Rectangle>(aison2::Fields{
        aison2::field(&Rectangle::width, "width"),
        aison2::field(&Rectangle::height, "height"),
    });

    auto shapeDef = aison2::variant<Shape>(aison2::VariantAlternatives{
        aison2::type<Circle>("circle"),
        aison2::type<Rectangle>("rectangle"),
    });

    auto labelDtoDef =
        aison2::object<LabelDto>(aison2::Fields{aison2::field(&LabelDto::value, "value")});

    auto schema = aison2::schema(std::tuple{pointDef, colorDef, circleDef, rectangleDef, shapeDef,
                                            labelDtoDef});

    auto labelCustom = aison2::custom<Label>(
        [](const Label& label, const auto& ctx) {
            LabelDto dto{label.text};
            return ctx.encode(dto);
        },
        [](const Json::Value& value, const auto& ctx) {
            auto dto = ctx.template decode<LabelDto>(value);
            return Label{dto.value};
        });

    auto sceneDef = aison2::object<Scene>(aison2::Fields{
        aison2::field(&Scene::origin, "origin"),
        aison2::field(&Scene::shapes, "shapes"),
        aison2::field(&Scene::color, "color"),
        aison2::field(&Scene::label, "label"),
    });

    auto fullSchema = aison2::schema(
        std::tuple{pointDef, colorDef, circleDef, rectangleDef, shapeDef, labelDtoDef, labelCustom,
                   sceneDef});

    Scene scene{
        .origin = {.x = 1, .y = 2},
        .shapes = {Circle{.radius = 3}, Rectangle{.width = 4, .height = 5}},
        .color = Color::kGreen,
        .label = Label{.text = "hi"},
    };

    const Json::Value encoded = aison2::json::encode(fullSchema, scene);
    const Scene decoded = aison2::json::decode<Scene>(fullSchema, encoded);

    CHECK(decoded.origin.x == 1);
    CHECK(decoded.origin.y == 2);
    REQUIRE(decoded.shapes.size() == 2);
    CHECK(std::get<Circle>(decoded.shapes[0]).radius == 3);
    CHECK(std::get<Rectangle>(decoded.shapes[1]).width == 4);
    CHECK(std::get<Rectangle>(decoded.shapes[1]).height == 5);
    CHECK(decoded.color.has_value());
    CHECK(decoded.color.value() == Color::kGreen);
    CHECK(decoded.label.text == "hi");

    // Ensure enum encoded as string and variant tag present
    CHECK(encoded["color"].asString() == "green");
    CHECK(encoded["shapes"][0u]["type"].asString() == "circle");
    CHECK(encoded["shapes"][1u]["type"].asString() == "rectangle");
}
