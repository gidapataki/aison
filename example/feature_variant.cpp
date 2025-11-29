#include <aison/aison.h>

#include <iostream>

#include "types.h"

namespace example {

namespace {

struct ShapeSchema : aison::Schema<ShapeSchema> {
    template<typename T>
    struct Object;
    template<typename T>
    struct Variant;
};

template<>
struct ShapeSchema::Variant<Shape> : aison::Variant<ShapeSchema, Shape> {
    Variant()
    {
        name("Shape");
        discriminator("kind");
    }
};

template<>
struct ShapeSchema::Object<Circle> : aison::Object<ShapeSchema, Circle> {
    Object()
    {
        name("circle");
        add(&Circle::radius, "radius");
    }
};

template<>
struct ShapeSchema::Object<Rectangle> : aison::Object<ShapeSchema, Rectangle> {
    Object()
    {
        name("rect");
        add(&Rectangle::width, "width");
        add(&Rectangle::height, "height");
    }
};

}  // namespace

void variantExample1()
{
    std::vector<Shape> shapes;
    shapes.push_back(Circle{.radius = 15});
    shapes.push_back(Rectangle{.width = 10, .height = 20});

    Json::Value root;
    auto res = aison::encode<ShapeSchema>(shapes, root);

    if (!res) {
        std::cout << "== Encode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
        return;
    }

    std::cout << "== Encoded ==\n";
    std::cout << root.toStyledString() << "\n\n";

    shapes = {};
    res = aison::decode<ShapeSchema>(root, shapes);

    if (!res) {
        std::cout << "== Decode error ==\n";
        for (auto& err : res.errors) {
            std::cout << err.path << ": " << err.message << "\n";
        }
        return;
    }

    std::cout << "== Decoded ==\n";
    std::cout << shapes.size() << "\n";
}
}  // namespace example
