#include <aison/aison.h>
#include <json/json.h>

#include <iomanip>
#include <iostream>
#include <sstream>

// -------------------- Enum + types --------------------
enum class Kind { Unknown, Foo, Bar };

struct RGB {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
};

struct Stats {
    Kind kind{};

    struct Nested {
        int x{};
        std::string y;
    } nested;

    std::vector<int> ls;
    std::optional<int> maybe;
    RGB color;
};

// -------------------- Schema --------------------
struct SchemaA {
    template<typename T>
    struct Object;

    template<typename E>
    struct Enum;

    struct Config : aison::Config {
        int version = 0;
    };

    // Custom primitive encodings (RGB as hex)
    static void encodeValue(const RGB& src, Json::Value& dst, aison::Encoder<SchemaA>& enc)
    {
        std::ostringstream oss;
        oss << '#' << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(src.r)
            << std::setw(2) << static_cast<int>(src.g) << std::setw(2) << static_cast<int>(src.b);
        dst = oss.str();
    }

    static void decodeValue(const Json::Value& src, RGB& dst, aison::Decoder<SchemaA>& dec)
    {
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
};

template<>
struct SchemaA::Enum<Kind> : aison::Enum<SchemaA, Kind> {
    Enum()
    {
        add(Kind::Unknown, "unknown");
        add(Kind::Foo, "foo");
        add(Kind::Bar, "bar");
    }
};

template<>
struct SchemaA::Object<Stats> : aison::Object<SchemaA, Stats, aison::EncodeDecode> {
    Object()
    {
        add(&Stats::kind, "kind");  // enum class
        add(&Stats::nested, "nested");
        add(&Stats::ls, "ls");
        add(&Stats::maybe, "maybe");
        add(&Stats::color, "color");
    }
};

template<>
struct SchemaA::Object<Stats::Nested> : aison::Object<SchemaA, Stats::Nested, aison::EncodeDecode> {
    Object()
    {
        add(&Stats::Nested::x, "x");
        add(&Stats::Nested::y, "y");
    }
};

// main

int main()
{
    Stats s;
    s.kind = Kind::Foo;
    s.nested.x = 7;
    s.nested.y = "hello";
    s.ls = {1, 2, 3};
    s.maybe = 99;
    s.color = RGB{0x12, 0x34, 0xAB};

    Json::Value root;

    // Using Encoder directly
    SchemaA::Config cfg{.version = 55};
    aison::Encoder<SchemaA> enc(cfg);
    aison::Result er = enc.encode(s, root);

    std::cout << "== Encoded ==\n" << root.toStyledString() << "\n\n";

    if (!er) {
        for (const auto& e : er.errors) {
            std::cerr << e.path << ": " << e.message << "\n";
        }
    }

#if 1
    Stats out{};
    aison::Result dr = aison::decode<SchemaA>(root, out, cfg);

    if (!dr) {
        std::cerr << "== Decode errors ==\n";
        for (const auto& e : dr.errors) {
            std::cerr << e.path << ": " << e.message << "\n";
        }
    } else {
        std::cout << "== Decoded ==\n";
    }
#endif
}
