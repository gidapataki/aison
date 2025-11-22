#include "utils.h"

#include <iostream>

void printJson(const Json::Value& value)
{
    std::cerr << value.toStyledString() << "\n\n";
}

void printErrors(const aison::Result& result)
{
    for (const auto& err : result.errors) {
        std::cerr << err.path << ": " << err.message << "\n\n";
    }

    if (result) {
        std::cerr << "no errors\n\n";
    }
}
