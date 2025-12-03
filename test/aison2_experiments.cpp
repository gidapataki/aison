#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest.h>

#include "aison2/aison2.h"

TEST_CASE("aison2: scaffold compiles") {
  aison2::SchemaDescriptor schema{};
  CHECK(&schema != nullptr);
}
