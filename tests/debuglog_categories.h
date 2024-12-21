#pragma once

// for EnumType_t
#include "debuglog_enum.h"

// Intentional compilation guard to make possible to easy override completely the file
// (for example for your own tests)
#ifndef _DEBUGLOG_CATEGORIES_H_

/**
 * Compile-time categories
 */

// If we would like to use SENTRY_FUNC_COND/SENTRY_CONTEXT_COND, involved values must be 0 or 1.
// Otherwise we are free to use any non-zero value as "turn on" category.
// Possible usage is, for example, a detail level (for example argument of SAY_STACKTRACE to determine its depth level).

// By default logging is on
#define DEBUGLOG_CATEG_DEFAULT 1

// Examples of user on/off categories
#define DEBUGLOG_CATEG_TEST_OFF 0
#define DEBUGLOG_CATEG_TEST_ON 1

// Example of category which gives depth of backtrace of some cases
#define DEBUGLOG_CATEG_TEST_BT 10

// Track objects (in real life for each kind of tracked obj it is usefull to have separate category)
#define DEBUGLOG_CATEG_OBJ_TRACK 1

/**
  *  User-defined override of Kind.
  *  Keep its exactly structure. Just add own kinds in the marked area
  */

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

    // The beginning of user-specific kinds
    TestOn,
    TestOff,
    TestBT,
    TestTracked1,
    // The end of user-specific kinds

    // The last record (used in the getNumberOfKinds() )
    NumberOfKinds
};

/**
 * Following function must be defined somewhere in the source file
 * 
 * unsigned getNumberOfKinds() { return static_cast<unsigned>( tsv::debuglog::sentry_enum::Kind::NumberOfKinds); }
 */

}

#endif