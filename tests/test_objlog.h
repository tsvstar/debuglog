#pragma once

// minimal include required for check category
#include "debuglog_categories.h"

// If 0 then object tracking will be wipeout without any extra efforts
// So no memory and execution overhead for intensively used objects
#define DEBUG_OBJLOG DEBUGLOG_CATEG_OBJ_TRACK
#include "objlog.h"

/**
 * Optional way to manage object tracking compile-time.
 * Could be used if several kind of tracked objects are in the header
 * 
 * #if DEBUGLOG_CATEG_OBJ_TRACK || DEBUGLOG_CATEG_ANOTHER_TRACK
 * #include "objlog.h"
 * #endif
 *
 */

namespace tsv::debuglog::tests::objlog
{

// Sample trivial item
class TrackedItem
{
public:
    TrackedItem();
    TrackedItem(int x);
    // this is just to show how manage tracking in custom copy ctor.
    // if it is default, then no need to customize (see operator= sample)
    TrackedItem(const TrackedItem&);

    // We define copy ctor so operator= should be defined.
    // But it could be default.
    TrackedItem& operator=(const TrackedItem&)  = default;

    int x_;
    virtual void test();
    static constexpr auto* baseClassName_ = "TrackedItem";

private:
// Sample of explicit compile-time guarding of the tracking (for example for several kinds in the single header)
#if DEBUGLOG_CATEG_OBJ_TRACK
    // Parameters should be defined in the ctors if not given here
    tsv::debuglog::ObjLogger debugEntry_;
#endif
};

// Derived from tracked object
// Note that no need to add tracker object to track it too
class DerivedTrackedItem : public TrackedItem
{
public:
    int y_ = 10;
    void test() override;
};

// Example how to track objects which have default ctors/operators
struct TrackedSimple
{
    int z_ = 55;

// @note To make implicit aggregate ctor works - all members should be public
//private:
    tsv::debuglog::ObjLogger debugEntry_{
        this,
        "Simple",   // name of the class in tracker
        0,          // stacktrace depth on any operation (optional. default=0. no stacktrace)
        "",         // (optional) comment
        sentry_enum::Level::Warning, //(optional) Impacts on tracking
        sentry_enum::Kind::TestTracked1 //(optional). Impacts on tracking
    };
};

}