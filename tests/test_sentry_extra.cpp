/**
 * Test conditional compile-time
 */

// DEBUG_LOGGING could be complex condition
// We could have different compile-time categories in the same translation unit
// and differentially apply the state to each statement
#define DEBUG_LOGGING (DEBUGLOG_CATEG_TEST_ON || DEBUGLOG_CATEG_TEST_OFF)

#include "debuglog.h"

#include "tostr_fmt_include.h"
#include "main.h"

namespace tsv::debuglog::tests
{

namespace test_sentry2
{

int test_default(int x, int y)
{
    // That works if the debuglog is enabled for the file
    // (true for this case because DEBUGLOG_CATEG_TEST_ON is 1 and so DEBUG_LOGGING is true also)
    SENTRY_FUNC();
    SENTRYLOGGER_DO(setLogLevel)(SentryLogger::Level::Warning);
    SAY_STACKTRACE({/*.depth=*/11});
    SAY_ARGS(y);
    // just check if IS_LOGGING_OBJ compilable and works (gives false)
    if (!IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/))
        SAY_JOIN(IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/));
    SAY_AND_RETURN(x+y);
}

int test_cond_func_on(int x,int y)
{
    // Example how to manage compile-time categories if several categories used in the file
    // DEBUGLOG_CATEG_TEST_ON is 1 (on) so the sentry and all its dependends is compiled and works
    // Example of extended usage (with kind and args)
    SENTRY_FUNC_COND(DEBUGLOG_CATEG_TEST_ON, {/*.kind=*/SentryLogger::Kind::TestOn})(x);
    SENTRYLOGGER_DO(setLogLevel)(SentryLogger::Level::Warning);
    SAY_STACKTRACE({/*.depth=*/11});
    SAY_ARGS(y);
    // just check if IS_LOGGING_OBJ compilable and works (gives false)
    if (!IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/))
        SAY_JOIN(IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/));
    SAY_AND_RETURN(x+y);
}

int test_cond_func_off(int x,int y)
{
    // In this sample sentry and all its dependands are disabled compile-time
    // So produce nothing for **optimized** build
    // That is also check short-form of the sentry
    SENTRY_FUNC_COND(DEBUGLOG_CATEG_TEST_OFF);
    SENTRYLOGGER_DO(setLogLevel)(SentryLogger::Level::Error);
    SAY_STACKTRACE({/*.depth=*/11});
    SAY_ARGS(y);
    if (!IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/))
        SAY_JOIN(IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/));
    SAY_AND_RETURN(x+y);
}

int test_cond_context(int x, int y)
{
    {
        // Short form of context sentry. Turned on for compile-time
        SENTRY_CONTEXT_COND(DEBUGLOG_CATEG_TEST_ON, "context_on");
        SENTRYLOGGER_DO(setLogLevel)(SentryLogger::Level::Error);
        SAY_STACKTRACE({/*.depth=*/11});
        SAY_ARGS(y);
        if (!IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/))
            SAY_JOIN(IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/));
    }

    {
        // Long form of context sentry. Turned off for compile-time
        SENTRY_CONTEXT_COND(DEBUGLOG_CATEG_TEST_OFF, "context_off", {/*.kind=*/SentryLogger::Kind::TestBT})(x);
        SENTRYLOGGER_DO(setLogLevel)(SentryLogger::Level::Warning);
        SAY_STACKTRACE({/*.depth=*/11});
        SAY_ARGS(y);
        if (!IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/))
            SAY_JOIN(IS_LOGGING_OBJ(&loggedString /*, SentryLogger::Kind::Default*/));
        SAY_AND_RETURN(x+y);
    }
}

void run()
{
    isOkTotal = true;
    setupDefault();

    test_default(0, -1);
    TEST("[Info:Dflt]01>{test_sentry2::test_default}>> Enter scope\n"
         "[Warn:Dflt]01 {test_sentry2::test_default}stacktrace disabled. depth=11\n"
         "[Warn:Dflt]01 {test_sentry2::test_default}y = -1\n"
         "[Warn:Dflt]01 {test_sentry2::test_default}false\n"
         "[Warn:Dflt]01<{test_sentry2::test_default}>> Leave scope. RV = -1\n");

    test_cond_func_on(1, 2);
    TEST("[Info:TestOn]01>[on]{test_sentry2::test_cond_func_on}>> Enter x = 1\n"
         "[Warn:TestOn]01 [on]{test_sentry2::test_cond_func_on}stacktrace disabled. depth=11\n"
         "[Warn:TestOn]01 [on]{test_sentry2::test_cond_func_on}y = 2\n"
         "[Warn:TestOn]01 [on]{test_sentry2::test_cond_func_on}false\n"
         "[Warn:TestOn]01<[on]{test_sentry2::test_cond_func_on}>> Leave scope. RV = 3\n"
         );

    test_cond_func_off(3, 4);
    // DEBUGLOG_CATEG_TEST_OFF is 0, so the sentry is replaced to the stub compile-time and no action made
    TEST("");

    test_cond_context(5, 6);
    // Only first context sentry is compiled and produce output
    TEST("[Info:Dflt]01>{context_on}>> Enter scope\n"
         "[Err:Dflt]01 {context_on}stacktrace disabled. depth=11\n"
         "[Err:Dflt]01 {context_on}y = 6\n"
         "[Err:Dflt]01 {context_on}false\n"
         "[Err:Dflt]01<{context_on}>> Leave scope\n"
         );

}

} // namespace test_sentry2
} // namespace tsv::debuglog::tests
