#include <aison/aison.h>
#include <json/json.h>

#include <iomanip>
#include <iostream>
#include <sstream>

struct RGBColor;
std::string toHexColor(const RGBColor& color);
std::optional<RGBColor> toRGBColor(const std::string& str);

struct RGBColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

enum class Alignment {
    kLeft,
    kCenter,
    kRight,
};

struct Span {
    std::string str;
    RGBColor color;
    float fontSize = 24.f;
};

struct Paragraph {
    std::vector<Span> spans;
    Alignment alignment = {};
};

struct TextSchema : aison::Schema<TextSchema, aison::Facet::EncodeDecode> {};

template<>
struct TextSchema::Enum<Alignment> : aison::Enum<TextSchema, Alignment> {
    Enum()
    {
        add(Alignment::kLeft, "left");
        add(Alignment::kCenter, "center");
        add(Alignment::kRight, "right");
    }
};

template<>
struct TextSchema::Object<Span> : aison::Object<TextSchema, Span> {
    Object()
    {
        add(&Span::str, "str");
        add(&Span::color, "color");
        add(&Span::fontSize, "fontSize");
    }
};

template<>
struct TextSchema::Object<Paragraph> : aison::Object<TextSchema, Paragraph> {
    Object()
    {
        add(&Paragraph::spans, "spans");
        add(&Paragraph::alignment, "alignment");
    }
};

template<>
struct TextSchema::CustomDecoder<RGBColor> : aison::CustomDecoder<TextSchema, RGBColor> {
    void operator()(const Json::Value& src, RGBColor& dst)
    {
        if (!src.isString()) {
            addError("String field required");
            return;
        }
        if (auto value = toRGBColor(src.asString()); value) {
            dst = *value;
        } else {
            addError("Could not parse value for RGBColor");
        }
    }
};
template<>
struct TextSchema::CustomEncoder<RGBColor> : aison::CustomEncoder<TextSchema, RGBColor> {
    void operator()(const RGBColor& src, Json::Value& dst) { dst = toHexColor(src); }
};

// Implementation

std::string toHexColor(const RGBColor& color)
{
    std::stringstream stream;
    stream << '#' << std::hex << std::setfill('0');
    stream << std::setw(2) << int(color.r);
    stream << std::setw(2) << int(color.g);
    stream << std::setw(2) << int(color.b);
    return stream.str();
}

std::optional<RGBColor> toRGBColor(const std::string& str)
{
    if (str.size() != 7 || str[0] != '#') {
        return std::nullopt;
    }

    for (int i = 1; i < 7; ++i) {
        if (!std::isxdigit(str[i])) {
            return std::nullopt;
        }
    }

    const char* start = str.c_str();
    char* end = nullptr;
    auto colorValue = std::strtoul(start + 1, &end, 16);

    RGBColor color;
    color.r = (colorValue >> 24) & 0xff;
    color.g = (colorValue >> 16) & 0xff;
    color.b = (colorValue >> 8) & 0xff;
    return {color};
}

int main()
{
    Paragraph para;
    para.alignment = Alignment::kCenter;

    if (auto& span = para.spans.emplace_back(); true) {
        span.color = {0x92, 0xca, 0x30};
        span.fontSize = 32;
        span.str = "Hi ";
    }

    if (auto& span = para.spans.emplace_back(); true) {
        span.color = {0x00, 0x20, 0xf3};
        span.fontSize = 24;
        span.str = "mom";
    }

    Json::Value root;
    auto res = aison::encode<TextSchema, Paragraph>(para, root);

    if (res) {
        std::cout << "== Encoded ==\n";
        std::cout << root.toStyledString() << "\n\n";

        auto res2 = aison::decode<TextSchema, Paragraph>(root, para);
        if (res2) {
            std::cout << "== Decode success ==\n";
        } else {
            std::cout << "== Decode error ==\n";
            for (auto& err : res2.errors) {
                std::cout << err.path << ": " << err.message << "\n";
            }
        }
    } else {
        std::cout << "== Encode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
    }

    return 0;
}

#if 0
// -------------------- Enum + types --------------------
enum class Kind { Unknown, Foo, Bar };

struct RGB {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
};

struct Stats {
    Kind kind{};

    struct Nested {
        int x{};
        std::string y;
    } nested;

    float f = 0;
    uint8_t u8 = 33;
    std::vector<int> ls;
    std::optional<int> maybe;
    RGB color;
};

// -------------------- Schema --------------------
struct SchemaA {
    template<typename T>
    struct Object;

    template<typename E>
    struct Enum;

    struct Config : aison::Config {
        int version = 0;
    };

    // Custom primitive encodings (RGB as hex)
    static void encodeValue(const RGB& src, Json::Value& dst, aison::Encoder<SchemaA>& enc)
    {
        std::ostringstream oss;
        oss << '#' << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(src.r)
            << std::setw(2) << static_cast<int>(src.g) << std::setw(2) << static_cast<int>(src.b);
        dst = oss.str();
    }

    static void decodeValue(const Json::Value& src, RGB& dst, aison::Decoder<SchemaA>& dec)
    {
        if (!src.isString()) {
            dec.addError("Expected hex RGB string");
            return;
        }

        std::string s = src.asString();
        if (s.size() != 7 || s[0] != '#') {
            dec.addError("Invalid RGB hex format");
            return;
        }

        auto hexByte = [&](int start, unsigned char& out) {
            std::string sub = s.substr(start, 2);
            char* end = nullptr;
            long v = std::strtol(sub.c_str(), &end, 16);
            if (!end || *end != '\0' || v < 0 || v > 255) {
                dec.addError("Invalid RGB component");
                return false;
            }
            out = static_cast<unsigned char>(v);
            return true;
        };

        if (!hexByte(1, dst.r)) return;
        if (!hexByte(3, dst.g)) return;
        if (!hexByte(5, dst.b)) return;
    }
};

template<>
struct SchemaA::Enum<Kind> : aison::Enum<SchemaA, Kind> {
    Enum()
    {
        add(Kind::Unknown, "unknown");
        add(Kind::Foo, "foo");
        add(Kind::Bar, "bar");
    }
};

template<>
struct SchemaA::Object<Stats> : aison::Object<SchemaA, Stats, aison::EncodeDecode> {
    Object()
    {
        add(&Stats::kind, "kind");
        add(&Stats::nested, "nested");
        add(&Stats::ls, "ls");
        add(&Stats::maybe, "maybe");
        add(&Stats::color, "color");
        add(&Stats::f, "f");
        add(&Stats::u8, "u8");
    }
};

template<>
struct SchemaA::Object<Stats::Nested> : aison::Object<SchemaA, Stats::Nested, aison::EncodeDecode> {
    Object()
    {
        add(&Stats::Nested::x, "x");
        add(&Stats::Nested::y, "y");
    }
};

// main

int main()
{
    Stats s;
    s.kind = Kind::Foo;
    s.nested.x = 7;
    s.nested.y = "hello";
    s.ls = {1, 2, 3};
    s.maybe = 99;
    s.color = RGB{0x12, 0x34, 0xAB};
    s.f = NAN;

    Json::Value root;

    // Using Encoder directly
    aison::Encoder<SchemaA> enc({});
    aison::Result er = enc.encode(s, root);

    std::cout << "== Encoded ==\n" << root.toStyledString() << "\n\n";

    if (!er) {
        for (const auto& e : er.errors) {
            std::cerr << e.path << ": " << e.message << "\n";
        }
    }

#if 1
    root["u8"] = 333;
    Stats out{};
    aison::Result dr = aison::decode<SchemaA>(root, out, {.version = 55});

    if (!dr) {
        std::cerr << "== Decode errors ==\n";
        for (const auto& e : dr.errors) {
            std::cerr << e.path << ": " << e.message << "\n";
        }
    } else {
        std::cout << "== Decoded ==\n";
    }
#endif
}
#endif