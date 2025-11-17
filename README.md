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
  - `std::string`
  - structs (via schema)
  - enums
  - `std::optional`
  - `std::vector`
  - custom encode/decode hooks (optionally using a runtime config)
- schema properties:
  - non-intrusive definitions (no changes to your structs)
  - multiple schemas for the same type
  - schemas are fully independent

### Requirements
- C++17
- [JsonCpp](https://github.com/open-source-parsers/jsoncpp)



---

#### Note on the project's origin

Aison began as an experiment to see how quickly a library can be developed when
AI-assisted tooling is used to speed up exploration and rewriting. It was
eye-opening how fast different approaches could be tried and tested.

After a rapid prototyping phase, the library went through a full manual refactor,
followed by several rounds of review and polishing. The final result reflects
deliberate engineering decisions.
