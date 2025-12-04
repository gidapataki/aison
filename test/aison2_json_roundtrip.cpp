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
        +[](aison2::detail::ObjectContext<Point>& ctx) {
        return aison2::Fields{
            ctx.add(&Point::x, "x"),
            ctx.add(&Point::y, "y"),
        };
    });

    auto colorDef = aison2::enumeration<Color>(
        +[](aison2::detail::EnumContext<Color>& ctx) {
        return aison2::EnumValues{
            ctx.value("red", Color::kRed),
            ctx.value("green", Color::kGreen),
        };
    });

    auto circleDef = aison2::object<Circle>(
        +[](aison2::detail::ObjectContext<Circle>& ctx) {
        return aison2::Fields{
            ctx.add(&Circle::radius, "radius"),
        };
    });

    auto rectangleDef = aison2::object<Rectangle>(
        +[](aison2::detail::ObjectContext<Rectangle>& ctx) {
            return aison2::Fields{
                ctx.add(&Rectangle::width, "width"),
                ctx.add(&Rectangle::height, "height"),
            };
        });

    auto shapeDef = aison2::variant<Shape>({.tag = "kind"},
                                           +[](aison2::detail::VariantContext<Shape>& ctx) {
            ctx.template add<Circle>("circle");
            ctx.template add<Rectangle>("rectangle");
        });

    auto labelDtoDef = aison2::object<LabelDto>(
        +[](aison2::detail::ObjectContext<LabelDto>& ctx) {
        return aison2::Fields{
            ctx.add(&LabelDto::value, "value"),
        };
    });

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

    auto sceneDef = aison2::object<Scene>(
        +[](aison2::detail::ObjectContext<Scene>& ctx) {
        return aison2::Fields{
            ctx.add(&Scene::origin, "origin"),
            ctx.add(&Scene::shapes, "shapes"),
            ctx.add(&Scene::color, "color"),
            ctx.add(&Scene::label, "label"),
        };
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
    CHECK(encoded["shapes"][0u]["kind"].asString() == "circle");
    CHECK(encoded["shapes"][1u]["kind"].asString() == "rectangle");
}
