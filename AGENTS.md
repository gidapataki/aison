# Repository Guidelines

## Project Structure & Module Organization
- Core library is header-only in `include/aison/aison.h`; no separate build artifact is produced beyond consuming the headers.
- Examples live in `example/` and mirror realistic schema usage; useful for quick manual validation.
- Tests are in `test/` and rely on Doctest headers vendored under `third_party/doctest`.
- External dependency JsonCpp is vendored under `third_party/jsoncpp`; no network fetch is required.
- CMake outputs go to `build/` by default. Use a separate directory per build type to keep artifacts isolated.

## Build, Test, and Development Commands
- Recommended helper: `cmake -S . -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug` to configure with CMake (default generator: Ninja).
- Build everything: `cmake --build build`. Build a target (e.g., the example binary): `cmake --build build -t example`.
- Run tests with ctest: `ctest --test-dir build --output-on-failure`.
- Full build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --output-on-failure --test-dir build`.
- Sanitizers are opt-in: append for `-DAISON_ENABLE_ASAN=ON` (or `..._UBSAN`, `..._LSAN` on Linux) to the configure step.

## Coding Style & Naming Conventions
- Format with `.clang-format` (Google base, 4-space indent, left-aligned pointers, 100-col limit, brace wrapping on classes/functions). Apply via `clang-format -i path/to/file.cc`.
- `.clang-tidy` enables `bugprone-*`, `performance-*`, `clang-analyzer-*`, and naming rules: functions/methods/variables use `camelBack`, classes `CamelCase`, private/protected members suffixed with `_`.
- Favor non-intrusive schemas and explicit mappings; mirror existing examples for Variant/Object/Enum specializations.

## Testing Guidelines
- Tests are Doctest-based (`TEST_CASE` blocks in `test/*.cpp`). Keep scenario-focused cases short and isolated; prefer helper utilities in `test/utils.{h,cpp}`.
- Name tests after behavior (`TEST_CASE("decode: rejects missing field")` style) and cover both success and failure paths for encoders/decoders.
- Run the full suite before PRs; for tight loops or pointer-heavy code, run with ASan/UBSan flags during debug builds.

## Schema Metadata Requirements
- Schema specializations must expose compile-time metadata: every `Schema::Object<T>`, `Schema::Enum<T>`, and `Schema::Custom<T>` declares `static constexpr auto name = "..."`; every `Schema::Variant<V>` declares both `static constexpr auto name = "..."` and `static constexpr auto discriminator = "..."` (non-empty).
- Variant alternatives must be mapped as objects and each object mapping must declare its own static `name`; introspection relies on these names and variant encode/decode writes/reads discriminator strings from them.

## Commit & Pull Request Guidelines
- Commit messages mirror current history: concise, title case or imperative without trailing punctuation (e.g., `Improve error paths`).
- PRs should state motivation, summarize major changes, and link issues if relevant. Include build/test commands executed and any sanitizer runs.
- Add screenshots or sample JSON payloads when altering error reporting or introspection output. Note new flags or options in `README.md`/`REFERENCE.md` as needed.

## Misc
- When building in restricted sandboxes, disable ccache (`CCACHE_DISABLE=1 cmake --build ...`) to avoid temp-file failures.
