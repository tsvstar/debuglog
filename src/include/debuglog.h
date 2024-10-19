#pragma once


/**
  Purpose: Main header. 
           Includes scope/function enter/exit, regular logging, stacktrace and TOSTR
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt

  IMPORTANT!! Do not include this file directly or indirectly into PreCompiledHeaders scope,
              otherwise DEBUG_LOGGING / DEBUGLOG_CATEG switching will not work
*/


// ===== PROLOGUE: Determine DEBUG_LOGGING ====

// DEBUG_LOGGING - determine if SentryLogger macros generate output (SENTRY_*, SAY_*, SAY_DBG)
// a) Could be replaced by exact value (0 or 1) for local file (define before #include "debuglog.h")
// b) Could be replaced by category (DEBUGLOG_CATEG_..) for local file (define before #include "debuglog.h")
// c) If doesn't use DEBUGLOG_CATEG_DEFAULT value

#include "debuglog_categories.h"

// ===== MAIN HEADER ====

#include "debuglog_main.h"

// ===== EPILOGUE: Extra debuglog features ====


#if DEBUG_LOGGING
// Include extra headers only if logger turned on
#include "debugresolve.h"
#include "tostr.h"

#else

// Otherwise provide fallback to "do nothing" for TOSTR_* macro
#ifndef TOSTR_ARGS
#define TOSTR_ARGS(...) std::string()
#define TOSTR_JOIN(...) std::string()
#define TOSTR_EXPR(...) std::string()
#define TOSTR_FMT(...)  std::string()
#endif

#endif
