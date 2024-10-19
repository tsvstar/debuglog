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

// Reset possible previous #define ObjLogger to make correct declaration below
#undef ObjLogger

namespace tsv::debuglog
{

/**
  Use this class to log construct/desctruct objects

  HOWTO USE:
   1. Add private member to class
           ObjLogger debug_sentry_;
   2. Add to initalization list of ctor
             ,debug_sentry_( this, "ClassName", backtracedepth, "CommentIfNeeded")
   3. Add to initalization list of copy ctor
             ,debug_sentry_( obj.debug_sentry_ )
   To disable logging of some object class - just replace "this" to nullptr
*/

class ObjLogger
{
    public:
        ObjLogger( void* owner, const char* className, int depth=0, const char* comment="", SentryLogger::Level level = SentryLogger::Level::Default, SentryLogger::Kind kind = SentryLogger::Kind::Tracked);
        ObjLogger( void* owner, void* copied_from, const char* className, int depth, const char* comment, SentryLogger::Level level = SentryLogger::Level::Default, SentryLogger::Kind kind = SentryLogger::Kind::Tracked);
        ObjLogger( const ObjLogger& obj );
        ObjLogger( const ObjLogger&& obj );
        ObjLogger& operator=( const ObjLogger& obj );
        //ObjLogger& ObjLogger::operator=( const ObjLogger&& obj );    // not implemented yet

        ~ObjLogger();

        // say that all these objects are equal ( to not influe onto trivial classes )
        bool operator==( const ObjLogger& ) { return true; }
        bool operator<( const ObjLogger& )  { return false; }

        static void printTrackedPtr( SentryLogger::Level level, const char* className = nullptr, SentryLogger::Kind kind = SentryLogger::Kind::Tracked);

    public:
        // Settings
        static bool includeContextName_s;   // if true, then logging will include current context name

    public:
        int depth_;                 // how many stacktrace records print on

    private:
        const int offs_;            // offset of ObjLogger from begin of owner
        std::string className_;
        SentryLogger::Kind kind_;
        SentryLogger::Level level_;
};

// Stub class which replace real one if OBJ logging is disabled
class ObjLogEmptyClass
{
    public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
        ObjLogEmptyClass ( void* ptr, const char* className, int depth=0, const char* comment="", SentryLogger::Kind kind = SentryLogger::Kind::Default, SentryLogger::Level level = SentryLogger::Level::Default ) {}
        ObjLogEmptyClass ( void* ptr, void* copied_from, const char* className, int depth, const char* comment, SentryLogger::Kind kind = SentryLogger::Kind::Default, SentryLogger::Level level = SentryLogger::Level::Default ) {}
        static void printTrackedPtr( const char* className = nullptr ) {}
#pragma GCC diagnostic pop
    private:
};

} // namespace tsv::debuglog
#endif

/**
    YES, that is the end of header guard.
    Section below make possible turn on/off obj logging
    inside of header
*/

// Is ObjLogger object real
#ifndef DEBUG_OBJLOG
#define DEBUG_OBJLOG 1
#endif

#undef ObjLogger
#if DEBUG_OBJLOG
#else
// ObjLogging is forbidden
#define ObjLogger ObjLogEmptyClass
#endif
