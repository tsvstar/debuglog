#pragma once

/**
     Wrapper to choose available in the project format library
 */

#undef TOSTR_FMT

#if __has_include(<fmt/format.h>)
#include <fmt/format.h>
#define TOSTR_FMT(...) fmt::format( __VA_ARGS__ )

#elif __has_include(<format>)
#include <format>
#define TOSTR_FMT(...) std::format( __VA_ARGS__ )

#else
static_assert(false, "No format library detected");

#endif

