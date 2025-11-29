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
    std::optional<RGBColor> bgColor;
};

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
        addAlias(Alignment::kCenter, "center2");
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
        add(&Paragraph::bgColor, "bgColor");
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

// ShapeSchema

struct Circle {
    float radius = 0;
};

struct Rectangle {
    float width = 0;
    float height = 0;
};

using Shape = std::variant<Circle, Rectangle>;

struct ShapeSchema : aison::Schema<ShapeSchema> {
    static constexpr auto discriminatorKey = "kind";

    template<typename T>
    struct Object;
};

template<>
struct ShapeSchema::Object<Circle> : aison::Object<ShapeSchema, Circle> {
    Object()
    {
        discriminator("circle");
        add(&Circle::radius, "radius");
    }
};

template<>
struct ShapeSchema::Object<Rectangle> : aison::Object<ShapeSchema, Rectangle> {
    Object()
    {
        discriminator("rect");
        add(&Rectangle::width, "width");
        add(&Rectangle::height, "height");
    }
};

// ColorSchema

struct Channels {
    std::vector<uint8_t> r, g, b;
};

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
    color.r = (colorValue >> 16) & 0xff;
    color.g = (colorValue >> 8) & 0xff;
    color.b = (colorValue >> 0) & 0xff;
    return {color};
}

void testTextSchema()
{
    Paragraph para;
    para.alignment = Alignment::kCenter;
    para.bgColor = RGBColor{0, 0, 100};

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
    auto res = aison::encode<TextSchema>(para, root, cfg);

    if (!res) {
        std::cout << "== Encode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
        return;
    }

    std::cout << "== Encoded ==\n";
    std::cout << root.toStyledString() << "\n\n";

    // Alias
    root["alignment"] = "center2";
    root["bgColor"] = Json::nullValue;

    res = aison::decode<TextSchema>(root, para, cfg);
    if (!res) {
        std::cout << "== Decode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
    }

    std::cout << "== Decode success ==\n";
    Json::Value value;
    res = aison::encode<TextSchema>(para, value, cfg);
    std::cout << value.toStyledString() << "\n";
}

void testShapeSchema()
{
    std::vector<Shape> shapes;
    shapes.push_back(Circle{.radius = 15});
    shapes.push_back(Rectangle{.width = 10, .height = 20});

    Json::Value root;
    auto res = aison::encode<ShapeSchema>(shapes, root);

    if (!res) {
        std::cout << "== Encode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
        return;
    }

    std::cout << "== Encoded ==\n";
    std::cout << root.toStyledString() << "\n\n";

    shapes = {};
    res = aison::decode<ShapeSchema>(root, shapes);

    if (!res) {
        std::cout << "== Decode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
        return;
    }

    std::cout << "== Decoded ==\n";
    std::cout << shapes.size() << "\n";
}

void testColorSchema()
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

int main()
{
    // testTextSchema();
    // testShapeSchema();
    testColorSchema();

    return 0;
}
