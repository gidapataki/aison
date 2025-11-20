# Aison Reference Manual

Aison is a small C++17 library for mapping C++ types to/from JsonCpp.  
Mappings are defined in a schema using regular C++ without macros, reflection, or code generation.

---

## 1. Schema

A schema defines how C++ types map to JSON.

```cpp
struct MySchema
    : aison::Schema<MySchema, aison::EncodeDecode/* optional */, MyConfig/* optional */>
{
    template<typename T> struct Object;       // struct mappings
    template<typename E> struct Enum;         // enum mappings
    template<typename T> struct Encoder;      // custom encode (optional)
    template<typename T> struct Decoder;      // custom decode (optional)
};
```

### 1.1 Facets

The schema’s facet controls which operations are allowed:

| Facet | Meaning |
|-------|---------|
| `EncodeOnly` | Only encoding is enabled. Decoding causes compile errors. |
| `DecodeOnly` | Only decoding is enabled. Encoding causes compile errors. |
| `EncodeDecode` | Both encode and decode are allowed (default). |

Custom encoders/decoders must match the facet  
(e.g., decode-only schema requires only `Decoder<T>` specializations).

### 1.2 Config

Any schema may define an optional config type:

```cpp
struct MyConfig {
    bool upperCaseHex = false;
};
```

A config object is passed to:

- custom encoders (`Schema::Encoder<T>`)
- custom decoders (`Schema::Decoder<T>`)

Example usage:

```cpp
MyConfig cfg{ .upperCaseHex = true };
aison::Result result = aison::decode<MySchema>(json, obj, cfg);
```

Config is available as `this->config()` inside custom encode/decode hooks.

---

## 2. Object Mapping (`Schema::Object<T>`)

Object mappings describe how C++ struct/class fields appear in JSON.

Example with a graphic **Shape** type:

```cpp
struct Label {
    std::string str;
    float x, y;
    float scale;
    Color color;
};
```

Mapping:

```cpp
template<>
struct MySchema::Object<Label>
    : aison::Object<MySchema, Label>
{
    Object() {
        add(&Label::str,   "str");
        add(&Label::x,     "x");
        add(&Label::y,     "y");
        add(&Label::scale, "scale");
        add(&Label::color, "color");
    }
};
```

### Rules

- Every struct field must be listed with `add(&T::member, "<name>")`.
- Every listed field is **required** during decoding.
- Nested objects and arrays work as expected (`std::vector<T>`, `std::optional<T>`).

---

## 3. Enum Mapping (`Schema::Enum<T>`)

Enums must be explicitly mapped:

```cpp
enum class Color { Red, Green, Blue };

template<>
struct MySchema::Enum<Color>
    : aison::Enum<MySchema, Color>
{
    Enum() {
        add(Color::Red,   "red");
        add(Color::Green, "green");
        add(Color::Blue,  "blue");
    }
};
```

### Rules

- Must list **every** enum value.
- Encoding produces the associated string.
- Decoding fails on unrecognized strings.
- Decoding fails on non-string JSON values.

---

## 4. Custom Encoders / Decoders

Use these when the default mapping is insufficient — for example, storing a color struct as a hex string.

```cpp
struct RgbColor { uint8_t r, g, b; };
```

### Custom encoder

```cpp
template<>
struct MySchema::Encoder<RgbColor>
    : aison::Encoder<MySchema, RgbColor>
{
    void operator()(const RgbColor& src, Json::Value& dst) {
        std::ostringstream ss;
        if (config().upperCaseHex) {
            ss << std::uppercase;
        }
        ss << "#" << std::hex << std::setfill('0')
           << std::setw(2) << int(src.r)
           << std::setw(2) << int(src.g)
           << std::setw(2) << int(src.b);
        dst = ss.str();
    }
};
```

### Custom decoder

```cpp
template<>
struct MySchema::Decoder<RgbColor>
    : aison::Decoder<MySchema, RgbColor>
{
    void operator()(const Json::Value& src, RgbColor& dst) {
        if (!src.isString()) {
            addError("Expected hex color string.");
            return;
        }

        const std::string& s = src.asString();
        if (s.size() != 7 || s[0] != '#') {
            addError("Invalid color format. Expected '#RRGGBB'.");
            return;
        }

        auto hex = [&](int i) {
            return std::stoi(s.substr(i, 2), nullptr, 16);
        };

        dst = RgbColor{ uint8_t(hex(1)), uint8_t(hex(3)), uint8_t(hex(5)) };
    }
};
```

### Notes

- `addError(msg)` attaches messages at the correct JSON path.
- `config()` gives access to the config object.

---

## 5. Encoding / Decoding Functions

### 5.1 With automatic EmptyConfig

```cpp
aison::Result r = aison::decode<MySchema>(json, label);
```

This can be used only when the schema was defined without a config type:
```cpp
struct MySchema : aison::Schema<MySchema> {
    ...
}
```

### 5.2 With explicit config

```cpp
MyConfig cfg;
aison::Result r = aison::encode<MySchema>(label, json, cfg);
aison::Result r = aison::decode<MySchema>(json, label, cfg);
```

`Result` contains *all* errors.

---

## 6. Error Model

Each error has:

- `path` — JSON-style location such as `$.labels[3].color`
- `message` — human-readable reason

Examples:

- `"Expected float."`
- `"Missing required field 'fill'."`
- `"Unknown enum value 'purple'."`
- `"Invalid color format. Expected '#RRGGBB'."`
- `"NaN is not allowed here."`

Aison accumulates all errors; it does not stop at the first failure.

---

## 7. Type Support Matrix

| Type | Encode | Decode | Notes |
|------|--------|--------|------|
| `bool` | ✔ | ✔ | |
| Integral types | ✔ | ✔ | Range-checked for unsigned |
| `float`, `double` | ✔ | ✔ | Rejects NaN |
| `std::string` | ✔ | ✔ | |
| `std::optional<T>` | ✔ | ✔ | Null = empty optional |
| `std::vector<T>` | ✔ | ✔ | Recursive support |
| Enums | ✔ | ✔ | Requires Enum mapping |
| Structs | ✔ | ✔ | Requires Object mapping |
| Custom types | ✔ | ✔ | Via custom Encoder/Decoder |
| Raw pointers | ⚠ Not supported by default | ⚠ Can be handled via custom encoder/decoder |

Pointers can be supported if the user *explicitly* maps them via custom coding.

---

## 8. Schema Validation

Aison validates compile-time schema correctness.

Common compile-time failures:

- **Missing Object<T> mapping**  
  > “Type is not mapped as an object. Either define … or provide a custom encoder.”

- **Missing Enum<E> mapping**  
  > “No schema enum mapping for this type.”

- **Facet mismatch**  
  > “DecoderImpl<Schema> cannot be used with an EncodeOnly schema facet.”

- **Missing custom encoder/decoder**  
  > “Unsupported type. Define a custom encoder as …”


---


## 10. Design Principles

- **Explicit schemas, no inference** — you always see exactly what is serialized.
- **Full error aggregation** — Aison does not stop early.
- **No runtime polymorphism** — no virtual functions.
- **Config-aware custom mappings** — configurable behaviors for advanced types.
- **Predictable & robust** — strong guards about schema mistakes both during compile-time and runtime.
