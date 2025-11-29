#include <aison/aison.h>

#include <iostream>

#include "types.h"

namespace example {

namespace {

// TextSchema

struct Config {
    bool upperCaseHex = false;
};

struct TextSchema : aison::Schema<TextSchema, aison::EncodeDecode, Config> {
    static constexpr auto enableAssert = false;
    static constexpr auto strictOptional = false;

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
struct TextSchema::Object<Text> : aison::Object<TextSchema, Text> {
    Object()
    {
        add(&Text::paragraphs, "paragraphs");
        add(&Text::bgColor, "bgColor");
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

// ColorSchema

struct ColorSchema : aison::Schema<ColorSchema, aison::EncodeOnly> {
    template<typename T>
    struct Object;

    template<typename T>
    struct Encoder;
};

template<>
struct ColorSchema::Encoder<Channels> : aison::Encoder<ColorSchema, Channels> {
    void operator()(const Channels& src, Json::Value& dst)
    {
        auto size = src.r.size();
        if (src.g.size() != size || src.b.size() != size) {
            addError("Color channels should have the same number of entries");
            return;
        }

        dst = Json::arrayValue;
        for (auto i = 0u; i < size; ++i) {
            auto& node = dst.append({});
            encode(RGBColor{src.r[i], src.g[i], src.b[i]}, node);
        }
    }
};

template<>
struct ColorSchema::Encoder<RGBColor> : aison::Encoder<ColorSchema, RGBColor> {
    void operator()(const RGBColor& src, Json::Value& dst) { dst = toHexColor(src, true); }
};

}  // namespace

void encoderExample1()
{
    Text text;
    auto& para = text.paragraphs.emplace_back();

    text.bgColor = RGBColor{0, 0, 100};
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
    auto res = aison::encode<TextSchema>(text, root, cfg);

    if (!res) {
        std::cout << "== Encode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
        return;
    }

    std::cout << "== Encoded ==\n";
    std::cout << root.toStyledString() << "\n\n";

    res = aison::decode<TextSchema>(root, text, cfg);
    if (!res) {
        std::cout << "== Decode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
    }

    std::cout << "== Decode success ==\n";
    Json::Value value;
    res = aison::encode<TextSchema>(text, value, cfg);
    std::cout << value.toStyledString() << "\n";
}

void encoderExample2()
{
    Channels chan;
    for (auto i = 0; i < 6; ++i) {
        chan.r.push_back(i * 48 % 256);
        chan.g.push_back((32 + i * 72) % 256);
        chan.b.push_back((96 + i * 42) % 256);
    }

    Json::Value root;
    auto res = aison::encode<ColorSchema>(chan, root);

    if (!res) {
        std::cout << "== Encode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
        return;
    }
    std::cout << "== Encoded ==\n";
    std::cout << root.toStyledString() << "\n\n";
}

}  // namespace example
