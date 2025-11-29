# Aison
pronounced /ˈeɪson/

Aison is a small C++17 library for mapping objects to JSON and back.  
Mappings are defined explicitly in a C++ schema, without macros, code
generation, or runtime reflection. The library is header-only and depends
only on JsonCpp.

### Features
- clear struct → JSON mappings
- strict decoding with useful error messages
- supports:
    - integral and floating-point types
    - `std::string`
    - `std::optional`
    - `std::vector`
    - custom enums & structs (via schema)
    - discriminated unions of schema-mapped objects (via `std::variant`)
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
- custom mapping
- config object (used in custom mapping)
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
    template<typename T> struct Custom;
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
struct TextSchema::Custom<RGBColor> : aison::Custom<TextSchema, RGBColor> {
    Custom() { name("Color"); }

    void encode(const RGBColor& src, Json::Value& dst) const {
        dst = toHexColor(src, config().upperCaseHex);
    }

    void decode(const Json::Value& src, RGBColor& dst) const {
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
struct Circle {
    float radius;
};

struct Rectangle {
    float width;
    float height;
};

using Shape = std::variant<Circle, Rectangle>;

struct ShapeSchema : aison::Schema<ShapeSchema> {
    template<typename T> struct Object;
    template<typename Variant> struct Variant;
};

template<>
struct ShapeSchema::Variant<Shape> : aison::Variant<ShapeSchema, Shape> {
    Variant()
    {
        name("Shape");                // optional variant label for introspection
        discriminator("kind");        // required discriminator key for this variant
    }
};

template<> struct ShapeSchema::Object<Circle> : aison::Object<ShapeSchema, Circle> {
    Object() {
        name("circle");               // used as discriminator tag
        add(&Circle::radius, "radius");
    }
};

template<> struct ShapeSchema::Object<Rectangle> : aison::Object<ShapeSchema, Rectangle> {
    Object() {
        name("rect");                 // used as discriminator tag
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
- Extensible via config objects and custom mappings

---

#### Note on the project's origin

Aison was inspired by Circe, and also started as an experiment to see how fast it can evolve
when AI-assisted tooling is used to speed up exploration and refactoring. It was eye-opening
to see how much difference it makes if we can quickly jump between different design ideas
without losing touch with the internal details.
