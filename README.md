# Aison

Aison is a small C++17 library for mapping objects to JSON and back.<br>
Mappings are defined explicitly in a C++ schema, without macros, code generation, or runtime reflection.<br>
The library consists of a single header, and depends on JsonCpp.

### Features
- clear struct → JSON mappings
- strict decoding with useful error messages
- support for:
  - primitive types (bool, int, int64_t, unsigned, uint64_t, float, double, std::string)
  - structs
  - enums
  - std::optional
  - std::vector
  - custom encoding and/or decoding (with an optional runtime config)
- schema features:
  - non-intrusive definition
  - allows multiple schemas for the same type
  - schemas are fully independent

### Requirements
- C++17 support
- [JsonCpp](https://github.com/open-source-parsers/jsoncpp)

