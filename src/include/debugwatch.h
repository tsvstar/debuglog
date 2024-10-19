#pragma once

/**
  Purpose:  Watching access to POD and STL members
  Author: Taranenko Sergey (tsvstar@gmail.com)
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

/**
   Watching access to POD and STL class members
*/

#include "properties_ext.h"
#include "tostr.h"

#define _def_prop_watch_aux( OwnerClass, MemberType, PropertyName, RealName, BacktraceDepth, ShowValues, OffsetFunc, Static, ... )  \
  __VAR_ARGS__ MemberType RealName;                                                                                     \
  Static MemberType const& prop_get_ ## PropertyName (OwnerClass const* self, const char* comment )                     \
      { return ::tsv::debuglog::Watch_Getter( self, #PropertyName, self.RealName, BacktraceDepth, ShowValues, comment ); } \
  Static void prop_set_ ## PropertyName (OwnerClass* self, MemberType const& value, const char* comment )               \
      { ::tsv::debuglog::Watch_Setter( self, #PropertyName, self.RealName, value, BacktraceDepth, ShowValues, comment );   \
        self.RealName = value; }                                                                                        \
  __VAR_ARGS__ ::properties_extension::prop<OwnerClass, MemberType, prop_get_ ## PropertyName, prop_set_ ## PropertyName, OffsetFunc> PropertyName


/**
 Declare hidden member "RealName" and public "PropertyName". Where Public have watched accessors
      OwnerClass - class which own that members
      MemberType - type of member
      ProperyName - name of public member (which actually call setter/getter which are watchers)
      RealName    - name of underlying member (which actually store value and is tracked)
      BacktraceDepth - if <0 then each access logged with full callstack of its context;
                   if >0 - then limited to this num of frames callstack
                   if =0 - then no callstack logged
      ShowValues  - if <0 then logged fact of access
                   otherwise that is print mode (which is one of ::tsv::util::tostr::ENUM_TOSTR_* values)
*/
#define def_prop_watch_member( OwnerClass, MemberType, PropertyName, RealName, BacktraceDepth, ShowValues )             \
  MemberType RealName;                                                                                                  \
  static size_t prop_offset_ ## PropertyName () { return offsetof (OwnerClass, PropertyName); }                         \
  _def_prop_watch_aux( OwnerClass, MemberType, PropertyName, RealName, BacktraceDepth, ShowValues, prop_offset_ ## PropertyName, static )

// The same as above but for static class members.
// @note - if static member with initialize, then add extra arg "inline" at the end
#define def_prop_watch_static_member( OwnerClass, MemberType, PropertyName, RealName, BacktraceDepth, ShowValues, ... ) \
  static __VAR_ARGS__ MemberType RealName;                                                                              \
  _def_prop_watch_aux( OwnerClass, MemberType, PropertyName, RealName, BacktraceDepth, ShowValues, nullptr, static, static __VAR_ARGS__ )

// The same as above but for global variables
#define def_prop_watch_global( MemberType, PropertyName, RealName, BacktraceDepth, ShowValues ) \
  MemberType RealName;                                                                          \
  _def_prop_watch_aux( ::tsv::debuglog::watch::impl::Global, MemberType, PropertyName, RealName, BacktraceDepth, ShowValues, nullptr, )

// "extern" for replaced to property global variable
#define def_prop_watch_extern( MemberType, PropertyName, RealName, BacktraceDepth, ShowValues )            \
  extern MemberType const &prop_get_ ## PropertyName (OwnerClass const* self, const char* comment );       \
  extern void prop_set_ ## PropertyName (OwnerClass* self, MemberType const& value, const char* comment )  \
  extern ::properties_extension::prop<::tsv::debuglog::watch::impl::Global, MemberType, prop_get_ ## PropertyName, prop_set_ ## PropertyName, nullptr> PropertyName

// For local variables changes outside of function scope is only possible by reference or pointer.
// Reference case means replace arg to property in whole callstack and so changes become not lightweight
// Pointer case doesn't trackeable at all



namespace tsv::debuglog
{

namespace watch::impl
{

class Global
{
};

void print(int depth, std::string_view comment, std::string_view suffix, std::string_view operation, std::string_view typeName, void* self, std::string_view memberName, std::string_view value);
}

/**
 Logging write access
  val            - current value of refered member
  membername     - its name
  backTraceDepth - if !=0, then calltrace to accessor
  showValues     - if <0 (default), then no value will be displayed
                   otherwise it have be one of ::tsv::util::tostr::ENUM_TOSTR_* values
  comment        - context of getter ( nullptr - do not track )
*/
template<typename OwnerClass, typename T>
const T& Watch_Getter(const OwnerClass* self, const char* memberName, const T& val, int backTraceDepth = 0, int showValues = -1, const char* comment = "")
{
    // comment == nullptr,  means do not track
    if ( !comment )
        return val;

    std::string value;   
    if (showValues >= 0)
        value = ::tsv::util::tostr::toStr(val,showValues);
    watch::impl::print(backTraceDepth, comment, "GET", typeid(OwnerClass).name(), self, memberName, value);
    return val;
}

/**
 Logging write access (and change value)
  existed_val    - reference to real object member with not changed yet value
  new_val        - new value
  membername     - its name
  backTraceDepth - if !=0, then print calltrace to accessor
  showValues     - if <0 (default), then no value will be displayed
                   otherwise it have be one of ::tsv::util::tostr::ENUM_TOSTR_* values
  comment        - context of setter ( nullptr - do not track )
*/
template<typename OwnerClass, typename T>
T& Watch_Setter(const OwnerClass* self, const char* memberName, T& existed_val, const T& new_val, int backTraceDepth = 0, int showValues = -1, const char* comment = "")
{
    // comment == nullptr,  means do not track
    if ( !comment )
        return existed_val;

    std::string value;   
    if (showValues >= 0)
    {
        value = ::tsv::util::tostr::toStr(existed_val, showValues);
        if (&existed_val!=&new_val)
            value.append(" ==> ").append(::tsv::util::tostr::toStr(new_val, showValues));
    }
    watch::impl::print(backTraceDepth, comment, "SET", typeid(OwnerClass).name(), self, memberName, value);
    return existed_val;
}

}   // namespace tsv::debuglog
