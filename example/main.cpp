#include <aison/aison.h>
#include <json/json.h>

namespace example {

void variantExample1();
void encoderExample1();
void encoderExample2();
void introspectExample1();

void run()
{
    variantExample1();
    encoderExample1();
    encoderExample2();
    introspectExample1();
}

}  // namespace example

int main()
{
    example::run();
    return 0;
}
