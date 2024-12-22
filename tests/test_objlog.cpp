#include <iostream>
#include <string>
#include <cstdio>

// Include works regarless of including of "debuglog.h" earlier
#include "test_objlog.h"

#define DEBUG_LOGGING DEBUGLOG_CATEG_OBJ_TRACK
#include "debuglog.h"

// This is for SAY_VIRT_FUNC_CALL/WHICH_VIRTFUNC_WILL_BE_CALLED
// It is pretty complex and uncommonly used feature so it is separated to save compile time
#include "debugrvirtresolve.h"
// For Settings
#include "debuglog_settings.h"

#include "main.h"
#include <unordered_map>
#include <regex>

/************* Example toStr() extending **********************/

namespace tsv::util::tostr::impl
{
// Handler definition
// Declared in "debuglog_tostr_my_handler.h" because should be included in the middle of tostr_handler.h
// Place of definition has no matter. Different handlers could be defined in different places.
ToStringRV __toString(const tsv::debuglog::tests::objlog::TrackedItem& value, int mode)
{
    using namespace ::tsv::util;

    if (mode == ENUM_TOSTR_DEFAULT)
        return {tostr::toStr(value.x_), true};
    else if (mode == ENUM_TOSTR_REPR)
        return {"{x=" + tostr::toStr(value.x_) + "}", true};
    // Extended info
    return {"{ addr=" + tostr::hex_addr(&value) + ", x=" + tostr::toStr(value.x_) + "}",
            true};
}

ToStringRV __toString(const tsv::debuglog::tests::objlog::TrackedSimple& value, int mode)
{
    if (mode == ENUM_TOSTR_REPR)
        return {"{z=" + tostr::toStr(value.z_) + "}", true};
    return {tostr::toStr(value.z_), true};
}

} // namespace tsv::util::tostr::impl

namespace
{
std::unordered_map<void*, std::string> replaces;

void applyReplaces()
{
    auto& str = tsv::debuglog::tests::loggedString;
    for (auto p : replaces)
    {
        std::string srcAddr = tsv::util::tostr::hex_addr(p.first);
        size_t pos = str.find(srcAddr);
        while (pos != std::string::npos)
        {
            str.replace(pos, srcAddr.size(), p.second);
            pos = str.find(srcAddr, pos + srcAddr.size());
        }
    }
}

}

namespace tsv::debuglog::tests::objlog
{

void TrackedItem::test()
{
    // as not SENTRY defined, print in the last scope
    SAY_DBG("TrackedItem.test()");
}

TrackedItem::TrackedItem()
    : x_(1)
    // need to add ctor of the tracker to every non-default ctor and process non-default operator= too
    , debugEntry_(this,
                  // The only way to differentiate real class name
                  // is adding the argument "name" to the base class ctors (with default value to that base class name)
                  // and give real name in that argument when call base class ctor in each of derived classes.
                  // Here we just track whole hierarchy as a same kind objects.
                  baseClassName_,
                  3,
                  "default ctor")
{}

TrackedItem::TrackedItem(int x)
    : x_(x)
    , debugEntry_(
          // sample of conditional tracking (do not track if nullptr)
          (x == 3 ? nullptr : this),
          baseClassName_,
          3,
          TOSTR_JOIN("ctor x=", x).c_str())
{}

// Sample of managing of objlogger in custom copy ctor
TrackedItem::TrackedItem(const TrackedItem& src)
    : x_(src.x_)
    /* if default behavior is ok for us,
        debugEntry_(src.debugEntry_)*/
    , debugEntry_(  //
          this,
          &src,
          baseClassName_,
          3,
          TOSTR_JOIN("ctor src=", &src).c_str(),
          src.debugEntry_.getLevel(),
          src.debugEntry_.getKind())
{}


void DerivedTrackedItem::test()
{
    // as not SENTRY defined, print in the last scope
    SAY_DBG("DerivedTrackedItem.test()");
}

/************* TESTS OBJLOGGING **********************/

TrackedItem func2_val(TrackedItem arg)
{
    replaces[&arg] = "F2V";
    SENTRY_FUNC()(arg);
    arg.x_ += 100;
    return arg;
}

TrackedItem func2_move(TrackedItem&& arg)
{
    replaces[&arg] = "F2M";
    SENTRY_FUNC()("func2_move version");
    return arg;
}

TrackedItem func2_ref(TrackedItem& arg)
{
    SENTRY_FUNC()(arg);
    arg.x_ += 100;
    return arg;
}

void func1(TrackedItem tp)
{
    SENTRY_FUNC()(tp);
    TrackedItem t3_ = func2_val(tp);
    replaces[&tp] = "F1";
    replaces[&t3_] = "T3_";
    SAY_ARGS("after func2_val:", tp, t3_);
    TrackedItem rv = func2_ref(tp);
    replaces[&rv] = "F2RRV";
    SAY_ARGS("after func2_ref:", tp);
}

void test_objlog_body()
{
    SENTRY_CONTEXT("body");
    TrackedItem t1;
    TrackedItem t2(11);
    TrackedItem t3(33);
    replaces[&t1] = "T1";
    replaces[&t2] = "T2";
    replaces[&t3] = "T3";

    //@todo make known_pointers work for objlogger
    // Although ctor is always earlier than registering, it still valuable
    util::tostr::known_pointers::registerPointerNameWType(&t2, "T2KNOWN");

    SAY_ARGS(t1, t2, t3);
    t1 = t2;
    SAY_ARGS(t1, t2);
    func1(std::move(t1));

    {
        SENTRY_CONTEXT("");
        DerivedTrackedItem dt1;
        DerivedTrackedItem* dt2 = new DerivedTrackedItem();
        SAY_ARGS(dt1, dt2);
        replaces[&dt1] = "DT1";
        replaces[dt2] = "DT2";

        // Example how to determine which virtual function will be called
        TrackedItem* dt3 = &dt1;
        SAY_DBG("CALL "
                + tsv::debuglog::resolveAddr2Name(WHICH_VIRTFUNC_WILL_BE_CALLED(TrackedItem, dt3, test),
                                                  true,
                                                  true));
        dt3->test();
    }
    // Leak dt2 here

    t3 = func2_move(TrackedItem(44));
    SAY_DBG(TOSTR_ARGS(util::tostr::Details::Extended, t3));
 
    // Check for all kind of leaked object
   ::tsv::debuglog::ObjLogger::printTrackedPtr(TrackedItem::baseClassName_);
}

void test_2()
{
    // Simple test of in-place built tracker with separate kind
    // (so could turn on/off on-fly)
    SENTRY_FUNC();
    TrackedSimple o1{66};
    {
        TrackedSimple o2{o1};
        TrackedSimple o3;
        replaces[&o1] = "O1";
        replaces[&o2] = "O2";
        replaces[&o3] = "O3";
        o1 = o2;
    }

    // Check which objects of TrackedSimple are still live
    ::tsv::debuglog::ObjLogger::printTrackedPtr("Simple");
}

void test_vec()
{
    SENTRY_FUNC();
    std::vector<TrackedSimple> v;
    v.push_back({12});
    replaces[v.data()] = "V0";
    SAY_DBG("next");
    v.push_back({13});
    replaces[v.data()] = "V0A";
    replaces[v.data()+1] = "V1";
    SAY_DBG("end");
}

void run()
{
    // Prepare sequence
    isOkTotal = true;
    setupDefault();

    test_objlog_body();
    applyReplaces();
    {
        // printTrackedPtr output order depends on real address so replace it to static output
        std::regex pattern("TrackedItem .4 objects.: [^\\n]+");
        loggedString = std::regex_replace(loggedString, pattern, "TrackedItem (4 objects): DT2,T3,T2,T1");
    }

    TEST(
        "[Info:Dflt]01>{body}>> Enter scope\n"
        // construct t1, t2, t3
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:1] create T1 default ctor\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:2] create T2 ctor x=11\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:3] create T3 ctor x=33\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Dflt]01 {body}t1 = {x=1}, t2 = {x=11}, t3 = {x=33}\n"
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:3] T1 operator=(T2)\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Dflt]01 {body}t1 = {x=11}, t2 = {x=11}\n"
        // func1(std::move(t1))
        // @todo - why ctor report for unknown addr?
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:4] F1 copy_ctor(T1) ctor src=0xADDR (11)\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Dflt]02>>{objlog::func1}>> Enter tp = {x=11}\n"
        // func2_val(tp)
        "[Info:Tr]02  [track]{objlog::func1}[obj:TrackedItem:5] F2V copy_ctor(F1) ctor src=0xADDR (11)\n"
        "[Info:Tr]02  [track]{objlog::func1}stacktrace disabled. depth=3\n"
        "[Info:Dflt]03>>>{objlog::func2_val}>> Enter arg = {x=11}\n"
        // return value of func2_val
        "[Info:Tr]03   [track]{objlog::func2_val}[obj:TrackedItem:6] T3_ copy_ctor(F2V) ctor src=F2V (111)\n"
        "[Info:Tr]03   [track]{objlog::func2_val}stacktrace disabled. depth=3\n"
        "[Info:Dflt]03<<<{objlog::func2_val}>> Leave scope\n"
        "[Info:Tr]02  [track]{objlog::func1}[obj:TrackedItem:5] destroy F2V\n"
        "[Info:Tr]02  [track]{objlog::func1}stacktrace disabled. depth=3\n"
        //func2_ref
        "[Info:Dflt]02  {objlog::func1}after func2_val: tp = {x=11}, t3_ = {x=111}\n"
        "[Info:Dflt]03>>>{objlog::func2_ref}>> Enter arg = {x=11}\n"
        // return value of func2_ref
        "[Info:Tr]03   [track]{objlog::func2_ref}[obj:TrackedItem:6] F2RRV copy_ctor(F1) ctor src=0xADDR (111)\n"
        "[Info:Tr]03   [track]{objlog::func2_ref}stacktrace disabled. depth=3\n"
        "[Info:Dflt]03<<<{objlog::func2_ref}>> Leave scope\n"

        "[Info:Dflt]02  {objlog::func1}after func2_ref: tp = {x=111}\n"
        "[Info:Tr]02  [track]{objlog::func1}[obj:TrackedItem:5] destroy F2RRV\n"
        "[Info:Tr]02  [track]{objlog::func1}stacktrace disabled. depth=3\n"
        "[Info:Tr]02  [track]{objlog::func1}[obj:TrackedItem:4] destroy T3_\n"
        "[Info:Tr]02  [track]{objlog::func1}stacktrace disabled. depth=3\n"
        "[Info:Dflt]02<<{objlog::func1}>> Leave scope\n"
        // destroy arg of func1
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:3] destroy F1\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"

        // derived dt1, dt2
        "[Info:Dflt]02>>{test_objlog.cpp:181}>> Enter scope\n"
        "[Info:Tr]02  [track]{test_objlog.cpp:181}[obj:TrackedItem:4] create DT1 default ctor\n"
        "[Info:Tr]02  [track]{test_objlog.cpp:181}stacktrace disabled. depth=3\n"
        "[Info:Tr]02  [track]{test_objlog.cpp:181}[obj:TrackedItem:5] create DT2 default ctor\n"
        "[Info:Tr]02  [track]{test_objlog.cpp:181}stacktrace disabled. depth=3\n"
        "[Info:Dflt]02  {test_objlog.cpp:181}dt1 = *DT1 (tsv::debuglog::tests::objlog::DerivedTrackedItem), dt2 = DT2 (tsv::debuglog::tests::objlog::DerivedTrackedItem)\n"
        // WHICH_VIRTFUNC_WILL_BE_CALLED and actually call
        "[Info:Dflt]02  {test_objlog.cpp:181}CALL 0xADDR ??\n"
        "[Info:Dflt]02  {test_objlog.cpp:181}DerivedTrackedItem.test()\n"
        "[Info:Tr]02  [track]{test_objlog.cpp:181}[obj:TrackedItem:4] destroy DT1\n"
        "[Info:Tr]02  [track]{test_objlog.cpp:181}stacktrace disabled. depth=3\n"
        "[Info:Dflt]02<<{test_objlog.cpp:181}>> Leave scope\n"

        // inplace creation of arg
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:5] create F2M ctor x=44\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Dflt]02>>{objlog::func2_move}>> Enter func2_move version\n"
        // return value of f2_move
        "[Info:Tr]02  [track]{objlog::func2_move}[obj:TrackedItem:6] 0xADDR copy_ctor(F2M) ctor src=F2M (44)\n"
        "[Info:Tr]02  [track]{objlog::func2_move}stacktrace disabled. depth=3\n"
        "[Info:Dflt]02<<{objlog::func2_move}>> Leave scope\n"

        "[Info:Tr]01 [track]{body}[obj:TrackedItem:6] T3 operator=(0xADDR)\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:5] destroy 0xADDR\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:4] destroy F2M\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"

        "[Info:Dflt]01 {body}t3 = { addr=T3, x=44}\n"
        // what objects still exist
        "[Info:Tr]01 [track]{body} [obj] Print TrackedItem (4 objects): DT2,T3,T2,T1\n"
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:3] destroy T3\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:2] destroy T2\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Tr]01 [track]{body}[obj:TrackedItem:1] destroy T1\n"
        "[Info:Tr]01 [track]{body}stacktrace disabled. depth=3\n"
        "[Info:Dflt]01<{body}>> Leave scope\n",
        true);

    // Check for existed (leaked) object of all tracked types
    ::tsv::debuglog::ObjLogger::printTrackedPtr();
    applyReplaces();
    TEST(
        "[Info:Tr]00 [track]{core} [obj] Print all active pointers\n"
        "[Info:Tr]00 [track]{core} [obj] Print TrackedItem (1 objects): DT2\n"        
    );
    replaces.clear();

    Settings::set({{SentryLogger::Kind::TestTracked1, true}});
    test_2();
    applyReplaces();
    TEST(
        "[Info:Dflt]01>{objlog::test_2}>> Enter scope\n"
        "[Warn:]01 {objlog::test_2}[obj:Simple:1] create O1 \n"
        "[Warn:]01 {objlog::test_2}[obj:Simple:2] O2 copy_ctor_dflt(O1)\n"
        "[Warn:]01 {objlog::test_2}[obj:Simple:3] create O3 \n"
        "[Warn:]01 {objlog::test_2}[obj:Simple:3] O1 operator=(O2)\n"
        "[Warn:]01 {objlog::test_2}[obj:Simple:2] destroy O3\n"
        "[Warn:]01 {objlog::test_2}[obj:Simple:1] destroy O2\n"
        "[Info:Tr]01 [track]{objlog::test_2} [obj] Print Simple (1 objects): O1\n"
        "[Warn:]01 {objlog::test_2}[obj:Simple:0] destroy O1\n"
        "[Info:Dflt]01<{objlog::test_2}>> Leave scope\n"
    );

    Settings::setLoggerKindState(SentryLogger::Kind::TestTracked1, false);
    test_2();
    TEST(
        "[Info:Dflt]01>{objlog::test_2}>> Enter scope\n"
        // no object tracked because kind is disabled
        "[Info:Tr]01 [track]{objlog::test_2} [obj] Print Simple (0 objects): \n"
        "[Info:Dflt]01<{objlog::test_2}>> Leave scope\n"
    );

    Settings::setLoggerKindState(SentryLogger::Kind::TestTracked1, true);
    test_vec();
    applyReplaces();
    TEST(
        "[Info:Dflt]01>{objlog::test_vec}>> Enter scope\n"
        // add first item
        "[Warn:]01 {objlog::test_vec}[obj:Simple:1] create 0xADDR\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:2] V0 copy_ctor_move(0xADDR)\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:2] 0xADDR become unitialized\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:1] destroy 0xADDR\n"
        // add second item
        "[Info:Dflt]01 {objlog::test_vec}next\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:2] create 0xADDR\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:3] V1 copy_ctor_move(0xADDR)\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:3] 0xADDR become unitialized\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:4] V0A copy_ctor_dflt(V0)\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:3] destroy V0\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:2] destroy 0xADDR\n"
        // vector destroying
        "[Info:Dflt]01 {objlog::test_vec}end\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:1] destroy V0A\n"
        "[Warn:]01 {objlog::test_vec}[obj:Simple:0] destroy V1\n"
        "[Info:Dflt]01<{objlog::test_vec}>> Leave scope\n",
        true);
}

}   // namespace tsv::debuglog::tests::objlog
