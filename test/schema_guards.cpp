#include <doctest.h>
#include <json/json.h>

#include "aison/aison.h"

namespace {

// Basic schema for structural tests (no variants needed here)
struct GuardSchema : aison::Schema<GuardSchema> {
    using EnableAssert = std::false_type;  // to avoid aborting on asserts in tests
    template<typename T>
    struct Object;
    template<typename T>
    struct Enum;
};

// --- Types for field mapping tests ---

struct Size {
    int x = 0;
    int y = 0;
};

struct DuplicateName {
    int a = 0;
    int b = 0;
};

struct DuplicateMember {
    int v = 0;
};

enum class Mode { Normal, Debug, Legacy };

// Object and Enum definitions

template<>
struct GuardSchema::Object<Size> : aison::Object<GuardSchema, Size> {
    Object()
    {
        add(&Size::x, "x");
        add(&Size::y, "y");  // same type as x, ensure this is handled
    }
};

template<>
struct GuardSchema::Object<DuplicateName> : aison::Object<GuardSchema, DuplicateName> {
    Object()
    {
        add(&DuplicateName::a, "value");
        // second add() with same name should be ignored when EnableAssert == false
        add(&DuplicateName::b, "value");
    }
};

template<>
struct GuardSchema::Object<DuplicateMember> : aison::Object<GuardSchema, DuplicateMember> {
    Object()
    {
        add(&DuplicateMember::v, "primary");
        // same member under a different name should be ignored (second mapping)
        add(&DuplicateMember::v, "alias");
    }
};

template<>
struct GuardSchema::Enum<Mode> : aison::Enum<GuardSchema, Mode> {
    Enum()
    {
        add(Mode::Normal, "normal");
        add(Mode::Debug, "debug");
        add(Mode::Legacy, "legacy");
        // alias for debug
        addAlias(Mode::Debug, "dbg");
    }
};

// --- Tests ---

TEST_CASE("schema guards: multiple fields of same type work correctly")
{
    Size s{};
    s.x = 10;
    s.y = 20;

    Json::Value root;
    auto encResult = aison::encode<GuardSchema>(s, root);
    REQUIRE(encResult);
    REQUIRE(encResult.errors.empty());

    REQUIRE(root.isObject());
    CHECK(root["x"].isInt());
    CHECK(root["y"].isInt());
    CHECK(root["x"].asInt() == 10);
    CHECK(root["y"].asInt() == 20);

    // Round-trip
    Size decoded{};
    auto decResult = aison::decode<GuardSchema>(root, decoded);
    CHECK(decResult);
    CHECK(decResult.errors.empty());
    CHECK(decoded.x == 10);
    CHECK(decoded.y == 20);
}

TEST_CASE(
    "schema guards: duplicate field name in add() ignores second mapping when asserts disabled")
{
    DuplicateName obj{};
    obj.a = 1;
    obj.b = 2;

    Json::Value root;
    auto encResult = aison::encode<GuardSchema>(obj, root);
    REQUIRE(encResult);
    REQUIRE(encResult.errors.empty());

    // Only the first mapping ("value" -> a) should be active
    REQUIRE(root.isObject());
    CHECK(root.isMember("value"));
    CHECK(root["value"].asInt() == 1);

    // Decode: only 'a' will be set from JSON; 'b' remains default
    DuplicateName decoded{};
    auto decResult = aison::decode<GuardSchema>(root, decoded);
    CHECK(decResult);
    CHECK(decResult.errors.empty());

    CHECK(decoded.a == 1);
    CHECK(decoded.b == 0);
}

TEST_CASE(
    "schema guards: duplicate member pointer mapping is ignored for second add() when asserts "
    "disabled")
{
    DuplicateMember obj{};
    obj.v = 42;

    Json::Value root;
    auto encResult = aison::encode<GuardSchema>(obj, root);
    REQUIRE(encResult);
    REQUIRE(encResult.errors.empty());

    // Only the first mapping ("primary") should be present
    REQUIRE(root.isObject());
    CHECK(root.isMember("primary"));
    CHECK_FALSE(root.isMember("alias"));
    CHECK(root["primary"].asInt() == 42);

    // Decode: only "primary" is mapped, so alias doesn't exist
    DuplicateMember decoded{};
    auto decResult = aison::decode<GuardSchema>(root, decoded);
    CHECK(decResult);
    CHECK(decResult.errors.empty());
    CHECK(decoded.v == 42);
}

TEST_CASE("schema guards: enum alias decodes but primary name encodes")
{
    Json::Value root;
    Mode m = Mode::Debug;

    // Encode should use canonical name ("debug")
    auto encResult = aison::encode<GuardSchema>(m, root);
    REQUIRE(encResult);
    REQUIRE(encResult.errors.empty());
    REQUIRE(root.isString());
    CHECK(root.asString() == "debug");

    // Decode canonical name
    Mode decoded{};
    auto decResult1 = aison::decode<GuardSchema>(root, decoded);
    CHECK(decResult1);
    CHECK(decoded == Mode::Debug);

    // Decode alias name "dbg"
    Json::Value aliasValue("dbg");
    auto decResult2 = aison::decode<GuardSchema>(aliasValue, decoded);
    CHECK(decResult2);
    CHECK(decoded == Mode::Debug);

    // Unknown value should produce an error
    Json::Value badValue("unknown");
    auto decResult3 = aison::decode<GuardSchema>(badValue, decoded);
    CHECK_FALSE(decResult3);
    REQUIRE_FALSE(decResult3.errors.empty());
}

}  // namespace
