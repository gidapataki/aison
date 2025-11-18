#include <aison/aison.h>
#include <json/json.h>

#include <iomanip>
#include <iostream>
#include <sstream>

struct RGBColor;
std::string toHexColor(const RGBColor& color, bool upperCaseHex);
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

struct Config {
    bool upperCaseHex = false;
};

struct TextSchema : aison::Schema<TextSchema, aison::EncodeDecode, Config> {
    template<typename T>
    struct Object;

    template<typename T>
    struct Enum;

    template<typename T>
    struct Encoder;

    template<typename T>
    struct Decoder;
};

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
struct TextSchema::Decoder<RGBColor> : aison::Decoder<TextSchema, RGBColor> {
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
struct TextSchema::Encoder<RGBColor> : aison::Encoder<TextSchema, RGBColor> {
    void operator()(const RGBColor& src, Json::Value& dst)
    {
        dst = toHexColor(src, config().upperCaseHex);
    }
};

// Implementation

std::string toHexColor(const RGBColor& color, bool upperCaseHex)
{
    std::stringstream stream;
    if (upperCaseHex) {
        stream << std::uppercase;
    }
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
    Config cfg{.upperCaseHex = true};
    auto res = aison::encode<TextSchema, Paragraph>(para, root, cfg);

    if (res) {
        std::cout << "== Encoded ==\n";
        std::cout << root.toStyledString() << "\n\n";

#if 1
        auto res2 = aison::decode<TextSchema, Paragraph>(root, para, cfg);
        if (res2) {
            std::cout << "== Decode success ==\n";
        } else {
            std::cout << "== Decode error ==\n";
            for (auto& err : res2.errors) {
                std::cout << err.path << ": " << err.message << "\n";
            }
        }
#endif
    } else {
        std::cout << "== Encode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
    }

    return 0;
}
