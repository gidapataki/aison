#include <aison/aison.h>
#include <doctest.h>
#include <json/json.h>

#include <string>
#include <variant>

namespace {

// Schema with asserts disabled to observe runtime errors instead of aborts.
struct GuardSchema : aison::Schema<GuardSchema> {
    static constexpr auto enableAssert = false;

    template<typename T>
    struct Object;
};

struct DupField {
    int a = 0;
    int b = 0;
};

struct DupMember {
    int v = 0;
};

template<>
struct GuardSchema::Object<DupField> : aison::Object<GuardSchema, DupField> {
    static constexpr auto name = "dupField";

    Object()
    {
        add(&DupField::a, "value");
        add(&DupField::b, "value");  // duplicate name would assert with EnableAssert == true
    }
};

template<>
struct GuardSchema::Object<DupMember> : aison::Object<GuardSchema, DupMember> {
    static constexpr auto name = "dupMember";

    Object()
    {
        add(&DupMember::v, "primary");
        add(&DupMember::v, "alias");  // duplicate member mapping would assert otherwise
    }
};

// Incomplete variant mapping --------------------------------------------------

struct CircleV {
    double r = 0.0;
};

struct RectV {
    double w = 0.0;
};

using ShapeIncomplete = std::variant<CircleV, RectV>;

struct SceneIncomplete {
    ShapeIncomplete shape;
};

struct SchemaIncompleteVariant : aison::Schema<SchemaIncompleteVariant> {
    static constexpr auto enableAssert = false;

    template<typename T>
    struct Object;
    template<typename T>
    struct Variant;
};

template<>
struct SchemaIncompleteVariant::Variant<ShapeIncomplete>
    : aison::Variant<SchemaIncompleteVariant, ShapeIncomplete> {
    static constexpr auto discriminator = "kind";

    Variant() { add<CircleV>("circle"); }  // missing RectV on purpose
};

template<>
struct SchemaIncompleteVariant::Object<CircleV>
    : aison::Object<SchemaIncompleteVariant, CircleV> {
    Object() { add(&CircleV::r, "r"); }
};

template<>
struct SchemaIncompleteVariant::Object<RectV>
    : aison::Object<SchemaIncompleteVariant, RectV> {
    Object() { add(&RectV::w, "w"); }
};

template<>
struct SchemaIncompleteVariant::Object<SceneIncomplete>
    : aison::Object<SchemaIncompleteVariant, SceneIncomplete> {
    Object() { add(&SceneIncomplete::shape, "shape"); }
};

TEST_SUITE("Runtime asserts (asserts disabled)")
{
    TEST_CASE("Duplicate field name is ignored when asserts disabled")
    {
        DupField d{};
        d.a = 1;
        d.b = 2;
        Json::Value json;
        auto enc = aison::encode<GuardSchema>(d, json);
        REQUIRE(enc);
        CHECK(enc.errors.empty());
        CHECK(json["value"].asInt() == 1);
    }

    TEST_CASE("Duplicate member mapping is ignored when asserts disabled")
    {
        DupMember m{};
        m.v = 7;

        Json::Value json;
        auto enc = aison::encode<GuardSchema>(m, json);
        REQUIRE(enc);
        CHECK(enc.errors.empty());
        CHECK(json.isMember("primary"));
        CHECK_FALSE(json.isMember("alias"));
        CHECK(json["primary"].asInt() == 7);
    }

    TEST_CASE("Incomplete variant mapping reports schema error")
    {
        SceneIncomplete scene;
        scene.shape = CircleV{3.0};

        Json::Value json;
        auto enc = aison::encode<SchemaIncompleteVariant>(scene, json);
        CHECK_FALSE(enc);
        REQUIRE_FALSE(enc.errors.empty());
        CHECK(enc.errors[0].message.find("Variant mapping missing alternatives") != std::string::npos);
    }
}  // TEST_SUITE

}  // namespace
