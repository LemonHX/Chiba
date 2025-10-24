#pragma once

// disable testing/logging/etc ANSI colors if NO_COLOR is defined
#ifndef NO_COLOR
#define ANSI_BOLD "\033[1m"
#define ANSI_RESET "\033[0m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_BLUE "\033[34m"
#define ANSI_CYAN "\033[36m"
#else
#define ANSI_BOLD ""
#define ANSI_RESET ""
#define ANSI_RED ""
#define ANSI_GREEN ""
#define ANSI_BLUE ""
#define ANSI_CYAN ""
#endif

#ifdef NDEBUG
#define RELEASE
#else
#define DEBUG
#endif

