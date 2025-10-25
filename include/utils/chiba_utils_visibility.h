#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define PUBLIC __declspec(dllexport)
#else
#define PUBLIC __attribute__((visibility("default")))
#endif
#define PRIVATE static

#define UTILS static inline

#define C8(name) CHIBA_##name

#define BEFORE_START __attribute__((constructor)) static
