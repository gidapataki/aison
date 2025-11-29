#pragma once
#include <optional>
#include <string>
#include <vector>

namespace example {

// Color

struct RGBColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

struct Channels {
    std::vector<uint8_t> r, g, b;
};

// Text

enum class Alignment {
    kLeft,
    kCenter,
    kRight,
};

struct Span {
    std::string str;
    RGBColor color;
    float fontSize = 24.f;
};

struct Paragraph {
    std::vector<Span> spans;
    Alignment alignment = Alignment::kLeft;
};

struct Text {
    std::vector<Paragraph> paragraphs;
    std::optional<RGBColor> bgColor;
};

// Shape

struct Circle {
    float radius = 0;
};

struct Rectangle {
    float width = 0;
    float height = 0;
};

using Shape = std::variant<Circle, Rectangle>;

// Ice cream

enum class Flavor { kVanilla, kChocolate };

struct Topping {
    std::string name;
    bool crunchy = false;
};

struct Cone {
    int scoops = 1;
    Flavor flavor = Flavor::kVanilla;
    std::vector<Topping> toppings;
};

struct Cup {
    bool sprinkles = false;
    std::optional<Topping> drizzle;
};

using Dessert = std::variant<Cone, Cup>;

struct Order {
    std::string customer;
    std::optional<Dessert> dessert;
    std::vector<Topping> extras;
};

// Helpers

std::string toHexColor(const RGBColor& color, bool upperCaseHex = false);
std::optional<RGBColor> toRGBColor(const std::string& str);

}  // namespace example
