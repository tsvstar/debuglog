#pragma once

// Intentional compilation guard to make possible to easy override completely the file (for example for tests)
#ifdef _DEBUGLOG_CATEGORIES_H_

// If we would like to use SENTRY_FUNC_COND/SENTRY_CONTEXT_COND, involved values must be 0 or 1.
// Otherwise we are free to use any non-zero value as "turn on" category.
// Possible usage is, for example, a detail level (for example argument of SAY_STACKTRACE to determine its depth level).

// By default logging is on
#define DEBUGLOG_CATEG_DEFAULT 1

#define DEBUGLOG_CATEG_TEST_OFF 0
#define DEBUGLOG_CATEG_TEST_ON 1

#endif