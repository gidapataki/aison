# Aison Design Notes

## Goals
- Header-only schema library with explicit, CRTP-based mappings for objects, enums, variants, and custom types.
- Predictable validation: schema problems are surfaced early and consistently, and runtime errors are reported via `Result`/`Context`.
- Keep implementation C++17-compliant, avoid compiler-specific extensions.

## Schema & Mapping Invariants
- Schemas must derive from `aison::Schema<Derived, Config>` and expose `SchemaTag` and `ConfigType`.
- Objects/Enums/Variants/Custom mappings must inherit their corresponding `aison::Object/Enum/Variant/Custom` bases.
- Variant mappings must target `std::variant` types; every alternative must have an Object mapping.
- Variants always require a discriminator and named alternatives (used for tagging); introspection-enabled (`enableIntrospection = true`) additionally requires names on objects, enums, variants, and custom types.
- `Object::add` field types must be supported: mapped Object/Enum/Custom/Variant types, `std::optional`, `std::vector`, and primitive scalar/string types.

## Validation & Error Surfacing
- API entry points (`aison::encode/decode/introspect`) validate schema CRTP/tagging up front.
- Schema validations (base checks, missing names/discriminator, unsupported field types) use the shared helpers in `aison.h` and report at most once per issue through the runtime `Context` (schema errors are marked `(Schema error)`).
- Runtime data issues continue to use `Context::addError` with path tracking.

## Custom Types
- Define `template<> struct Schema::Custom<T> : aison::Custom<Schema, T>`.
- Implement `void encode(const T&, Json::Value&, EncodeContext&)` for encode-capable schemas and `void decode(const Json::Value&, T&, DecodeContext&)` for decode-capable schemas.
- Use the provided context helpers: `config()`, `addError(...)`, and `encode/decode` for nested values.
- Name is required when introspection is enabled; set via `name("...")`.

## Variants
- Define `template<> struct Schema::Variant<V> : aison::Variant<Schema, V>`; set `name(...)` (when introspection is on) and `discriminator(...)`.
- Each alternative must map to an object with a name; discriminator is written/read alongside encoded fields.
- Discriminator and regular object fields should not clash, otherwise it is a schema error.

## Development Checklist
- Use forward declarations for all types & functions in `aison.h`.
- Preserve C++17 compatibility
- Preferred commands (from AGENTS.md): `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build`, `ctest --output-on-failure --test-dir build`. Use `CCACHE_DISABLE=1` if cache permissions cause issues.
