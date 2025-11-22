#pragma once

#include <aison/aison.h>
#include <json/json.h>

void printJson(const Json::Value& value);
void printErrors(const aison::Result& result);
