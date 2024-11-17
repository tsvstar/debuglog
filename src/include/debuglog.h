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


#include "debuglog_main.h"

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
