#pragma once

namespace tsv::debuglog::sentry_enum
{
// All enums of the class SentryLogger matches to this type
using EnumType_t = unsigned;

enum class Level : EnumType_t
{
    Fatal,
    Error,
    Warning,
    Important,
    Info,
    Debug,
    Trace,
    // Special cases. Important to keep this block Off-Default-Parent as is at the end
    Off,      // Turn off logger  
    Default,  // Special kind - replaced to Settings::getDefaultSentryLoggerLevel()
    This,     // Special kind - means level of the logger
    Parent    // Special kind - means level of upper level logger
};

// Forward declaration to ensure that type of the enum in "debuglog_categories.h" is correct
enum class Kind : EnumType_t;

}


#if __has_include("debuglog_categories.h")
// Define your own list of macros with categories to differential compile-time control.
// as well as your extended kinds list. Take a look example in tests
#include "debuglog_categories.h"

// @note The library itself doesn't know user-side values of enum Kind.
// Trick is that we make it compatible by defining with the same name and type,
// and return the last value in the weak-linked function.

#else

// if no user-defined header given, declare default enum with built-in values only.
// This branch is also used for compilation of library itself.
namespace tsv::debuglog::sentry_enum
{
enum class Kind : EnumType_t
{
    Parent = 0,     // Special kind - means kind of parent sentry
    Off = 9,        // Built-in
    Default,
    Tracked,        // built-in default category for objlog module
    KnownPtr,       // built-in category for (de)register known_pointers

    // User-specified kinds should be defined after that item 
    LastBuiltinReserved = 20,

    // in "debuglog_categories.h" more kinds could be defined in this section
    // ....

    // the last record
    NumberOfKinds
};
}
#endif
