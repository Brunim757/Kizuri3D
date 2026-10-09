#pragma once
namespace Kizuri {
void AssertFail(const char* expr, const char* file, int line);
void AssertFailMessage(const char* expr, const char* message, const char* file, int line);
}
#if defined(_DEBUG) || defined(KIZURI_FORCE_ASSERT)
#define KZ_ASSERT(cond) ((void)((cond) ? 0 : (Kizuri::AssertFail(#cond, __FILE__, __LINE__), 0)))
#define KZ_ASSERT_MSG(cond, msg) ((void)((cond) ? 0 : (Kizuri::AssertFailMessage(#cond, msg, __FILE__, __LINE__), 0)))
#else
#define KZ_ASSERT(cond) ((void)0)
#define KZ_ASSERT_MSG(cond, msg) ((void)0)
#endif
