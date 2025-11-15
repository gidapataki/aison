#include <aison/aison.h>
#include <json/json.h>

#include <iomanip>
#include <iostream>
#include <sstream>

// -------------------- Custom type: RGB --------------------
struct RGB {
  unsigned char r = 0;
  unsigned char g = 0;
  unsigned char b = 0;
};

// -------------------- Your data types --------------------
struct Stats {
  int a;

  struct Nested {
    int x;
    std::string y;
  } nested;

  std::vector<int> ls;
  std::optional<int> maybe;
  RGB color;
};

// -------------------- Schema --------------------
struct SchemaA {
  // Type -> field mapping
  template <typename T>
  struct Fields;

  // Custom primitive encodings live here (RGB in hex)
  static void encodeValue(const RGB& src, Json::Value& dst,
                          aison::Encoder<SchemaA>&) {
    std::ostringstream oss;
    oss << '#' << std::hex << std::setfill('0') << std::setw(2)
        << static_cast<int>(src.r) << std::setw(2) << static_cast<int>(src.g)
        << std::setw(2) << static_cast<int>(src.b);
    dst = oss.str();
  }

#if 0
  static void decodeValue(const Json::Value& src, RGB& dst,
                          aison::Decoder<SchemaA>& dec) {
    if (!src.isString()) {
      dec.addError("Expected hex RGB string");
      return;
    }

    std::string s = src.asString();
    if (s.size() != 7 || s[0] != '#') {
      dec.addError("Invalid RGB hex format");
      return;
    }

    auto hexByte = [&](int start, unsigned char& out) {
      std::string sub = s.substr(start, 2);
      char* end = nullptr;
      long v = std::strtol(sub.c_str(), &end, 16);
      if (!end || *end != '\0' || v < 0 || v > 255) {
        dec.addError("Invalid RGB component");
        return false;
      }
      out = static_cast<unsigned char>(v);
      return true;
    };

    if (!hexByte(1, dst.r)) return;
    if (!hexByte(3, dst.g)) return;
    if (!hexByte(5, dst.b)) return;
  }
#endif
};

// -------------------- SchemaA::Fields<T> specializations --------------------
template <>
struct SchemaA::Fields<Stats>
    : aison::Fields<SchemaA, Stats, aison::encodeOnly> {
  Fields() {
    add(&Stats::a, "a");
    add(&Stats::nested, "nested");
    add(&Stats::ls, "ls");
    add(&Stats::maybe, "maybe");
    add(&Stats::color, "color");
  }
};

template <>
struct SchemaA::Fields<Stats::Nested>
    : aison::Fields<SchemaA, Stats::Nested, aison::encodeDecode> {
  Fields() {
    add(&Stats::Nested::x, "x");
    add(&Stats::Nested::y, "y");
  }
};

// -------------------- Demo --------------------
int main() {
  Stats s;
  s.a = 42;
  s.nested.x = 7;
  s.nested.y = "hello";
  s.ls = {1, 2, 3};
  s.maybe = 99;
  s.color = RGB{0x12, 0x34, 0xAB};

  // ---------- Encode ----------
  aison::Encoder<SchemaA> encoder;
  Json::Value root;
  auto encRes = encoder.encode(s, root);

  Json::StreamWriterBuilder builder;
  builder["indentation"] = "  ";
  std::cout << "Encoded JSON (SchemaA):\n"
            << Json::writeString(builder, root) << "\n\n";

  if (!encRes) {
    std::cout << "Encode errors:\n";
    for (const auto& e : encRes.getErrors()) {
      std::cout << "  " << e.path << ": " << e.message << "\n";
    }
  }

  // Break some things: wrong type, missing field, bad color
  root.removeMember("ls");  // missing required field
  // root["a"] = "not-int";      // wrong type
  // root["color"] = "#12345Z";  // invalid hex

  // ---------- Decode ----------
  // aison::Decoder<SchemaA> decoder;
  // Stats out{};
  // auto decRes = decoder.decode(root, out);

  // if (!decRes) {
  //   std::cout << "Decode errors:\n";
  //   for (const auto& e : decRes.getErrors()) {
  //     std::cout << "  " << e.path << ": " << e.message << "\n";
  //   }
  // } else {
  //   std::cout << "Decoded successfully.\n";
  // }

  return 0;
}
