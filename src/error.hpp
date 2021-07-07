#pragma once
#include <sstream>

namespace xrun {
extern std::stringstream error_message;
void panic(int code = -1);
void panic(const char* message, int code = -1);
}
