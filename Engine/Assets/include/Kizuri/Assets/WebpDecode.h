#pragma once
#include <stdint.h>
#include <vector>
namespace Kizuri {
bool DecodeWebp(const void* bytes, size_t size, std::vector<unsigned char>& rgba, int& w, int& h);
}
