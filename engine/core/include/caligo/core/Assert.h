#pragma once

namespace caligo {

[[noreturn]] void FailAssert(const char* expression, const char* file, int line,
                             const char* message);

}

#if defined(CALIGO_DEBUG)
#define CALIGO_ASSERT(condition)                                                                   \
  ((condition) ? static_cast<void>(0)                                                              \
               : ::caligo::FailAssert(#condition, __FILE__, __LINE__, nullptr))
#define CALIGO_ASSERT_MSG(condition, message)                                                      \
  ((condition) ? static_cast<void>(0)                                                              \
               : ::caligo::FailAssert(#condition, __FILE__, __LINE__, (message)))
#else
#define CALIGO_ASSERT(condition) static_cast<void>(0)
#define CALIGO_ASSERT_MSG(condition, message) static_cast<void>(0)
#endif
