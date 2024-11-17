#pragma once

/**
  Purpose: Resolving symbols and calltrace
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

#include <vector>
#include <string>

namespace tsv::debuglog
{

    // Transform type and function names to pretty form
    std::string demangle(const char* name);

    // Resolve pointer "addr" to function name
    //  if addLineNum = true, then include "at file:lineno"
    //  if includeHexAddr = true, then include hex value of pointer
    std::string resolveAddr2Name(const void* addr, bool addLineNum = false, bool includeHexAddr = false);

    // Get backtrace
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
    [[clang::noinline]] [[gnu::noinline]]
    std::vector<std::string> getStackTrace(int depth = -1, int skip = 0);
#pragma GCC diagnostic pop

    namespace resolve::settings
    {

        typedef void (*BTDisabledOutputFunc_t)(int/*depth*/,int/*skip*/,std::vector<std::string>& /*out*/);

        // Backtrace tunings
        extern bool btEnable;         // if false - resolveAddr2Name() returns "??" 
        extern BTDisabledOutputFunc_t btDisabledOutputCallback; // Called by getStackTrace() in case if btEnable=false
        extern bool btIncludeLine;    // if true, include to stacktrace "at file:line"
        extern bool btIncludeAddr;    // if true, include to stacktrace addr
        extern bool btShortList;      // if true, remember already printed stacktraces and just say it's number on repeat
        extern bool btShortListOnly;  // if true, do not print full stack - only short line
        extern int  btNumHeadFuncs;  // how many first functions include into collapsed stacktrace
    }

}  // namespace tsv::debuglog
