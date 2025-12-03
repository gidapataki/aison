# AISON Dynamic Schema Design Summary

## 0. Goals / Constraints

- C++17.
- No RTTI, no virtuals.
- JSON ↔ C++ data mapping via explicit schemas.
- Dynamic-ish schema model:
  - You define independent “defs” for each C++ type.
  - Then you build a `Schema` from a pack of defs.
- Strong compile-time guarantees.
- Ergonomics: Defs as separate statements (semicolon style), no long fluent chains.

## 1. High-Level API

### Object Definitions

```cpp
struct Bar { int x; };
struct Foo { int y; Bar bar; };

auto barDef = aison::Object<Bar>([](auto& ctx) {
    return aison::Fields{
        ctx.add(&Bar::x, "x"),
    };
});

auto fooDef = aison::Object<Foo>([](auto& ctx) {
    return aison::Fields{
        ctx.add(&Foo::y,   "y"),
        ctx.add(&Foo::bar, "bar"),
    };
});
```

### Enum Definitions

```cpp
enum class Mode { Dark, Light, Automatic };

auto modeDef = aison::Enum<Mode>([](auto& ctx) {
    return aison::EnumValues{
        ctx.value("dark",  Mode::Dark),
        ctx.value("light", Mode::Light),
        ctx.value("auto",  Mode::Automatic),
    };
});
```

### Schema Construction

```cpp
auto schema = aison::Schema(
    fooDef,
    barDef,
    modeDef,
);
```

## 2. Compile-Time Layer

### Typelist

Simple pack type:

```cpp
template<class... Ts>
struct TypeList {};
```

### Field Descriptor

```cpp
template<class Owner, class Field>
struct FieldDef {
    using OwnerType = Owner;
    using FieldType = Field;
    const char* name;
    Field Owner::* ptr;
};
```

### FieldList with CTAD

```cpp
template<class... Fields>
struct FieldList {
    std::tuple<Fields...> fields;
};

template<class... Fields>
FieldList(Fields...)->FieldList<Fields...>;
```

### Object Definition

```cpp
template<class T, class FieldList>
struct ObjectDef {
    using Type = T;
    using FieldsType = FieldList;
    using Deps = TypeList<typename Fields::field_type...>;
    FieldList fields;
};
```

### Enum Definition

```cpp
template<class T, class ValuesList>
struct EnumDef {
    using Type = T;
    using Deps = TypeList<>;
    ValuesList values;
};
```


## 3. Object<T>(lambda) Implementation

```cpp
template<class T, class F>
auto Object(F&& f) {
    ObjectCtx ctx;
    auto fields = f(ctx);
    using FieldListType = std::decay_t<decltype(fields)>;
    return ObjectDef<T, FieldListType>{ std::move(fields) };
}
```

## 4. Schema(defs...) Implementation

### Responsibilities

- Collect defined types.
- Collect declared types.
- Collect all dependencies.
- `static_assert` if any dependency is not part of the declared types.

### Example Logic

```cpp
template<class... Defs>
struct SchemaDef {
    using Defs = TypeList<Defs...>;
    using DefinedTypes = /* all Def::type for ObjectDef/EnumDef/CustomDef */;
    using DeclaredTypes = /* defined_types ∪ all T from DeclareDef<T> */;
    using Deps = /* union of Def::deps for all Defs */;

    static_assert(all_deps_in_declared_types, "Missing definitions...");
};
```

## 5. Custom Types

```cpp
auto pathDtoDef = aison::Object<PathDTO>(...);

auto pathCustomDef = aison::Custom<Path2D>()
    .via<PathDTO>()
    .encoder([](Encoder& e, const Path2D& p, auto& ctx) {
        auto dto = toDTO(p);
        ctx.encode<PathDTO>(e, dto);
    })
    .decoder([](Decoder& d, auto& ctx) -> Path2D {
        auto dto = ctx.decode<PathDTO>(d);
        return fromDTO(dto);
    });
```

## 6. Guarantees

- Order-independent schema construction.
- Forward and cyclic references are technically allowed.
- Static errors when defs are missing for referenced types.
- Clear separation of compile-time vs runtime.

## 7. Summary for Codex

Implement:

1. `TypeList` + minimal utilities.
2. `FieldDef`, `FieldList` (with CTAD).
3. `ObjectCtx`.
4. `Object<T>(lambda)` → `ObjectDef<T, FieldList>`.
5. `Enum<T>(lambda)` → `EnumDef<T, ValuesList>`.
6. `Declare<T>()` → `DeclareDef<T>`.
7. `Schema(defs...)` → compile-time validation + store defs.
8. encode/decode/introspect
