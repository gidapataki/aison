# Aison
pronounced /'eye-son/

Aison is a small C++17 library for mapping objects to JSON and back.  
Mappings are defined explicitly in a C++ schema, without macros, code
generation, or runtime reflection. The library is header-only and depends
only on JsonCpp.

### Features
- clear struct → JSON mappings
- strict decoding with useful error messages
- supports:
    - integral and floating-point types
    - enums
    - structs (via schema)
    - `std::string`
    - `std::optional`
    - `std::vector`
    - `std::variant` via discriminators
    - custom encode/decode hooks (optionally using a config object)
- schema properties:
    - non-intrusive definitions (no changes to your structs)
    - multiple schemas for the same type
    - schemas are fully independent

### Requirements
- C++17
- [JsonCpp](https://github.com/open-source-parsers/jsoncpp)

### Documentation

- [Reference Manual](REFERENCE.md)


### Advanced example
This example shows a realistic usecase where we want to encode text styling info into JSON. It shows how to use
- enum mapping
- struct mapping
- nested data via structs and `std::vector`
- custom encoder / decoder
- config object (used in custom encoder)
- error handling

#### Data
```C++
enum class Alignment {
    kLeft,
    kCenter,
    kRight,
};

struct RGBColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
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
```

#### Schema
```C++
struct Config {
    bool upperCaseHex = false;
};

struct TextSchema : aison::Schema<TextSchema, aison::EncodeDecode, Config> {
    template<typename T> struct Object;
    template<typename T> struct Enum;
    template<typename T> struct Encoder;
    template<typename T> struct Decoder;
};

template<>
struct TextSchema::Enum<Alignment> : aison::Enum<TextSchema, Alignment> {
    Enum() {
        add(Alignment::kLeft, "left");
        add(Alignment::kCenter, "center");
        add(Alignment::kRight, "right");
    }
};

template<>
struct TextSchema::Decoder<RGBColor> : aison::Decoder<TextSchema, RGBColor> {
    void operator()(const Json::Value& src, RGBColor& dst) {
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
    void operator()(const RGBColor& src, Json::Value& dst) {
        dst = toHexColor(src, config().upperCaseHex);
    }
};

template<>
struct TextSchema::Object<Span> : aison::Object<TextSchema, Span> {
    Object() {
        add(&Span::str, "str");
        add(&Span::color, "color");
        add(&Span::fontSize, "fontSize");
    }
};

template<>
struct TextSchema::Object<Paragraph> : aison::Object<TextSchema, Paragraph> {
    Object() {
        add(&Paragraph::spans, "spans");
        add(&Paragraph::alignment, "alignment");
    }
};

```
#### Usage
```C++
Json::Value root;
Config cfg{.upperCaseHex = true};
Paragraph para;
...
// Encode from para to root
if (auto res = aison::encode<TextSchema, Paragraph>(para, root, cfg)) {
    std::cout << root.toStyledString();
} else {
    for (auto& err : res.errors) {
        std::cerr << "error at " << err.path << ": " << err.message << "\n";
    }
}
...
// Decode from root to para
if (auto res = aison::decode<TextSchema, Paragraph>(root, para, cfg) {
    std::cout << "Success\n";
} else {
    for (auto& err : res.errors) {
        std::cerr << "error at " << err.path << ": " << err.message << "\n";
    }
}
```

### Polymorphism (`std::variant`)

```C++
enum class ShapeKind { kCircle, kRectangle };

struct Circle {
    float radius;
};

struct Rectangle {
    float width;
    float height;
};

using Shape = std::variant<Circle, Rectangle>;

struct ShapeSchema : aison::Schema<ShapeSchema> {
    static const auto discriminatorKey = "kind"; // (optional) sets default key for all types
    template<typename T> struct Object;
};

template<> struct ShapeSchema::Object<Circle> : aison::Object<ShapeSchema, Circle> {
    Object() {
        discriminator("circle");
        add(&Circle::radius, "radius");
    }
};

template<> struct ShapeSchema::Object<Rectangle> : aison::Object<ShapeSchema, Rectangle> {
    Object() {
        discriminator("rect", "kind"); // (optional) override default key for this type
        add(&Rectangle::width, "width");
        add(&Rectangle::height, "height");
    }
};
```

### Design principles

- C++ only - no RTTI, no virtual functions
- Non-intrusive and fully explicit schemas
- Strong compile-time and runtime guards against misuse
- All errors collected during encode/decode
- Support for encode-only and decode-only use cases
- Extensible via config objects and custom encoders / decoders

---

#### Note on the project's origin

Aison began as an experiment to see how quickly a library can be developed when
AI-assisted tooling is used to speed up exploration and rewriting. It was
eye-opening how fast different approaches could be tried and tested.

After a rapid prototyping phase, the library went through a full manual refactor,
followed by several rounds of review and polishing. The final result reflects
deliberate engineering decisions.
