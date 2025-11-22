# Aison Reference Manual

Aison is a small C++17 library for mapping well‑structured C++ types to/from JsonCpp.  
Mappings are defined in a schema using regular C++ — without macros, reflection, or code generation.

This document describes the complete public API.

---

## 1. Schema

A schema defines how C++ types map to JSON.

```cpp
struct MySchema
    : aison::Schema<MySchema, aison::EncodeDecode /* optional facet */, MyConfig /* optional */>
{
    template<typename T> struct Object;   // struct mappings
    template<typename E> struct Enum;     // enum mappings
    template<typename T> struct Encoder;  // custom encode (optional)
    template<typename T> struct Decoder;  // custom decode (optional)
};
```

### 1.1 Facets

The facet defines which operations the schema supports:

| Facet | Meaning |
|-------|---------|
| `EncodeOnly`     | Only encoding is allowed; decoding triggers compile‑errors. |
| `DecodeOnly`     | Only decoding is allowed; encoding triggers compile‑errors. |
| `EncodeDecode`   | Both encoding and decoding (default). |

### 1.2 Config

A schema may optionally define a config type:

```cpp
struct MyConfig {
    bool upperCaseHex = false;
};
```

The config object is passed to:

- custom encoders (`Schema::Encoder<T>`)
- custom decoders (`Schema::Decoder<T>`)

Usage:

```cpp
MyConfig cfg{ .upperCaseHex = true };
aison::Result r = aison::decode<MySchema>(json, obj, cfg);
```

`this->config()` is available from custom encode/decode hooks.

### 1.3 Schema Assertions (`EnableAssert`)

By default:

```cpp
using EnableAssert = std::true_type;
```

which means:

- schema mistakes trigger **assert()** in debug mode,
- and are **skipped silently** otherwise.

Users may disable assertions:

```cpp
struct MySchema : aison::Schema<MySchema> {
    using EnableAssert = std::false_type;
};
```

---

## 2. Object Mapping (`Schema::Object<T>`)

Maps struct/class fields into JSON fields.

Example:

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

- Every field must be added exactly once.
- All fields are **required** during decoding.
- Duplicate JSON names or duplicate member pointers cause:
  - `assert()` (if `EnableAssert == true`)
  - silent skip if `EnableAssert == false`.

Nested structures, vectors, and optionals are supported.

---

## 3. Enum Mapping (`Schema::Enum<T>`)

Example:

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

- `add(value, name)` must list **every** enum value exactly once.
- Duplicate value or duplicate name → `assert()` / skip.
- Additional names can be added via `addAlias(value, alias)`:
  - The value **must** already have been added with `add()`.
  - Alias name must be unique.
  - Violations → `assert()` / skip.

Example:

```cpp
addAlias(Color::Red, "brightred");
```

Encoding uses only the *first* name given via `add`.  
Decoding accepts all names from `add()` and `addAlias()`.

---

## 4. Custom Encoders / Decoders

Used for non‑standard mappings (e.g., hex‑encoded colors).

```cpp
struct RgbColor { uint8_t r, g, b; };
```

### Custom Encoder Example

```cpp
template<>
struct MySchema::Encoder<RgbColor>
    : aison::Encoder<MySchema, RgbColor>
{
    void operator()(const RgbColor& c, Json::Value& dst) {
        std::ostringstream ss;
        if (config().upperCaseHex)
            ss << std::uppercase;

        ss << "#" << std::hex << std::setfill('0')
           << std::setw(2) << int(c.r)
           << std::setw(2) << int(c.g)
           << std::setw(2) << int(c.b);

        dst = ss.str();
    }
};
```

### Custom Decoder Example

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

---

## 5. Encoding / Decoding

### 5.1 Without Config (EmptyConfig only)

```cpp
aison::Result r1 = aison::encode<MySchema>(obj, json);
aison::Result r2 = aison::decode<MySchema>(json, obj);
```

### 5.2 With Config

```cpp
MyConfig cfg;
aison::Result r1 = aison::encode<MySchema>(obj, json, cfg);
aison::Result r2 = aison::decode<MySchema>(json, obj, cfg);
```

`Result` collects *all* errors:

---

## 6. Error Model

Each error contains:

- `path` — JSON-like path (e.g. `$.shapes[2].color`)
- `message` — explanation

```cpp
if (!result) {
    for (auto& error : result.errors) {
        std::cerr << error.path << ": " << error.message << "\n";
    }
}

```

Errors accumulate; decoding never stops early.

---

## 7. Type Support Matrix

| Type | Encode/Decode | Notes |
|------|---------------|------|
| `bool` | ✔ | |
| Integral types | ✔ | Range‑checked |
| `float`, `double` | ✔ | NaN rejected |
| `std::string` | ✔ | |
| `std::optional<T>` | ✔ | null → empty optional |
| `std::vector<T>` | ✔ | recursive support |
| enums | ✔ | requires `Enum<T>` |
| structs | ✔ | requires `Object<T>` |
| custom types | ✔ | via `Encoder<T>` / `Decoder<T>` |
| raw pointers | ⚠ allowed only via custom mapping | not automatic |

---

## 8. Schema Validation

Compile‑time errors include:

- Missing Object mapping  
- Missing Enum mapping  
- Facet mismatch  
- Missing custom encoder/decoder

Runtime schema errors include:

- Duplicate object fields  
- Duplicate enum values  
- Duplicate enum names  
- Alias for undefined enum value  

These trigger:

- `assert()` (when `EnableAssert == true`)
- skip mapping (otherwise)

