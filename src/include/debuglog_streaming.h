#pragma once

/**
  Purpose: Main header with allowed ostream interface for debugging. 
           Includes scope/function enter/exit, regular logging, stacktrace and TOSTR
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt

  IMPORTANT!! Do not include this file directly or indirectly into PreCompiledHeaders scope,
              otherwise DEBUG_LOGGING / DEBUGLOG_CATEG switching will not work
*/



// SENTRY_*() AND SAY_*()
#define DEBUGLOG_ALLOW_STREAM_INTERFACE 1

#include "debuglog.h"
