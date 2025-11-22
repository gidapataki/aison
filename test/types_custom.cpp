#include <aison/aison.h>
#include <doctest.h>
#include <json/json.h>

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

// --- Encode-only schema ------------------------------------------------------

struct EncodeOnlyText {
    std::string value;
};

struct EncodeOnlySchema : aison::Schema<EncodeOnlySchema, aison::EncodeOnly> {
    template<typename T>
    struct Encoder;
};

template<>
struct EncodeOnlySchema::Encoder<EncodeOnlyText>
    : aison::Encoder<EncodeOnlySchema, EncodeOnlyText> {
    void operator()(const EncodeOnlyText& src, Json::Value& dst) { dst = src.value + "!"; }
};

// --- Decode-only schema ------------------------------------------------------

struct DecodeOnlyNumber {
    int value = 0;
};

struct DecodeOnlySchema : aison::Schema<DecodeOnlySchema, aison::DecodeOnly> {
    template<typename T>
    struct Decoder;
};

template<>
struct DecodeOnlySchema::Decoder<DecodeOnlyNumber>
    : aison::Decoder<DecodeOnlySchema, DecodeOnlyNumber> {
    void operator()(const Json::Value& src, DecodeOnlyNumber& dst)
    {
        // Accept strings in the form "num:<int>"
        if (!src.isString()) {
            addError("Expected tagged string");
            return;
        }
        std::string s = src.asString();
        const std::string prefix = "num:";
        auto pos = s.find(prefix);
        if (pos != 0) {
            addError("Missing num: prefix");
            return;
        }
        try {
            dst.value = std::stoi(s.substr(prefix.size()));
        } catch (...) {
            addError("Invalid integer payload");
        }
    }
};

// --- Config-aware schema with custom encoder/decoder ------------------------

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct PaletteDoc {
    Color primary;
    std::optional<Color> accent;
    std::vector<Color> palette;
};

struct ColorConfig {
    bool upperHex = false;
};

static std::string toHex(const Color& c, bool upper)
{
    char buf[8];
    const char* fmt = upper ? "#%02X%02X%02X" : "#%02x%02x%02x";
    std::snprintf(
        buf, sizeof(buf), fmt, static_cast<unsigned>(c.r), static_cast<unsigned>(c.g),
        static_cast<unsigned>(c.b));
    return std::string(buf);
}

static bool parseHex(const std::string& s, Color& out)
{
    if (s.size() != 7 || s[0] != '#') return false;
    auto hexByte = [&](int start, std::uint8_t& dst) -> bool {
        std::string sub = s.substr(start, 2);
        char* end = nullptr;
        long v = std::strtol(sub.c_str(), &end, 16);
        if (end == nullptr || *end != '\0' || v < 0 || v > 255) return false;
        dst = static_cast<std::uint8_t>(v);
        return true;
    };
    if (!hexByte(1, out.r)) return false;
    if (!hexByte(3, out.g)) return false;
    if (!hexByte(5, out.b)) return false;
    return true;
}

struct ColorSchema : aison::Schema<ColorSchema, aison::EncodeDecode, ColorConfig> {
    template<typename T>
    struct Object;
    template<typename T>
    struct Encoder;
    template<typename T>
    struct Decoder;
};

template<>
struct ColorSchema::Encoder<Color> : aison::Encoder<ColorSchema, Color> {
    void operator()(const Color& src, Json::Value& dst) { dst = toHex(src, config().upperHex); }
};

template<>
struct ColorSchema::Decoder<Color> : aison::Decoder<ColorSchema, Color> {
    void operator()(const Json::Value& src, Color& dst)
    {
        if (!src.isString()) {
            addError("Expected hex string");
            return;
        }
        std::string s = src.asString();
        if (!parseHex(s, dst)) {
            addError("Invalid hex color");
        }
    }
};

template<>
struct ColorSchema::Object<PaletteDoc> : aison::Object<ColorSchema, PaletteDoc> {
    Object()
    {
        add(&PaletteDoc::primary, "primary");
        add(&PaletteDoc::accent, "accent");
        add(&PaletteDoc::palette, "palette");
    }
};

// --- Tests -------------------------------------------------------------------

TEST_SUITE("Custom encoders/decoders")
{
    TEST_CASE("Encode-only schema uses custom encoder")
    {
        EncodeOnlyText t;
        t.value = "hello";

        Json::Value json;
        auto res = aison::encode<EncodeOnlySchema>(t, json);
        REQUIRE(res);
        CHECK(res.errors.empty());
        CHECK(json.isString());
        CHECK(json.asString() == "hello!");
    }

    TEST_CASE("Decode-only schema uses custom decoder")
    {
        Json::Value json("num:42");
        DecodeOnlyNumber out;
        auto res = aison::decode<DecodeOnlySchema>(json, out);
        REQUIRE(res);
        CHECK(res.errors.empty());
        CHECK(out.value == 42);
    }

    TEST_CASE("Config-aware custom encode/decode round-trip")
    {
        PaletteDoc doc;
        doc.primary = Color{0x12, 0x34, 0x56};
        doc.accent = Color{0xAA, 0xBB, 0xCC};
        doc.palette = {Color{0, 0, 0}, Color{0xFF, 0xEE, 0xDD}};

        ColorConfig cfg;
        cfg.upperHex = true;  // ensures encoder uses uppercase hex

        Json::Value json;
        auto enc = aison::encode<ColorSchema>(doc, json, cfg);
        REQUIRE(enc);
        CHECK(enc.errors.empty());

        CHECK(json["primary"].asString() == "#123456");  // uppercase expectation

        PaletteDoc decoded;
        auto dec = aison::decode<ColorSchema>(json, decoded, cfg);

        REQUIRE(dec);
        CHECK(dec.errors.empty());

        CHECK(decoded.primary.r == doc.primary.r);
        CHECK(decoded.primary.g == doc.primary.g);
        CHECK(decoded.primary.b == doc.primary.b);
        REQUIRE(decoded.accent.has_value());
        CHECK(decoded.accent->r == doc.accent->r);
        CHECK(decoded.palette.size() == doc.palette.size());
        CHECK(decoded.palette[1].b == doc.palette[1].b);
    }
}

}  // namespace
