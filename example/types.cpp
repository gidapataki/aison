#include "types.h"

#include <iomanip>
#include <sstream>

namespace example {

std::string toHexColor(const RGBColor& color, bool upperCaseHex)
{
    std::stringstream stream;
    if (upperCaseHex) {
        stream << std::uppercase;
    }
    stream << '#' << std::hex << std::setfill('0');
    stream << std::setw(2) << int(color.r);
    stream << std::setw(2) << int(color.g);
    stream << std::setw(2) << int(color.b);
    return stream.str();
}

std::optional<RGBColor> toRGBColor(const std::string& str)
{
    if (str.size() != 7 || str[0] != '#') {
        return std::nullopt;
    }

    for (int i = 1; i < 7; ++i) {
        if (!std::isxdigit(str[i])) {
            return std::nullopt;
        }
    }

    const char* start = str.c_str();
    char* end = nullptr;
    auto colorValue = std::strtoul(start + 1, &end, 16);

    RGBColor color;
    color.r = (colorValue >> 16) & 0xff;
    color.g = (colorValue >> 8) & 0xff;
    color.b = (colorValue >> 0) & 0xff;
    return {color};
}

}  // namespace example
