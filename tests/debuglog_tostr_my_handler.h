#pragma once

/**
 * Example of user-defined TOSTR handlers.
 * Define your own header file with such name and place it to your include directory to automatically catch
*/

// Forward declaration of involved user entites
namespace tsv::debuglog::tests
{
struct KnownClass;
namespace objlog
{
struct TrackedItem;
struct TrackedSimple;
}
}


namespace tsv::util::tostr::impl
{
// Example of user-class tostr handler declaration.
// Take a look definition in the debuglog_tostr_my_handler.cpp
// Normally, assumed that you place handlers definition in such dedicated file(s) in your project
ToStringRV __toString(const tsv::debuglog::tests::KnownClass& value, int mode);
ToStringRV __toString(const tsv::debuglog::tests::objlog::TrackedItem& value, int mode);
ToStringRV __toString(const tsv::debuglog::tests::objlog::TrackedSimple& value, int mode);

// Declare your own handlers here ...

}
