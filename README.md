# aison
*(pronounced as aye-son)*


A tiny C++ JSON serialization library inspired by circe.  
Built on top of [JsonCpp](https://github.com/open-source-parsers/jsoncpp).

## Features

- Requires C++17
- Header-only API (`#include <aison/aison.h>`)
- Declarative field mappings for structs
- Custom visitors for user-defined types (e.g. RGB → hex color)
- Multiple `Fields` definitions for the same struct (aliases / different JSON views)

## Getting started

```bash
git clone --recurse-submodules git@github.com:you/aison.git
cd aison
./pls gen
./pls run
```
