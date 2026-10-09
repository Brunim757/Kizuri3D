#include "Kizuri/Assets/EmbeddedSkies.h"
namespace Kizuri {
size_t EmbeddedSkyCount() {
  return 0;
}
const EmbeddedSkyEntry& EmbeddedSkyAt(size_t i) {
  (void)i;
  static EmbeddedSkyEntry e = { "", nullptr, 0 };
  return e;
}
}
