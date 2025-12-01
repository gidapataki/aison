#include <aison/aison.h>
#include <doctest.h>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

struct Point {
    int x = 0;
    int y = 0;
};

enum class Mode { Off, On };

struct Circle {
    double r = 0.0;
};

struct Rect {
    double w = 0.0;
    double h = 0.0;
};

using Shape = std::variant<Circle, Rect>;

struct Color {
    std::string hex;
};

struct Doc {
    Point origin;
    Mode mode = Mode::Off;
    std::optional<Shape> shape;
    Color color;
    std::vector<int> values;
};

struct IntrospectSchema : aison::Schema<IntrospectSchema> {
    static constexpr bool enableIntrospect = true;

    template<typename T>
    struct Object;
    template<typename T>
    struct Variant;
    template<typename T>
    struct Enum;
    template<typename T>
    struct Custom;
};

template<>
struct IntrospectSchema::Enum<Mode> : aison::Enum<IntrospectSchema, Mode> {
    static constexpr auto name = "Mode";

    Enum()
    {
        add(Mode::Off, "off");
        add(Mode::On, "on");
    }
};

template<>
struct IntrospectSchema::Object<Point> : aison::Object<IntrospectSchema, Point> {
    static constexpr auto name = "Point";

    Object()
    {
        add(&Point::x, "x");
        add(&Point::y, "y");
    }
};

template<>
struct IntrospectSchema::Object<Circle> : aison::Object<IntrospectSchema, Circle> {
    static constexpr auto name = "Circle";

    Object() { add(&Circle::r, "r"); }
};

template<>
struct IntrospectSchema::Object<Rect> : aison::Object<IntrospectSchema, Rect> {
    static constexpr auto name = "Rect";

    Object()
    {
        add(&Rect::w, "w");
        add(&Rect::h, "h");
    }
};

template<>
struct IntrospectSchema::Variant<Shape> : aison::Variant<IntrospectSchema, Shape> {
    static constexpr auto name = "Shape";
    static constexpr auto discriminator = "kind";

    Variant()
    {
        add<Circle>("circle");
        add<Rect>("rect");
    }
};

template<>
struct IntrospectSchema::Custom<Color> : aison::Custom<IntrospectSchema, Color> {
    static constexpr auto name = "Color";

    void encode(const Color& src, Json::Value& dst, EncodeContext&) const { dst = src.hex; }

    void decode(const Json::Value& src, Color& dst, DecodeContext& ctx) const
    {
        if (!src.isString()) {
            ctx.addError("Expected string for Color");
            return;
        }
        dst.hex = src.asString();
    }
};

template<>
struct IntrospectSchema::Object<Doc> : aison::Object<IntrospectSchema, Doc> {
    static constexpr auto name = "Doc";

    Object()
    {
        add(&Doc::origin, "origin");
        add(&Doc::mode, "mode");
        add(&Doc::shape, "shape");
        add(&Doc::color, "color");
        add(&Doc::values, "values");
    }
};

TEST_SUITE("Introspection")
{
    TEST_CASE("introspect emits names, discriminator, and field metadata")
    {
        auto isp = aison::introspect<IntrospectSchema, Doc>();
        REQUIRE(isp);

        // Doc
        auto docId = aison::getTypeId<Doc>();
        auto itDoc = isp.types.find(docId);
        REQUIRE(itDoc != isp.types.end());
        const auto* docInfo = std::get_if<aison::ObjectInfo>(&itDoc->second);
        REQUIRE(docInfo);
        CHECK(docInfo->name == "Doc");
        CHECK(docInfo->fields.size() == 5U);

        // Variant info
        auto shapeId = aison::getTypeId<Shape>();
        auto itShape = isp.types.find(shapeId);
        REQUIRE(itShape != isp.types.end());
        const auto* varInfo = std::get_if<aison::VariantInfo>(&itShape->second);
        REQUIRE(varInfo);
        CHECK(varInfo->name == "Shape");
        CHECK(varInfo->discriminator == "kind");
        REQUIRE(varInfo->alternatives.size() == 2U);
        CHECK((varInfo->alternatives[0].name == "circle" ||
               varInfo->alternatives[1].name == "circle"));
        CHECK((varInfo->alternatives[0].name == "rect" ||
               varInfo->alternatives[1].name == "rect"));

        // Optional<Shape> should be present
        auto optShapeId = aison::getTypeId<std::optional<Shape>>();
        auto itOpt = isp.types.find(optShapeId);
        REQUIRE(itOpt != isp.types.end());
        const auto* optInfo = std::get_if<aison::OptionalInfo>(&itOpt->second);
        REQUIRE(optInfo);
        CHECK(optInfo->type == shapeId);

        // Enum info
        auto modeId = aison::getTypeId<Mode>();
        auto itMode = isp.types.find(modeId);
        REQUIRE(itMode != isp.types.end());
        const auto* enumInfo = std::get_if<aison::EnumInfo>(&itMode->second);
        REQUIRE(enumInfo);
        CHECK(enumInfo->name == "Mode");
        REQUIRE(enumInfo->values.size() == 2U);
        CHECK(enumInfo->values[0] == "off");
        CHECK(enumInfo->values[1] == "on");

        // Custom info
        auto colorId = aison::getTypeId<Color>();
        auto itColor = isp.types.find(colorId);
        REQUIRE(itColor != isp.types.end());
        const auto* customInfo = std::get_if<aison::CustomInfo>(&itColor->second);
        REQUIRE(customInfo);
        CHECK(customInfo->name == "Color");
    }
}

}  // namespace
