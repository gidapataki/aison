#include <aison/aison.h>
#include <doctest.h>
#include <json/json.h>

#include <string>
#include <variant>
#include <vector>

namespace {

enum class Mode { Off, On };

struct Point {
    int x = 0;
    int y = 0;
};

struct Circle {
    double r = 0.0;
};

struct Rectangle {
    double w = 0.0;
    double h = 0.0;
};

using Shape = std::variant<Circle, Rectangle>;

struct Doc {
    Point origin;
    Mode mode = Mode::Off;
    std::vector<int> values;
    std::optional<std::string> name;
    Shape shape;
};

struct ErrorSchema : aison::Schema<ErrorSchema> {
    template<typename T>
    struct Object;
    template<typename T>
    struct Enum;
    template<typename T>
    struct Variant;
};

template<>
struct ErrorSchema::Enum<Mode> : aison::Enum<ErrorSchema, Mode> {
    static constexpr auto name = "Mode";

    Enum()
    {
        add(Mode::Off, "off");
        add(Mode::On, "on");
    }
};

template<>
struct ErrorSchema::Object<Point> : aison::Object<ErrorSchema, Point> {
    static constexpr auto name = "point";

    Object()
    {
        add(&Point::x, "x");
        add(&Point::y, "y");
    }
};

template<>
struct ErrorSchema::Variant<Shape> : aison::Variant<ErrorSchema, Shape> {
    static constexpr auto name = "Shape";
    static constexpr auto discriminator = "kind";

    Variant()
    {
        add<Circle>("circle");
        add<Rectangle>("rect");
    }
};

template<>
struct ErrorSchema::Object<Circle> : aison::Object<ErrorSchema, Circle> {
    static constexpr auto name = "circle";

    Object()
    {
        add(&Circle::r, "r");
    }
};

template<>
struct ErrorSchema::Object<Rectangle> : aison::Object<ErrorSchema, Rectangle> {
    static constexpr auto name = "rect";

    Object()
    {
        add(&Rectangle::w, "w");
        add(&Rectangle::h, "h");
    }
};

template<>
struct ErrorSchema::Object<Doc> : aison::Object<ErrorSchema, Doc> {
    static constexpr auto name = "doc";

    Object()
    {
        add(&Doc::origin, "origin");
        add(&Doc::mode, "mode");
        add(&Doc::values, "values");
        add(&Doc::name, "name");
        add(&Doc::shape, "shape");
    }
};

TEST_SUITE("Runtime errors (asserts enabled)")
{
    TEST_CASE("Root is wrong type")
    {
        Json::Value root = Json::arrayValue;
        Doc out{};
        auto res = aison::decode<ErrorSchema>(root, out);
        CHECK_FALSE(res);
        REQUIRE_FALSE(res.errors.empty());
        CHECK(res.errors[0].path == "$");
        CHECK(res.errors[0].message.find("Expected object") != std::string::npos);
    }

    TEST_CASE("Missing required field")
    {
        Json::Value root(Json::objectValue);
        root["mode"] = "on";
        root["values"] = Json::arrayValue;
        root["values"].append(1);
        root["name"] = Json::nullValue;
        Json::Value circle(Json::objectValue);
        circle["kind"] = "circle";
        circle["r"] = 1.0;
        root["shape"] = circle;

        Doc out{};
        auto res = aison::decode<ErrorSchema>(root, out);
        CHECK_FALSE(res);
        REQUIRE_FALSE(res.errors.empty());
        CHECK(res.errors[0].path == "$");
        CHECK(res.errors[0].message.find("Missing required field 'origin'") != std::string::npos);
    }

    TEST_CASE("Field wrong type and array element wrong type")
    {
        Json::Value root(Json::objectValue);
        root["origin"] = Json::objectValue;
        root["origin"]["x"] = 1;
        root["origin"]["y"] = 2;
        root["mode"] = 123;  // should be string
        root["values"] = Json::arrayValue;
        root["values"].append(5);
        root["values"].append("oops");  // wrong element type
        root["name"] = Json::nullValue;
        Json::Value rect(Json::objectValue);
        rect["kind"] = "rect";
        rect["w"] = 2.0;
        rect["h"] = 3.0;
        root["shape"] = rect;

        Doc out{};
        auto res = aison::decode<ErrorSchema>(root, out);
        CHECK_FALSE(res);
        REQUIRE(res.errors.size() >= 2);
        CHECK(res.errors[0].path == "$.mode");
        CHECK(res.errors[0].message.find("Expected string for enum") != std::string::npos);
        CHECK(res.errors[1].path == "$.values[1]");
        CHECK(res.errors[1].message.find("Expected int") != std::string::npos);
    }

    TEST_CASE("Optional wrong type")
    {
        Json::Value root(Json::objectValue);
        root["origin"] = Json::objectValue;
        root["origin"]["x"] = 0;
        root["origin"]["y"] = 0;
        root["mode"] = "off";
        root["values"] = Json::arrayValue;
        root["values"].append(1);
        root["name"] = Json::arrayValue;
        Json::Value circle(Json::objectValue);
        circle["kind"] = "circle";
        circle["r"] = 1.0;
        root["shape"] = circle;

        Doc out{};
        auto res = aison::decode<ErrorSchema>(root, out);

        CHECK_FALSE(res);
        REQUIRE_FALSE(res.errors.empty());
        CHECK(res.errors[0].path == "$.name");
        CHECK(res.errors[0].message.find("Expected string") != std::string::npos);
    }

    TEST_CASE("Optional missing when strictOptional is true")
    {
        Json::Value root(Json::objectValue);
        root["origin"] = Json::objectValue;
        root["origin"]["x"] = 0;
        root["origin"]["y"] = 0;
        root["mode"] = "off";
        root["values"] = Json::arrayValue;
        root["values"].append(1);
        Json::Value circle(Json::objectValue);
        circle["kind"] = "circle";
        circle["r"] = 1.0;
        root["shape"] = circle;
        // name omitted entirely

        Doc out{};
        auto res = aison::decode<ErrorSchema>(root, out);

        CHECK_FALSE(res);
        REQUIRE_FALSE(res.errors.empty());
        CHECK(res.errors[0].path == "$");
        CHECK(res.errors[0].message.find("Missing required field 'name'") != std::string::npos);
    }

    TEST_CASE("Variant discriminator missing, unknown, or payload wrong type")
    {
        Doc out{};

        SUBCASE("Missing discriminator field")
        {
            Json::Value root(Json::objectValue);
            root["origin"] = Json::objectValue;
            root["origin"]["x"] = 1;
            root["origin"]["y"] = 1;
            root["mode"] = "off";
            root["values"] = Json::arrayValue;
            root["values"].append(1);
            root["name"] = Json::nullValue;
            Json::Value shape(Json::objectValue);
            shape["r"] = 2.0;  // no kind
            root["shape"] = shape;

            auto res = aison::decode<ErrorSchema>(root, out);

            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.shape.kind");
            CHECK(res.errors[0].message.find("Missing discriminator") != std::string::npos);
            CHECK(res.errors[0].message.find("Missing discriminator") != std::string::npos);
        }

        SUBCASE("Unknown discriminator value")
        {
            Json::Value root(Json::objectValue);
            root["origin"] = Json::objectValue;
            root["origin"]["x"] = 1;
            root["origin"]["y"] = 1;
            root["mode"] = "off";
            root["values"] = Json::arrayValue;
            root["values"].append(1);
            root["name"] = Json::nullValue;
            Json::Value shape(Json::objectValue);
            shape["kind"] = "triangle";
            root["shape"] = shape;

            auto res = aison::decode<ErrorSchema>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.shape.kind");
            CHECK(res.errors[0].message.find("Unknown discriminator value") != std::string::npos);
        }

        SUBCASE("Valid discriminator but payload wrong type")
        {
            Json::Value root(Json::objectValue);
            root["origin"] = Json::objectValue;
            root["origin"]["x"] = 1;
            root["origin"]["y"] = 1;
            root["mode"] = "on";
            root["values"] = Json::arrayValue;
            root["values"].append(1);
            root["name"] = Json::nullValue;
            Json::Value shape(Json::objectValue);
            shape["kind"] = "circle";
            shape["r"] = Json::Value("bad");  // wrong type
            root["shape"] = shape;

            auto res = aison::decode<ErrorSchema>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.shape.r");
            CHECK(res.errors[0].message.find("Expected double") != std::string::npos);
        }
    }
}

}  // namespace
