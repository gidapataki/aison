# Aison Reference Manual

Aison is a small C++17 library for mapping well‑structured C++ types to/from JsonCpp.  
Mappings are defined in a schema using regular C++ — without macros, reflection, or code generation.

This document describes the complete public API.

---

## Contents
- [Schema](#1-schema)
  - [Facets](#11-facets)
  - [Config](#12-config)
  - [Schema Assertions (`EnableAssert`)](#13-schema-assertions-enableassert)
  - [Optional Strictness (`strictOptional`)](#14-optional-strictness-strictoptional)
- [Object Mapping (`Schema::Object<T>`)](#2-object-mapping-schemaobjectt)
- [Enum Mapping (`Schema::Enum<T>`)](#3-enum-mapping-schemaenumt)
- [Custom Encoders / Decoders](#4-custom-encoders--decoders)
- [Encoding / Decoding](#5-encoding--decoding)
  - [Without Config](#51-without-config-emptyconfig-only)
  - [With Config](#52-with-config)
- [Polymorphism (`std::variant`)](#6-polymorphism-stdvariant)
  - [Discriminator key](#61-discriminator-key)
  - [Mapping variants](#62-mapping-variants)
  - [Rules](#63-rules)
- [Error Model](#7-error-model)
- [Type Support Matrix](#8-type-support-matrix)
- [Schema Validation](#9-schema-validation)

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

### 1.3 Schema Assertions (`enableAssert`)

By default: `true`

- schema mistakes trigger **assert()** in debug mode,
- and are **skipped silently** otherwise.

Users may disable assertions:

```cpp
struct MySchema : aison::Schema<MySchema> {
    static constexpr auto enableAssert = false;
};
```

### 1.4 Optional Strictness (`strictOptional`)

By default: `true`

- `strictOptional == true` (default):  
  - Decoding requires every `std::optional<T>` field to be **present** in JSON.  
  - Disengaged optionals must appear as explicit `null`.  
  - Encoding writes `null` for disengaged optionals.

- `strictOptional == false`:  
  - Missing optional fields decode to `std::nullopt` without errors.  
  - Encoding **omits** disengaged optionals instead of writing `null`.

Example:

```cpp
struct MySchema : aison::Schema<MySchema> {
    static constexpr auto strictOptional = false;
    template<typename T> struct Object;
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
- `std::optional<T>` fields follow the schema’s `strictOptional` setting (Section 1.4).

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

## 6. Polymorphism (`std::variant`)

Aison supports discriminated variants.

### 6.1 Discriminator key

You must define a discriminator key before mapping a variant:

- **Schema-level default (optional):**

  ```cpp
  struct ShapeSchema : aison::Schema<ShapeSchema> {
      static constexpr auto discriminatorKey = "__type__";
      template<typename T> struct Object;
  };
  ```

- **Per-object override:** `discriminator(tag, key)` sets both the tag value and the key for that alternative.

If the schema declares `discriminatorKey`, you can omit the key and call `discriminator(tag)` to use the schema default.

Keys must be **non-empty** and **identical across all alternatives**.

### 6.2 Mapping variants

```cpp
using Shape = std::variant<Circle, Rectangle>;

template<>
struct ShapeSchema::Object<Circle> : aison::Object<ShapeSchema, Circle> {
    Object() {
        add(&Circle::radius, "radius");
        discriminator("circle");               // uses schema key
        // discriminator("circle", "__kind__"); // override key per type
    }
};

template<>
struct ShapeSchema::Object<Rectangle> : aison::Object<ShapeSchema, Rectangle> {
    Object() {
        add(&Rectangle::width, "width");
        add(&Rectangle::height, "height");
        discriminator("rect");
    }
};
```

### 6.3 Rules

- Every alternative must call `discriminator(...)`.
- All alternatives must share the same discriminator key (schema default or per-object override).
- Missing keys/tags or mismatched keys produce errors; with `EnableAssert == true`, debug builds also assert.

---

## 7. Error Model

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

## 8. Type Support Matrix

| Type | Encode/Decode | Notes |
|------|---------------|------|
| `bool` | ✔ | |
| Integral types | ✔ | Range‑checked |
| `float`, `double` | ✔ | NaN rejected |
| `std::string` | ✔ | |
| `std::optional<T>` | ✔ | `strictOptional` controls whether null is required (`true`) or omission is allowed (`false`) |
| `std::vector<T>` | ✔ | recursive support |
| `std::variant<Ts...>` | ✔ | requires discriminator per alternative |
| enums | ✔ | requires `Enum<T>` |
| structs | ✔ | requires `Object<T>` |
| custom types | ✔ | via `Encoder<T>` / `Decoder<T>` |
| raw pointers | ⚠ allowed only via custom mapping | not automatic |

---

## 9. Schema Validation

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
- Missing discriminator key or tag on a variant alternative  
- Mismatched discriminator keys across variant alternatives  

These trigger:

- `assert()` (when `EnableAssert == true`)
- skip mapping (otherwise)
