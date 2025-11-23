#include <aison/aison.h>
#include <doctest.h>
#include <json/json.h>

#include <variant>
#include <vector>

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
    Object()
    {
        add(&DupField::a, "value");
        add(&DupField::b, "value");  // duplicate name would assert with EnableAssert == true
    }
};

template<>
struct GuardSchema::Object<DupMember> : aison::Object<GuardSchema, DupMember> {
    Object()
    {
        add(&DupMember::v, "primary");
        add(&DupMember::v, "alias");  // duplicate member mapping would assert otherwise
    }
};

// Variant guard scenarios -----------------------------------------------------

struct Circle {
    double r = 0.0;
};

struct Rect {
    double w = 0.0;
};

using ShapeBadVariant = std::variant<Circle, Rect>;

struct SceneBad {
    ShapeBadVariant shape;
};

// Missing discriminator on Circle triggers validation; asserts disabled to avoid abort.
struct SchemaMissingDiscriminator : aison::Schema<SchemaMissingDiscriminator> {
    static constexpr auto enableAssert = false;

    template<typename T>
    struct Object;
};

template<>
struct SchemaMissingDiscriminator::Object<Circle>
    : aison::Object<SchemaMissingDiscriminator, Circle> {
    Object() { add(&Circle::r, "r"); }  // no discriminator()
};

template<>
struct SchemaMissingDiscriminator::Object<Rect> : aison::Object<SchemaMissingDiscriminator, Rect> {
    Object()
    {
        discriminator("rect", "kind");
        add(&Rect::w, "w");
    }
};

template<>
struct SchemaMissingDiscriminator::Object<SceneBad>
    : aison::Object<SchemaMissingDiscriminator, SceneBad> {
    Object() { add(&SceneBad::shape, "shape"); }
};

// Mismatched keys -------------------------------------------------------------

using ShapeMismatched = std::variant<Circle, Rect>;

struct SceneMismatch {
    std::vector<ShapeMismatched> shapes;
};

struct SchemaMismatchedKey : aison::Schema<SchemaMismatchedKey> {
    static constexpr auto enableAssert = false;

    template<typename T>
    struct Object;
};

template<>
struct SchemaMismatchedKey::Object<Circle> : aison::Object<SchemaMismatchedKey, Circle> {
    Object()
    {
        discriminator("circle", "kind");
        add(&Circle::r, "r");
    }
};

template<>
struct SchemaMismatchedKey::Object<Rect> : aison::Object<SchemaMismatchedKey, Rect> {
    Object()
    {
        discriminator("rect", "type");  // different key
        add(&Rect::w, "w");
    }
};

template<>
struct SchemaMismatchedKey::Object<SceneMismatch>
    : aison::Object<SchemaMismatchedKey, SceneMismatch> {
    Object() { add(&SceneMismatch::shapes, "shapes"); }
};

// Empty discriminator key -----------------------------------------------------

struct SchemaEmptyKey : aison::Schema<SchemaEmptyKey> {
    static constexpr auto enableAssert = false;

    template<typename T>
    struct Object;
};

template<>
struct SchemaEmptyKey::Object<Circle> : aison::Object<SchemaEmptyKey, Circle> {
    Object()
    {
        discriminator("circle", "");  // invalid empty key would assert if enabled
        add(&Circle::r, "r");
    }
};

template<>
struct SchemaEmptyKey::Object<Rect> : aison::Object<SchemaEmptyKey, Rect> {
    Object()
    {
        discriminator("rect", "");  // same invalid key
        add(&Rect::w, "w");
    }
};

template<>
struct SchemaEmptyKey::Object<SceneBad> : aison::Object<SchemaEmptyKey, SceneBad> {
    Object() { add(&SceneBad::shape, "shape"); }
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

    TEST_CASE(
        "Variant alternative missing discriminator yields runtime error when asserts disabled")
    {
        SceneBad scene;
        scene.shape = Circle{3.0};

        Json::Value json;
        auto enc = aison::encode<SchemaMissingDiscriminator>(scene, json);
        CHECK_FALSE(enc);
        REQUIRE_FALSE(enc.errors.empty());
    }

    TEST_CASE("Variant alternatives with mismatched discriminator keys fail validation")
    {
        SceneMismatch scene;
        scene.shapes.push_back(Circle{1.0});
        scene.shapes.push_back(Rect{2.0});

        Json::Value json;
        auto enc = aison::encode<SchemaMismatchedKey>(scene, json);
        CHECK_FALSE(enc);
        REQUIRE_FALSE(enc.errors.empty());
    }

    TEST_CASE("Empty discriminator key surfaces validation error when asserts disabled")
    {
        SceneBad scene;
        scene.shape = Rect{5.0};

        Json::Value json;
        auto enc = aison::encode<SchemaEmptyKey>(scene, json);
        CHECK_FALSE(enc);
        REQUIRE_FALSE(enc.errors.empty());
    }

}  // TEST_SUITE

}  // namespace
