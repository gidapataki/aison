# Changelog

## Unreleased
- Variants now register alternatives explicitly via `add<Alt>("tag")`, requiring unique/non-empty tags for every `std::variant` alternative; alternative tags are no longer inferred from object names. Missing mappings surface schema errors on encode/decode/introspect.
- Schema names relaxed: `name` is only required when introspection is enabled; discriminator remains a required `static constexpr` for variants. Runtime setters are removed in favor of static members.
- Repository guidelines updated to reflect the new variant tagging flow, relaxed naming rules, and sandbox/ccache build note.

---
## aison v0.11.0
- Optional handling controls: new `strictOptional` schema flag (default true) lets disengaged optionals be omitted on encode/decode

---
## aison v0.10.0
- Added discriminated union support via `std::variant`:
    - runtime `discriminator(tag[, key])` on schema objects
    - validator enforces a shared, non-empty key and distinct tags across all alternatives
    - clear errors for missing/unknown tags or reserved keys
- Test suite expansion: basic types, custom encoder/decoder cases (including config-aware), runtime assert/error checks, variant coverage
- CI improvements: GitHub Actions builds/tests Debug and Release plus sanitizer configs using Ninja.

---
## aison v0.9.0 (first release)
- Supported types: integers/floats, `std::string`, `std::optional`, `std::vector`, custom enums/structs via schema.
- Custom encoders/decoders with optional config objects.
- Error aggregation: decode/encode collects all validation errors (with JSON paths) instead of failing fast.
- Basic test coverage and encode/decode examples.
