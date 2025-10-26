#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define PUBLIC __declspec(dllexport)
#else
#define PUBLIC __attribute__((visibility("default")))
#endif
#define PRIVATE static

#define UTILS static inline

#define BEFORE_START __attribute__((constructor)) static
#define AFTER_END __attribute__((destructor)) static

#define C8NS(name) CHIBA_##name