#ifndef DEBUGOBJLOG_H
/** That is intendend usage of header guard instead of pragma once
    to make possible to manipulate with obj logging in the header
*/
#define DEBUGOBJLOG_H 1

/**
  Purpose: Tracking objects lifecycle
  Author: Taranenko Sergey (tsvstar@gmail.com)
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt

 NOTE: If we plan to manipulate with DEBUG_OBJLOG, do not include this into PCH
*/

#include <string>
#include <vector>
#include "debuglog_enum.h"

namespace tsv::debuglog
{

/**
  Use this class to log construct/desctruct objects

  HOWTO USE:
   1. Add private member to class
           ObjLogger debug_sentry_;
   2. Add to initalization list of ctor
             ,debug_sentry_( this, "ClassName"[, backtracedepth, "CommentIfNeeded", level, kind])
   3. Add to initalization list of copy ctor
             ,debug_sentry_( obj.debug_sentry_ )
   To disable logging of some object class - just replace "this" to nullptr
*/

class ObjLoggerImpl
{
public:
    ObjLoggerImpl(const void* owner,
                  const char* className,
                  int depth = 0,
                  const char* comment = "",
                  sentry_enum::Level level = sentry_enum::Level::Default,
                  sentry_enum::Kind kind = sentry_enum::Kind::Tracked);

    ObjLoggerImpl(const void* owner,
                  const void* copied_from,
                  const char* className,
                  int depth,
                  const char* comment,
                  sentry_enum::Level level,
                  sentry_enum::Kind kind);

    ObjLoggerImpl(const ObjLoggerImpl& obj);
    ObjLoggerImpl(const ObjLoggerImpl&& obj);
    ObjLoggerImpl& operator=(const ObjLoggerImpl& obj);
    // ObjLogger& ObjLogger::operator=( const ObjLoggerImpl&& obj );    // not implemented yet

    ~ObjLoggerImpl();

    // say that all these objects are equal ( to not influe onto trivial classes )
    bool operator==(const ObjLoggerImpl&) { return true; }
    bool operator<(const ObjLoggerImpl&) { return false; }

    static void printTrackedPtr(const char* className = nullptr,
                                sentry_enum::Level level = sentry_enum::Level::Default,
                                sentry_enum::Kind kind = sentry_enum::Kind::Tracked);

    auto getKind() const { return kind_; }
    auto getLevel() const { return level_; }

public:
    // Settings
    static bool includeContextName_s;  // if true, then logging will include current context name

public:
    int depth_;  // how many stacktrace records print on

private:
    sentry_enum::Kind kind_;
    sentry_enum::Level level_;

    // Offset of ObjLogger from begin of owner.
    // <0 means "do not track"
    const int offs_;

    std::string className_;
};

// Stub class which replace real one if OBJ logging is disabled
class ObjLogEmptyClass
{
    public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
        ObjLogEmptyClass(const void* ptr,
                         const char* className,
                         int depth = 0,
                         const char* comment = "",
                         sentry_enum::Level level = sentry_enum::Level::Default,
                         sentry_enum::Kind kind = sentry_enum::Kind::Tracked)
        {}
        ObjLogEmptyClass(const void* ptr,
                         const void* copied_from,
                         const char* className,
                         int depth,
                         const char* comment,
                         sentry_enum::Level level,
                         sentry_enum::Kind kind)
        {}
        static void printTrackedPtr(const char* className = nullptr) {}
        auto getKind() const { return sentry_enum::Kind::Off; }
        auto getLevel() const { return sentry_enum::Level::Off; }
#pragma GCC diagnostic pop
};

} // namespace tsv::debuglog

/**
    YES, that is the end of header guard.
    Section below make possible turn on/off obj logging
    inside of header
*/
#endif

#ifndef DEBUG_OBJLOG
#define DEBUG_OBJLOG 1
#endif

namespace tsv::debuglog
{
#if DEBUG_OBJLOG
using ObjLogger = ObjLoggerImpl;
#else
using ObjLogger = ObjLogEmptyClass;
static_assert(false,"OFF");
#endif
}