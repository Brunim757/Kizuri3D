#pragma once
#include <cstddef>
namespace Kizuri {
struct EmbeddedSkyEntry {
  const char* name;
  const unsigned char* data;
  size_t size;
};
size_t EmbeddedSkyCount();
const EmbeddedSkyEntry& EmbeddedSkyAt(size_t i);
}
