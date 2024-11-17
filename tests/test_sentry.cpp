/**
 * Tests all essential logging features here
 */

// Optional define. That is to finetuned adding debugging code compile-time.
// It could be exact value (0 or 1), or defined as a category from the own "debuglog_Categories.h"
// If the end-value is 0, then no debug code in here will be compiled.
// If this macro is not defined, DEBUGLOG_CATEG_DEFAULT value is used.
#define DEBUG_LOGGING DEBUGLOG_CATEG_TEST_ON

#include "debuglog.h"

// In most files this include doesn't needed, but here we set up handler and other settings
#include "debuglog_settings.h"
// In most files this include doesn't needed, but here we set up btEnable
#include "debugresolve.h"
// This is for SAY_VIRT_FUNC_CALL/WHICH_VIRTFUNC_WILL_BE_CALLED
// It is pretty complex and uncommonly used feature so it is separated to save compile time
#include "debugrvirtresolve.h"
// For testing scope_exit snippet
#include "debuglog_scope.h"

#include "test_tostr.h"
#include "tostr_fmt_include.h"

namespace tsv::debuglog::tests
{

extern bool isOkTotal;
extern const char* testPrefix;

// Declaration from main.cpp
bool test(std::string&& val, const char* expected, bool removeAddrFlag, bool checkEquality);

namespace
{
std::string loggedString;

void testLoggerHandlerSentry(SentryLogger::Level level, SentryLogger::Kind kind, std::string_view line)
{
    using Level = SentryLogger::Level;
    using Kind = SentryLogger::Kind;
    static std::unordered_map<Level,std::string> levels { {Level::Warning, "Warn"}, {Level::Info,"Info"}, {Level::Fatal, "Fatal"}};
    static std::unordered_map<Kind,std::string> kinds { {Kind::Default, "Dflt"}, {Kind::TestOn,"TestOn"}, {Kind::TestOff, "TestOff"}, {Kind::TestBT,"BT"}};    

//    loggedString += TOSTR_FMT("[{}:{}]{}\n", static_cast<unsigned>(level), static_cast<unsigned>(kind), line);
    loggedString += TOSTR_FMT("[{}:{}]{}\n", levels[level], kinds[kind], line);
}

void testBacktraceHandler(int depth, int /*skip*/, std::vector<std::string>& out)
{
    out.push_back( TOSTR_FMT("stacktrace disabled. depth={}", depth) );
}

bool TEST(const char* expected = nullptr, bool removeAddrFlag = false)
{
    auto rv = tsv::debuglog::tests::test(std::move(loggedString), expected, removeAddrFlag, true);
    loggedString.clear();
    return rv;
}

} // anonymous namespace

namespace test_sentry1
{

struct Base
{
    virtual void run(std::string s)
    {
        // No SENTRY_*, so print in the parent scope sentry.
        // Mention this context in the message body to differentiate from rest parent scope messages.

        // PRO:
        // - Simple and easy.
        // - Laconic output
        // - No enter/leave lines which are excessive sometimes
        // - We report where from are we came (because contextName is parent)
        // EXTRA NOTE:
        // Output depends on "parent" sentry setting. If it is turned off,
        // there were no output here.
        // That could be helpfull if the func is "on the same page".
        // That could be cons if we could came from func(s) with different Kind arg
        SAY_JOIN("base_run: ", s);
    }

    void testLogObjects(std::string s)
    {
        // Only do output if we are interested in THIS one object info
        if (IS_LOGGING_OBJ(this /*, SentryLogger::Kind::Default*/))
        {
            // Alternate way to make laconic output
            // * That is separate scope/sentry so its Kind/Level is controlable
            // * There is no entry/leave message (SILENT)
            // * We still mention from we came (2nd arg which is appendParentContext is true)
            SENTRY_SILENT("testLogObjects", true /*appendParentContext*/ /*SentryLogger::Kind::Default*/);

            // Adjust sentry output level (we still could override level individually with SAY_*_L macroses)
            SENTRYLOGGER_DO(setLogLevel)(SentryLogger::Level::Warning);

            SAY_DBG(s);
        }
    }
};

struct Child : public Base
{
    void run(std::string s) override { SAY_JOIN("child_run: ", s); }
};


/**
 *      BODIES OF TESTS
 */

void testRawCall()
{
    int x = 11;
    // If no SENTRY_* given, message is printed in the last scope
    // At least in the scope named "[core]" if no other happened.
    SAY_ARGS("Test ", x);
}

void testTrivial()
{
    // SENTRY_FUNC could have no parameters.
    // That means Kind::Default/Level::Default
    SENTRY_FUNC();

    // If we need to print is string or convertible to it (literal, string_view),
    // then we could do just SAY_DBG()
    SAY_DBG("test");
}

int testRegularUsage(int x, std::string s)
{
    // Print variables with TOSTR_ARGS
    // First parenthesis contains init state.
    // No need to define its content - by default it is Kind::Default/Level::Default
    SENTRY_FUNC()(x, s);

    // If no argument given, that means -1 (print whole stack)
    SAY_STACKTRACE(/*{.depth=10}*/);

    // simple print as args with same level as whole sentry
    SAY_ARGS(x, s);

    // Output with different level than sentry level
    // Print with Warning instead of Info (which is Sentry default)
    SAY_ARGS_L(SentryLogger::Level::Warning, "with_level ", x);

    if ( x < 0)
        SAY_AND_RETURN( x, " -- early exit" );
    SAY_AND_RETURN(x + 5);
}

int testScopeExit()
{
    int rv = 0;
    SENTRY_FUNC();
    // That is a technique to catch multiple identical returns without SAY_AND_RETURN
    // or we can do on-exit values snapshots
    // Remind that we need to create named object to make lifetime till end of scope
    auto tmp = scope_exit( [&]{ SENTRYLOGGER_DO(setReturnValueStr)(std::to_string(rv)); });
    rv = 44;
    return rv;
}

int testBacktraceKind(int x)
{
    // TestBT used as depth value.
    // This is example that same kind is used as On(!=0)/Off(0)
    SENTRY_FUNC( {/*.kind=*/SentryLogger::Kind::TestBT} )(x);

    // ... and as depth of corresponding stacktraces.
    // Note that the kind value turn on/off sentry totally because used as a kind in SENTRY
    SAY_STACKTRACE({/*.depth=*/ Settings::getLoggerKindState(SentryLogger::Kind::TestBT)});

    // Example that uages of SAY_AND_RETURN() is optional 
    return x;
}


void testContextSentry( int x )
{
    // If automatic name of the SENTRY_FUNC is too long, we could use SENTRY_CONTEXT
    // 1. If arguments and/or init state needed - SENTRY_CONTEXT("name", {}, x, ..);
    // 2. Sometimes it is useful to see what about that func call for all SAY_*()
    //    then we can build the name like - SENTRY_CONTEXT( "name" + tostr::hex_addr(this) );
    SENTRY_CONTEXT( "testContext" )(x);

    // Example of conditional stacktrace (print it only if x is not negative)
    // with different output level
    SAY_STACKTRACE( {/*depth=*/ (x < 0) ? 0 : 10}, SentryLogger::Level::Fatal);

    int value = 0;
    for (int i = 0; i<3; i+=2 )
    {
        SENTRY_CONTEXT( "Context_loop_"+std::to_string(i),
                        {/*.kind=*/SentryLogger::Kind::TestOn} );
        int new_value = value + i;

        // Example of SAY_EXPR usage
        SAY_EXPR( new_value, " = ", value, "+", i);
        value = new_value;
    }

    if (x < 10)
    {
        // Empty name of context is converted to filename:lineno
        // As well as SENTRY_FUNC, it accepts arguments to print in optional second parenthesis
        SENTRY_CONTEXT("")(x);
        //do something 
    }
}

void testAppend()
{
    /* In C++20 we can define just .flags= with designated initializer*/
    SENTRY_FUNC({/*.kind=*/SentryLogger::Kind::Default,
                 /*.level=*/SentryLogger::Level::Default,
                 /*.flags=*/SentryLogger::Flags::AppendContextName});
}

void testDisabledSentry(int arg)
{
    /* In C++20 we can define just .enable= with designated initializer*/
    SENTRY_FUNC({/*.kind=*/SentryLogger::Kind::Default,
                 /*.level=*/(arg%2)?SentryLogger::Level::Default : SentryLogger::Level::Off,
                 /*.flags=*/SentryLogger::Flags::Default,
                 /*.enabled=*/arg > 50});

    // Both DISABLED (.enable=false) and TURNED OFF(Level over threshold/turned off kinds) sentries
    // means no enter/leave and usual output for the scope
    SAY_STACKTRACE();
    // sample of modifier
    SAY_ARGS( ::tsv::util::tostr::Details::NormalHex, arg);

    // DISABLED sentry is not counted in the path...
    // It doesn increase depth, it doesn't reffered by AppendContextName.
    // But TURNED OFF just silent
    testAppend();
}

void testExtendedArg( SentryLogger::Kind kind, int val)
{
    tsv::debuglog::tests::KnownClass k{ val, 0};
    SENTRY_FUNC( {/*.kind=*/kind})(::tsv::util::tostr::Details::Extended, k );
}

void testNestedExtended()
{
    SENTRY_CONTEXT("testNestedExtended",
                   {/*kind=*/SentryLogger::Kind::TestOn,
                    /*.level=*/SentryLogger::Level::Warning});

    // Level::Default + enabled
    testDisabledSentry(61);
    // Level::Off + enabled
    testDisabledSentry(60);
    // Level::Default + disabled
    testDisabledSentry(41);

    // normal output
    testExtendedArg(SentryLogger::Kind::TestOn, 10);
    // turned off kind
    testExtendedArg(SentryLogger::Kind::TestOff, 20);
}

void testVirtFunc()
{
    SENTRY_FUNC();

    // Test SAY_VIRT_FUNC
    std::unique_ptr<Base> objPtr { new Child() };
    SAY_VIRT_FUNC_CALL(Base, objPtr.get(), run, " -- we will call this one");
    objPtr->run("str");
}

void testObj()
{
    Base obj1;
    Child obj2;
    
    // Test the feature which help to user's objects know are we interested in related to it data

    SENTRY_CONTEXT("testObj");

    {
        // Temporarily register obj1 as "we should trace related to it data".
        // NOTE1: Enum "Kind" for logobjects is reused as category of object. It is optional.
        // NOTE2: nullptr temporarily allow to track all objects of this kind
        EXECUTE_IF_DEBUGLOG( logobjects::Guard guard(&obj1/*, SentryLogger::Kind::Default*/) );

        // So obj1 log its operation
        obj1.testLogObjects("obj1_temp");
        // But obj2 doesn't
        obj2.testLogObjects("obj2_temp");

        // we can enable all objects of the kind
        EXECUTE_IF_DEBUGLOG( logobjects::setAllowAll(true/*, SentryLogger::Kind::Default*/) );
        obj1.testLogObjects("obj1_total_allow");
        obj2.testLogObjects("obj2_total_allow");
        
        // The reverse operation is not "disable all", but reset that enabling
        EXECUTE_IF_DEBUGLOG( logobjects::setAllowAll(false/*, SentryLogger::Kind::Default*/) );
        obj1.testLogObjects("obj1_after_reset");
        obj2.testLogObjects("obj2_after_reset");
    }

    // guard is over here, so no "allowed to track" objects mentioned
    obj1.testLogObjects("obj1_final");
    obj2.testLogObjects("obj2_final");
}

void testTimerSentry()
{
    SENTRY_CONTEXT("testTimer1");
    // start timer sentry 1. do not stop it manually, so it reports on function exit
    SENTRYLOGGER_DO(setFlag)(SentryLogger::Flags::Timer);

    int x=55;
    {
        // it is possible to use TOSTR_* in SENTRY_ to make non-standard formatting
        SENTRY_CONTEXT("testTimer2")(TOSTR_FMT("the x is {}",x));
        // start timer for the sentry 2
        SENTRYLOGGER_DO(setFlag)(SentryLogger::Flags::Timer);
        // stop it. that reports results of measuring immediatelly
        SENTRYLOGGER_DO(setFlag)(SentryLogger::Flags::Timer, false);
        SAY_DBG("test");
        // and do not report on scope exit
    }
    // timer of sentry 1 still on, so report on exit
}

bool run()
{
    isOkTotal = true;

    resolve::settings::btEnable = false;
    resolve::settings::btDisabledOutputCallback = testBacktraceHandler;

    using Op = Settings::Operation;
    Settings::setOutputHandler(testLoggerHandlerSentry);
    SentryLogger::getRoot()->setLogLevel( SentryLogger::Level::Warning );
    Settings::setCutoffNamespaces( {"tsv::debuglog::tests::"} );
    // sample names
    Settings::setKindNames( {{SentryLogger::Kind::TestOn, "[on]"},
                             {SentryLogger::Kind::TestBT, "[BT]"}}
                             );

    Settings::set( { { SentryLogger::Level::Info, Op::SetLogLevel},        // system logging level
                     { SentryLogger::Level::Info, Op::SetDefaultLogLevel}, // this level assigned to sentry if not given

                     // NOTE: Settings::enableStacktrace() and resolve::settings::btEnable are different flags.
                     //    First one determine should SAY_STACKTRACE() do anything or not
                     //    Second determine if stacktrace feature available in the system.
                     { Op::EnableStackTraceTag{}, true},          // stacktrace enabled

                     // Turn off Kind::TestOff, and turn on rest kinds
                     { SentryLogger::Kind::Default, true },
                     { SentryLogger::Kind::TestOn,  true },
                     { SentryLogger::Kind::TestOff, false },

                     // Kind could be separated or combined category which contains integer value.
                     // This example is combine totally on/off kind and stacktrace depth
                     // take a look test_sentry to see how it could be compile-time disabled
                     { SentryLogger::Kind::TestBT,  DEBUGLOG_CATEG_TEST_BT }
                   } );


    testRawCall();
    testTrivial();
    TEST(
        // raw print - there is no sentry in caller scope stack, so that is "core" sentry
        "[Warn:Dflt]00 {core}Test  x = 11\n"
        // context name is truncated from full because of setCutoffNamespaces
        "[Info:Dflt]01>{test_sentry1::testTrivial}>> Enter scope\n"
        "[Info:Dflt]01 {test_sentry1::testTrivial}test\n"
        "[Info:Dflt]01<{test_sentry1::testTrivial}>> Leave scope\n"
        );

    {
        // Test temporary settings change. Check kind state effect
        // NOTE 1: If sentry is turned off, then no enter/leave and any SAY_* related to the scope printed
        // NOTE 2: Although that is not so quick as compile-time disable, it still very quick 
        //         and do not calculatute any values in printed list.
        Settings::TemporarySettings tmp( { { SentryLogger::Kind::Default,  false } } );
        testRegularUsage(11, "kind_off");
    }
    {
        // Check system level effect (No default output because default=Info, which is larger than system=Warning)
        Settings::TemporarySettings tmp( { { SentryLogger::Level::Warning, Op::SetLogLevel } } );
        testRegularUsage(12, "syslevel");
    }
    {
        // Check default level effect (No default output because default=Trace, which is larger than system=Info)
        Settings::TemporarySettings tmp( { { SentryLogger::Level::Trace, Op::SetDefaultLogLevel}
                                     } );
        testRegularUsage(13, "dfltlevel");
    }
    TEST(
        // No otput for "kind_off", because sentry kind is turned off
        // No stacktrace + sentry enter/leave, because their Info > system Warning
        "[Warn:Dflt]01 {test_sentry1::testRegularUsage}with_level  x = 12\n"
        // No stacktrace + sentry enter/leave, because their default Trace > system Info
        "[Warn:Dflt]01 {test_sentry1::testRegularUsage}with_level  x = 13\n"
    );

    // No temporary settings should effect here. Full output
    testRegularUsage(-4, "regular");
    TEST(
        // enter message contains input arguments to print (often "this" is in the list).
        // that is not a required to be only function input arg, that is input data
        "[Info:Dflt]01>{test_sentry1::testRegularUsage}>> Enter x = -4, s = \"regular\"\n"
        // stacktrace to that point is helpfull. by default print whole stack
        "[Info:Dflt]01 {test_sentry1::testRegularUsage}stacktrace disabled. depth=-1\n"
        // regular output has same level as a sentry
        "[Info:Dflt]01 {test_sentry1::testRegularUsage}x = -4, s = \"regular\"\n"
        // but it is possible output with different level
        "[Warn:Dflt]01 {test_sentry1::testRegularUsage}with_level  x = -4\n"
        // return value could be added to the leave message. it could contains remark about in what branch it is
        "[Warn:Dflt]01<{test_sentry1::testRegularUsage}>> Leave scope. RV = -4 -- early exit\n"
    );

    testScopeExit();
    TEST(
        "[Info:Dflt]01>{test_sentry1::testScopeExit}>> Enter scope\n"
        // this return value is catched automatically
        "[Info:Dflt]01<{test_sentry1::testScopeExit}>> Leave scope. RV = 44\n"
    );

    testBacktraceKind(18);
    TEST(
        // first block printed because kind equals to 10 (on)
        "[Info:BT]01>[BT]{test_sentry1::testBacktraceKind}>> Enter x = 18\n"
        // that value is reused as depth of stacktrace
        "[Info:BT]01 [BT]{test_sentry1::testBacktraceKind}stacktrace disabled. depth=10\n"
        "[Info:BT]01<[BT]{test_sentry1::testBacktraceKind}>> Leave scope\n"
    );

    // change backtrace kind. that also turns off whole sentry scope
    Settings::set( {{ SentryLogger::Kind::TestBT,  0 }} );
    testBacktraceKind(18);
    TEST(
        // no output because the kind used by sentry is 0 (Off)
        ""
    );

    testContextSentry(3);
    TEST(
        "[Info:Dflt]01>{testContext}>> Enter x = 3\n"
        "[Fatal:Dflt]01 {testContext}stacktrace disabled. depth=10\n"
        "[Info:TestOn]02>>[on]{Context_loop_0}>> Enter scope\n"
        "[Info:TestOn]02  [on]{Context_loop_0}new_value{0}  =  value{0} + i{0} \n"
        "[Info:TestOn]02<<[on]{Context_loop_0}>> Leave scope\n"
        "[Info:TestOn]02>>[on]{Context_loop_2}>> Enter scope\n"
        "[Info:TestOn]02  [on]{Context_loop_2}new_value{2}  =  value{0} + i{2} \n"
        "[Info:TestOn]02<<[on]{Context_loop_2}>> Leave scope\n"
        "[Info:Dflt]02>>{test_sentry.cpp:212}>> Enter x = 3\n"
        "[Info:Dflt]02<<{test_sentry.cpp:212}>> Leave scope\n"
        "[Info:Dflt]01<{testContext}>> Leave scope\n"
    );

    testNestedExtended();
    TEST(
        // Outer scope. Sample of level and kind assigning
        "[Warn:TestOn]01>[on]{testNestedExtended}>> Enter scope\n"

        // Nested 1 - sample conditional runtime enabling (odd so level=Dflt)
        "[Info:Dflt]02>>{test_sentry1::testDisabledSentry}>> Enter scope\n"
        "[Info:Dflt]02  {test_sentry1::testDisabledSentry}stacktrace disabled. depth=-1\n"
        // sample of output as a hex
        "[Info:Dflt]02  {test_sentry1::testDisabledSentry}arg = 0x3d\n"
        // Nested1.1
        // a) with default setting output is organized as a ladder, so easy to see nesting, 
        //    do lookup by level number, by enter/leave sign with context
        // b) example of Flags::AppendContextName. It is usefull if the func is called from different places
        //    and do a lot of output -- to specify its context without looking up outer scope  
        "[Info:Dflt]03>>>{test_sentry1::testDisabledSentry--test_sentry1::testAppend}>> Enter scope\n"
        "[Info:Dflt]03<<<{test_sentry1::testDisabledSentry--test_sentry1::testAppend}>> Leave scope\n"
        "[Info:Dflt]02<<{test_sentry1::testDisabledSentry}>> Leave scope\n"

        // Nested 2.1 - testDisabledSentry scope exists but turned off(even so level=Off).
        // It doesn't print its enter/leave, but affects on nesting depth and appended context name
        "[Info:Dflt]03>>>{test_sentry1::testDisabledSentry--test_sentry1::testAppend}>> Enter scope\n"
        "[Info:Dflt]03<<<{test_sentry1::testDisabledSentry--test_sentry1::testAppend}>> Leave scope\n"

        // Nested 3.1 - testDisabledSentry scope skipped(conditionally .enabled=false).
        // It produce no output and hidden in the stack. The best choice to do not waste output
        "[Info:Dflt]02>>{testNestedExtended--test_sentry1::testAppend}>> Enter scope\n"
        "[Info:Dflt]02<<{testNestedExtended--test_sentry1::testAppend}>> Leave scope\n"

        // Nested 4. Sample of how trigger extended output for the object for which that is possible
        "[Info:TestOn]02>>[on]{test_sentry1::testExtendedArg}>> Enter k = ExtAppearance#10/0\n"
        "[Info:TestOn]02<<[on]{test_sentry1::testExtendedArg}>> Leave scope\n"
        // Nested 5. Next call testExtendedArg output nothing because its kind is turned off run-time
        // Leave outer scope
        "[Warn:TestOn]01<[on]{testNestedExtended}>> Leave scope\n"
    );

    testVirtFunc();
    TEST(
        "[Info:Dflt]01>{test_sentry1::testVirtFunc}>> Enter scope\n"
        // it is possible to detect which virt function will be called for the object
        // but as its address dynamic, not easy to get and symbolresolving is disabled -- no real check here.
        // In real output instead of just "0xADDR ??" there will be address and full name of called function.
        "[Info:Dflt]01 {test_sentry1::testVirtFunc}Call vfunc 0xADDR ?? -- we will call this one\n"
        "[Info:Dflt]01 {test_sentry1::testVirtFunc}child_run: str\n"
        "[Info:Dflt]01<{test_sentry1::testVirtFunc}>> Leave scope\n",
        true /*convert addr*/
    );

    testObj();
    TEST(
        //@todo -- why silent append nothing
        "[Info:Dflt]01>{testObj}>> Enter scope\n"
        // obj1 registered as "object of interest" by guard, while obj2 doesn't
        "[Warn:Dflt]02  {testLogObjects}obj1_temp\n"
        // here setAllowAll for the kind is in action, so both objects are accessible
        "[Warn:Dflt]02  {testLogObjects}obj1_total_allow\n"
        "[Warn:Dflt]02  {testLogObjects}obj2_total_allow\n"
        // reset setAllowAll - so again only obj1 is guarded
        "[Warn:Dflt]02  {testLogObjects}obj1_after_reset\n"
        // and out of guard's scope both objects are not "objects of interest"
        "[Info:Dflt]01<{testObj}>> Leave scope\n"
    );

    testTimerSentry();
    TEST(
        "[Info:Dflt]01>{testTimer1}>> Enter scope\n"
        "[Info:Dflt]02>>{testTimer2}>> Enter the x is 55\n"
        //@todo - suppress real value. do explanation here and there
        "[Info:Dflt]02  {testTimer2}Processed time: 0.0000s\n"
        "[Info:Dflt]02  {testTimer2}test\n"
        "[Info:Dflt]02<<{testTimer2}>> Leave scope\n"
        // this tracks whole testTimer1 scope
        "[Info:Dflt]01<{testTimer1}>> Leave scope. Processing time = 0.0000s\n"
    );

//@todo -why doesn't print kind?? because map is not initialized. do that via settings vector<pair<>>

    return isOkTotal;
}

} // namespace test_sentry1
} // namespace tsv::debuglog::tests
