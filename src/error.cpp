#include <iostream>
#include "error.hpp"

namespace xrun {
std::stringstream error_message;
void panic(const int code) {
    std::cerr << error_message.str();
    exit(code);
}
void panic(const char* message, const int code) {
    std::cerr << error_message.str() << message;
    exit(code);
}
}
