// tests/aison_tests.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <aison/aison.h>
#include <json/json.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// ------------------------------------------------------------
// Test domain types
// ------------------------------------------------------------

enum class Mode { Off, On, Auto };

struct RgbColor {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct Foo {
    int id = 0;
    std::string name;            // default empty
    std::optional<bool> flagOpt; // default disengaged
    std::vector<int> samples;    // default empty
};

struct Obj {
    int intValue = 0;
    float floatValue = 0.0f;
    bool boolValue = false;
    std::string strValue;              // ""
    std::vector<int> intArray;         // []
    std::vector<bool> boolArray;       // []
    std::optional<std::string> strOpt; // nullopt
    Foo foo;                           // nested
    std::vector<Foo> fooArray;         // []
    std::optional<Foo> fooOpt;         // nullopt
    Mode enumValue = Mode::Off;
    RgbColor colorValue; // {0,0,0}
};

// encode-only / decode-only types wrapping color
struct EncodeOnlyColorHolder {
    RgbColor color;
};

struct DecodeOnlyColorHolder {
    RgbColor color;
};

// ------------------------------------------------------------
// Shared helpers: enum map + color encode/decode
// ------------------------------------------------------------

struct ModeEnumMap {
    static constexpr std::array<std::pair<Mode, std::string_view>, 3> mapping{{
        {Mode::Off, "off"},
        {Mode::On, "on"},
        {Mode::Auto, "auto"},
    }};
};

// Reusable color codec helpers, templated on Schema so we can call them
// from SchemaFull and SchemaPartial.
template <typename Schema>
void encodeColorCommon(const RgbColor& src, Json::Value& dst, aison::Encoder<Schema>&) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", static_cast<unsigned>(src.r),
                  static_cast<unsigned>(src.g), static_cast<unsigned>(src.b));
    dst = std::string(buf);
}

template <typename Schema>
void decodeColorCommon(const Json::Value& src, RgbColor& dst, aison::Decoder<Schema>& dec) {
    if (!src.isString()) {
        dec.addError("Expected hex RGB string");
        return;
    }

    std::string s = src.asString();
    if (s.size() != 7 || s[0] != '#') {
        dec.addError("Invalid RGB hex format");
        return;
    }

    auto hexByte = [&](int start, std::uint8_t& out) -> bool {
        std::string sub = s.substr(start, 2);
        char* end = nullptr;
        long v = std::strtol(sub.c_str(), &end, 16);
        if (!end || *end != '\0' || v < 0 || v > 255) {
            dec.addError("Invalid RGB component");
            return false;
        }
        out = static_cast<std::uint8_t>(v);
        return true;
    };

    if (!hexByte(1, dst.r))
        return;
    if (!hexByte(3, dst.g))
        return;
    if (!hexByte(5, dst.b))
        return;
}

// ------------------------------------------------------------
// SchemaFull: uses all Obj fields
// ------------------------------------------------------------

struct SchemaFull {
    template <typename T> struct Fields;

    template <typename E> struct Enum;

    // Color custom primitive
    static void encodeValue(const RgbColor& src, Json::Value& dst,
                            aison::Encoder<SchemaFull>& enc) {
        encodeColorCommon<SchemaFull>(src, dst, enc);
    }

    static void decodeValue(const Json::Value& src, RgbColor& dst,
                            aison::Decoder<SchemaFull>& dec) {
        decodeColorCommon<SchemaFull>(src, dst, dec);
    }
};

template <> struct SchemaFull::Enum<Mode> {
    static constexpr auto& mapping = ModeEnumMap::mapping;
};

template <> struct SchemaFull::Fields<Foo> : aison::Fields<SchemaFull, Foo, aison::encodeDecode> {
    Fields() {
        add(&Foo::id, "id");
        add(&Foo::name, "name");
        add(&Foo::flagOpt, "flagOpt");
        add(&Foo::samples, "samples");
    }
};

template <> struct SchemaFull::Fields<Obj> : aison::Fields<SchemaFull, Obj, aison::encodeDecode> {
    Fields() {
        add(&Obj::intValue, "intValue");
        add(&Obj::floatValue, "floatValue");
        add(&Obj::boolValue, "boolValue");
        add(&Obj::strValue, "strValue");
        add(&Obj::intArray, "intArray");
        add(&Obj::boolArray, "boolArray");
        add(&Obj::strOpt, "strOpt");
        add(&Obj::foo, "foo");
        add(&Obj::fooArray, "fooArray");
        add(&Obj::fooOpt, "fooOpt");
        add(&Obj::enumValue, "enumValue");
        add(&Obj::colorValue, "colorValue");
    }
};

// Encode-only & decode-only holders of RgbColor

template <>
struct SchemaFull::Fields<EncodeOnlyColorHolder>
    : aison::Fields<SchemaFull, EncodeOnlyColorHolder, aison::encodeOnly> {
    Fields() { add(&EncodeOnlyColorHolder::color, "color"); }
};

template <>
struct SchemaFull::Fields<DecodeOnlyColorHolder>
    : aison::Fields<SchemaFull, DecodeOnlyColorHolder, aison::decodeOnly> {
    Fields() { add(&DecodeOnlyColorHolder::color, "color"); }
};

// ------------------------------------------------------------
// SchemaPartial: only uses a subset of Obj fields
// ------------------------------------------------------------

struct SchemaPartial {
    template <typename T> struct Fields;

    template <typename E> struct Enum;

    // Reuse the same float and color codec, but templated on SchemaPartial
    static void encodeValue(const float& src, Json::Value& dst, aison::Encoder<SchemaPartial>&) {
        dst = static_cast<double>(src);
    }

    static void decodeValue(const Json::Value& src, float& dst,
                            aison::Decoder<SchemaPartial>& dec) {
        if (!src.isDouble() && !src.isInt()) {
            dec.addError("Expected float/double");
            return;
        }
        dst = static_cast<float>(src.asDouble());
    }

    static void encodeValue(const RgbColor& src, Json::Value& dst,
                            aison::Encoder<SchemaPartial>& enc) {
        encodeColorCommon<SchemaPartial>(src, dst, enc);
    }

    static void decodeValue(const Json::Value& src, RgbColor& dst,
                            aison::Decoder<SchemaPartial>& dec) {
        decodeColorCommon<SchemaPartial>(src, dst, dec);
    }
};

template <> struct SchemaPartial::Enum<Mode> {
    static constexpr auto& mapping = ModeEnumMap::mapping;
};

// For partial schema, we only care about a few fields (e.g. core config):
// - intValue
// - foo.id
// - enumValue
// - colorValue
template <>
struct SchemaPartial::Fields<Foo> : aison::Fields<SchemaPartial, Foo, aison::encodeDecode> {
    Fields() {
        add(&Foo::id, "id"); // only id, ignore others
    }
};

template <>
struct SchemaPartial::Fields<Obj> : aison::Fields<SchemaPartial, Obj, aison::encodeDecode> {
    Fields() {
        add(&Obj::intValue, "intValue");
        add(&Obj::foo, "foo"); // but only Foo::id is used
        add(&Obj::enumValue, "enumValue");
        add(&Obj::colorValue, "colorValue");
    }
};

// ------------------------------------------------------------
// Helper to build a fully-populated Obj
// ------------------------------------------------------------

static Obj makeSampleObj() {
    Obj o{}; // zero-initialize everything first

    o.intValue = 42;
    o.floatValue = 1.5f;
    o.boolValue = true;
    o.strValue = "hello";

    o.intArray = {1, 2, 3};
    o.boolArray = {true, false, true};

    o.strOpt = std::string("optional");

    o.foo.id = 7;
    o.foo.name = "fooName";
    o.foo.flagOpt = false;
    o.foo.samples = {10, 20};

    Foo f1;
    f1.id = 1;
    f1.name = "one";
    f1.samples = {1};

    Foo f2;
    f2.id = 2;
    f2.name = "two";
    f2.flagOpt = true;
    f2.samples = {2, 3};

    o.fooArray = {f1, f2};

    Foo fOpt;
    fOpt.id = 99;
    fOpt.name = "opt";
    fOpt.samples = {9, 9};
    o.fooOpt = fOpt;

    o.enumValue = Mode::Auto;
    o.colorValue = RgbColor{0x12, 0x34, 0x56};

    return o;
}

// ------------------------------------------------------------
// Tests
// ------------------------------------------------------------

TEST_SUITE("aison / Obj end-to-end") {

    TEST_CASE("Happy path: full schema roundtrip") {
        Obj in = makeSampleObj();

        Json::Value root;
        auto encRes = aison::encode<SchemaFull>(in, root);
        REQUIRE(encRes);
        CHECK(encRes.errors.empty());

        // Inspect JSON shapes (not serialized strings)
        CHECK(root["intValue"].isInt());
        CHECK(root["boolValue"].isBool());
        CHECK(root["strValue"].isString());
        CHECK(root["intArray"].isArray());
        CHECK(root["boolArray"].isArray());
        CHECK(root["strOpt"].isString());
        CHECK(root["foo"].isObject());
        CHECK(root["fooArray"].isArray());
        CHECK(root["fooOpt"].isObject());
        CHECK(root["enumValue"].isString());
        CHECK(root["colorValue"].isString());

        // Decode back
        Obj out{};
        auto decRes = aison::decode<SchemaFull>(root, out);
        REQUIRE(decRes);
        CHECK(decRes.errors.empty());

        CHECK(out.intValue == in.intValue);
        CHECK(out.floatValue == doctest::Approx(in.floatValue));
        CHECK(out.boolValue == in.boolValue);
        CHECK(out.strValue == in.strValue);

        CHECK(out.intArray == in.intArray);
        CHECK(out.boolArray.size() == in.boolArray.size());
        for (std::size_t i = 0; i < out.boolArray.size(); ++i) {
            CHECK(static_cast<bool>(out.boolArray[i]) == static_cast<bool>(in.boolArray[i]));
        }

        REQUIRE(out.strOpt.has_value());
        CHECK(out.strOpt.value() == in.strOpt.value());

        CHECK(out.foo.id == in.foo.id);
        CHECK(out.foo.name == in.foo.name);
        CHECK(out.foo.samples == in.foo.samples);
        CHECK(out.foo.flagOpt == in.foo.flagOpt);

        CHECK(out.fooArray.size() == in.fooArray.size());
        CHECK(out.fooArray[0].id == in.fooArray[0].id);
        CHECK(out.fooArray[1].name == in.fooArray[1].name);

        REQUIRE(out.fooOpt.has_value());
        CHECK(out.fooOpt->id == in.fooOpt->id);
        CHECK(out.enumValue == in.enumValue);

        CHECK(out.colorValue.r == in.colorValue.r);
        CHECK(out.colorValue.g == in.colorValue.g);
        CHECK(out.colorValue.b == in.colorValue.b);
    }

    TEST_CASE("Field-level decode errors for each Obj field") {
        Obj in = makeSampleObj();
        Json::Value base;
        REQUIRE(aison::encode<SchemaFull>(in, base));

        Obj out{};

        SUBCASE("intValue wrong type") {
            Json::Value root = base;
            root["intValue"] = "oops";
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.intValue");
        }

        SUBCASE("intValue missing") {
            Json::Value root = base;
            root.removeMember("intValue");
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$");
            CHECK(res.errors[0].message.find("Missing required field: intValue") !=
                  std::string::npos);
        }

        SUBCASE("floatValue wrong type") {
            Json::Value root = base;
            root["floatValue"] = Json::Value("x");
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.floatValue");
        }

        SUBCASE("boolValue wrong type") {
            Json::Value root = base;
            root["boolValue"] = 123;
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.boolValue");
        }

        SUBCASE("strValue wrong type") {
            Json::Value root = base;
            root["strValue"] = false;
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.strValue");
        }

        SUBCASE("intArray wrong type") {
            Json::Value root = base;
            root["intArray"] = Json::Value("not-array");
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.intArray");
            CHECK(res.errors[0].message.find("Expected array") != std::string::npos);
        }

        SUBCASE("intArray element wrong type") {
            Json::Value root = base;
            root["intArray"][1] = Json::Value("bad");
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.intArray[1]");
            CHECK(res.errors[0].message.find("Expected integer") != std::string::npos);
        }

        SUBCASE("boolArray wrong type") {
            Json::Value root = base;
            root["boolArray"] = 42;
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.boolArray");
        }

        SUBCASE("boolArray element wrong type") {
            Json::Value root = base;
            root["boolArray"][0] = Json::Value("nope");
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.boolArray[0]");
        }

        SUBCASE("strOpt wrong type (non-null)") {
            Json::Value root = base;
            root["strOpt"] = Json::Value(123);
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.strOpt");
            CHECK(res.errors[0].message.find("Expected string") != std::string::npos);
        }

        SUBCASE("strOpt missing => required error") {
            Json::Value root = base;
            root.removeMember("strOpt");
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$");
            CHECK(res.errors[0].message.find("Missing required field: strOpt") !=
                  std::string::npos);
        }

        SUBCASE("foo wrong type") {
            Json::Value root = base;
            root["foo"] = Json::Value("not-object");
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.foo");
            CHECK(res.errors[0].message.find("Expected object") != std::string::npos);
        }

        SUBCASE("fooArray wrong type") {
            Json::Value root = base;
            root["fooArray"] = Json::Value(true);
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.fooArray");
        }

        SUBCASE("fooArray element wrong type") {
            Json::Value root = base;
            root["fooArray"][0] = Json::Value("not-object");
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.fooArray[0]");
        }

        SUBCASE("fooOpt wrong type (non-null)") {
            Json::Value root = base;
            root["fooOpt"] = Json::Value(12);
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.fooOpt");
        }

        SUBCASE("enumValue wrong type") {
            Json::Value root = base;
            root["enumValue"] = Json::Value(5);
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.enumValue");
            CHECK(res.errors[0].message.find("Expected string for enum") != std::string::npos);
        }

        SUBCASE("enumValue unknown string") {
            Json::Value root = base;
            root["enumValue"] = Json::Value("unknown-mode");
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.enumValue");
            CHECK(res.errors[0].message.find("Unknown enum value") != std::string::npos);
        }

        SUBCASE("colorValue wrong type") {
            Json::Value root = base;
            root["colorValue"] = Json::Value(true);
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.colorValue");
            CHECK(res.errors[0].message.find("Expected hex RGB string") != std::string::npos);
        }

        SUBCASE("colorValue malformed hex") {
            Json::Value root = base;
            root["colorValue"] = Json::Value("#12"); // too short
            auto res = aison::decode<SchemaFull>(root, out);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$.colorValue");
        }
    }

    TEST_CASE("Zero-init & partial schema: unused fields stay default") {
        Obj in = makeSampleObj();

        Json::Value root;
        REQUIRE(aison::encode<SchemaFull>(in, root));

        Obj out{}; // zero-initialized
        auto res = aison::decode<SchemaPartial>(root, out);
        REQUIRE(res);
        CHECK(res.errors.empty());

        // Fields used by SchemaPartial
        CHECK(out.intValue == in.intValue);
        CHECK(out.foo.id == in.foo.id);
        CHECK(out.enumValue == in.enumValue);
        CHECK(out.colorValue.r == in.colorValue.r);
        CHECK(out.colorValue.g == in.colorValue.g);
        CHECK(out.colorValue.b == in.colorValue.b);

        // Fields NOT in SchemaPartial should remain at their default (from Obj{})
        CHECK(out.floatValue == 0.0f);
        CHECK(out.boolValue == false);
        CHECK(out.strValue.empty());
        CHECK(out.intArray.empty());
        CHECK(out.boolArray.empty());
        CHECK(!out.strOpt.has_value());
        CHECK(out.foo.name.empty());
        CHECK(out.foo.samples.empty());
        CHECK(!out.foo.flagOpt.has_value());
        CHECK(out.fooArray.empty());
        CHECK(!out.fooOpt.has_value());
    }

    TEST_CASE("encodeOnly and decodeOnly holders with RgbColor") {
        // Encode-only holder
        EncodeOnlyColorHolder encHolder{};
        encHolder.color = RgbColor{0x12, 0x34, 0x56};

        Json::Value root;
        auto encRes = aison::encode<SchemaFull>(encHolder, root);
        REQUIRE(encRes);
        CHECK(encRes.errors.empty());
        CHECK(root["color"].isString());
        CHECK(root["color"].asString() == "#123456");

        // Decode-only holder
        DecodeOnlyColorHolder decHolder{};
        Json::Value src = Json::objectValue;
        src["color"] = "#ABCDEF";

        auto decRes = aison::decode<SchemaFull>(src, decHolder);
        REQUIRE(decRes);
        CHECK(decRes.errors.empty());
        CHECK(decHolder.color.r == 0xAB);
        CHECK(decHolder.color.g == 0xCD);
        CHECK(decHolder.color.b == 0xEF);
    }

    TEST_CASE("Root is not an object for a reflected type") {
        Obj out{};
        Json::Value root = Json::arrayValue; // wrong top-level type
        auto res = aison::decode<SchemaFull>(root, out);
        CHECK_FALSE(res);
        REQUIRE_FALSE(res.errors.empty());
        CHECK(res.errors[0].path == "$");
        CHECK(res.errors[0].message.find("Expected object") != std::string::npos);
    }

} // TEST_SUITE
