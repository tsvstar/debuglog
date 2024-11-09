/**
 *  Tests of TOSTR module
 */

#include "tostr.h"
#include "tostr_fmt_include.h"	// smart include of format library

// For resolve::settings:btEnable
#include "debugresolve.h"

#include "test_tostr.h"

#include <iostream>
#include <functional>
#include <string>
#include <optional>
#include <memory>
#include <variant>


namespace tsv::debuglog::tests
{

extern bool isOkTotal;
extern const char* testPrefix;

// Declaration from main.cpp
bool test(std::string&& val, const char* expected = nullptr, bool removeAddrFlag = false, bool checkEquality = true );


/************** TEST **********/

using namespace tsv::util::tostr;

// TEST
struct TempClass
{
    int x;
};

int add( int a, int b )
{
    return a + b;
}

enum class TestEnum : unsigned
{
   V1 = 3,
   V2
};

std::string knownPtrLog = "";
void MyKnownPtrLogger(const std::string& log_str )
{
   knownPtrLog += (knownPtrLog.empty() ? "" : "|") + log_str;
}


bool test_tostr()
{
    // Set predefined set of settings with minimal variativness of output
    settings::showNullptrType = true;
    settings::showUnknownValueAsRef = true;
    settings::showFnPtrContent = true;
    settings::showPtrContent = true;
    settings::showEnumInteger = true;
    resolve::settings::btEnable = false;
    known_pointers::known_ptr_logger_s = MyKnownPtrLogger;

    int x = -10;
    int y = 15;
    double f = 0.10;
    TempClass c, c1;
    const char* vv = "str";
    std::string ss( "std_str");
    TempClass* tc_null = nullptr;
    TestEnum e {TestEnum::V2};
    KnownClass k1{44, 55};

    // Set test-suite global variables
    isOkTotal = true;
    testPrefix = "";

    std::cout << " ==== Test toStr() and TOSTR_* =======\n\n";

    std::cout << "BASICS:\n";
    test( toStr(x), "-10" );
    test( toStr(15), "15" );
    test( toStr("literal"), "literal");
    test( toStr(vv), "str");
    test( toStr(ss), "std_str");
    test( toStr(ss, ENUM_TOSTR_REPR), "\"std_str\"");
#if !defined(PREVENT_MAGIC_ENUM) || !defined(PREVENT_REFLECT_ENUM)
    test( toStr(e), "tsv::debuglog::tests::TestEnum::V2(4)");
#else
    test( toStr(e), "tsv::debuglog::tests::TestEnum::4");
#endif

    std::cout << "\nPOINTERS:\n";
    std::string vv_addr( hex_addr(vv) );
    test( hex_addr(vv), "0xADDR", true );
    test( toStr(&vv), "0xADDR", true );          // Do not show content for pointer to pointer
    test( toStr((void*)vv), vv_addr.c_str());    // void* - show just pointer
    test( toStr(nullptr), "nullptr");
    test( toStr(tc_null), "nullptr (tsv::debuglog::tests::TempClass)" );   // Nullptr with type - shows type
    test( toStr(&ss), "0xADDR (std_str)", true );   // Pointer to known type shows its content
    test( toStr(&x), "0xADDR (-10)", true );         // (for POD types too)
    test( toStr(&c), "0xADDR (tsv::debuglog::tests::TempClass)", true );   // for unknown type - say its typename
#if !defined(PREVENT_MAGIC_ENUM) || !defined(PREVENT_REFLECT_ENUM)
    test( toStr(&e), "0xADDR (tsv::debuglog::tests::TestEnum::V2(4))", true);
#else
    test( toStr(&e), "0xADDR (tsv::debuglog::tests::TestEnum::4)", true);
#endif


    std::cout << "\nOBJECTS:\n";
    // Unknown object - say type. Do mark is that value or pointer
    test( toStr(c), "*0xADDR (tsv::debuglog::tests::TempClass)", true );
    test( toStr(&c), "0xADDR (tsv::debuglog::tests::TempClass)", true  );
    // Example of user-defined handler of the class. Depending on details representation is differ.
    test( toStr(k1), "BaseAppearance#44", true );
    test( toStr(k1,ENUM_TOSTR_EXTENDED), "ExtAppearance#44/55", true );

    std::cout << "\nFEATURE known_pointers:\n";

    test( std::to_string(known_pointers::isPointerRegistered(&c)), "0" );  // "&c" is not registered yet
    test( known_pointers::getName(&c), "");  // for not registered return ""

    known_pointers::registerPointerName(&c1, "var_c");
    known_pointers::registerPointerName(&c, "var_C_initial");
    test( std::to_string(known_pointers::isPointerRegistered(&c)), "1" );  // "&c" is already registered
    test( known_pointers::getName(&c), "var_C_initial" );

    // Reuse existed name - to differentiate them a new instance get index suffix
    known_pointers::registerPointerName(&c, "var_c");
    test( std::to_string(known_pointers::isPointerRegistered(&c)), "1" );  // "&c" is still registered
    test( known_pointers::getName(&c), "var_c$1");
    // and that is how known pointers are represented in the log
    test( toStr(&c), "var_c$1[0xADDR] (tsv::debuglog::tests::TempClass)", true);

    // If nullptr give, then do not register pointer. That is the way to conditionally track it.
    bool track = false;
    known_pointers::registerPointerName( (track ? &f : nullptr), "var_F");
    test( std::to_string(known_pointers::isPointerRegistered(&f)), "0" );
	
    // registerPointerNameWType() can be used for case from the name of var or given name the typename is unclear but it is important
    known_pointers::registerPointerNameWType(&k1, "var_K");
    test( toStr(&k1), "var_K (tsv::debuglog::tests::KnownClass)[0xADDR] (BaseAppearance#44)", true );

    // Mismatched type of pointer doesn't remove it
    known_pointers::deletePointerName( reinterpret_cast<void*>(&c));
    test( std::to_string(known_pointers::isPointerRegistered(&c)), "1" );  // "&c" is still registered

    // Delete pointer (after its free). Normally it doesn't delete the pointer from known list, but mark it as removed.
    // That could help to catch use-after-free
    known_pointers::deletePointerName(&c);
    test( std::to_string(known_pointers::isPointerRegistered(&c)), "1" );
    test( known_pointers::getName(&c), "var_c$1(removed)" );

    // Check what is in the output
    test(std::move(knownPtrLog),
         "Register knownPtr 0xADDR = var_c // tsv::debuglog::tests::TempClass|"
         "Register knownPtr 0xADDR = var_C_initial // "
         "tsv::debuglog::tests::TempClass|"
         "Replace knownPtr 0xADDR = var_c$1 // tsv::debuglog::tests::TempClass|"
         "Register knownPtr 0xADDR = var_K (tsv::debuglog::tests::KnownClass) "
         "// tsv::debuglog::tests::KnownClass|"
         "Delete knownPtr 0xADDR:  var_c$1 // tsv::debuglog::tests::TempClass",
         true);

    std::cout << "\n\nSTL - Advanced:\n";
    std::optional<std::string> emptyOpt;
    std::optional<std::string> valueOpt {"valueOpt"};
    std::variant<int, std::string> intVariant {14};
    std::variant<int, std::string> strVariant {"strVariant"};
    test( toStr(emptyOpt), "no_value");
    test( toStr(valueOpt), "valueOpt");
    test( toStr(intVariant), "14");
    test( toStr(strVariant), "strVariant");

    std::cout << "\n\nSTL - Smart pointers:\n";
    std::unique_ptr<std::string> emptyUnique;
    auto valueUnique = std::make_unique<std::string>("valueUnique");
    std::shared_ptr<std::string> emptyShared;
    auto valueShared = std::make_shared<std::string>("valueShared");
    auto valueShared2 = valueShared;
    std::weak_ptr<std::string> valueWeak{valueShared};

    test( toStr(emptyUnique), "unique_ptr:nullptr (std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >)", true);
    test( toStr(valueUnique), "unique_ptr:0xADDR (valueUnique)", true);
    test( toStr(emptyShared), "shared_ptr(0):nullptr (std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >)", true);
    test( toStr(valueShared), "shared_ptr(2):0xADDR (valueShared)", true);
    test( toStr(valueWeak), "weak_ptr(2):0xADDR (valueShared)", true);

    std::cout << "\nFUNCTION POINTERS:\n";
    auto lambda = [](TempClass* ) -> int { return 0;};
    int (*fpNull)(int, int) = nullptr;
    int (*addPtr)(int, int) = add;
    std::function<int(int,int)> fpNull2;
    std::function<int(int,int)> addPtr2 { add };
    std::function<int(TempClass*)> lambdaPtr { lambda };

    test( toStr(add), "0xADDR (int (*)(int, int)/?\?)", true);
    test( toStr(addPtr), "0xADDR (int (*)(int, int)/?\?)", true);
    test( toStr(fpNull), "nullptr (int (*)(int, int))" );
    test( toStr(lambda), "*0xADDR (tsv::debuglog::tests::test_tostr()::{lambda(tsv::debuglog::tests::TempClass*)#1})", true); 
    test( toStr(addPtr2), "0xADDR (int (*)(int, int)/?\?)", true);
    test( toStr(fpNull2), "nullptr (int (*)(int, int))");
    test( toStr(lambdaPtr), "(callable type: tsv::debuglog::tests::test_tostr()::{lambda(tsv::debuglog::tests::TempClass*)#1})");


    std::cout << "\n\nMACRO:\n";

    test( TOSTR_ARGS( "ARGS:", x, f, "; More tests:", vv, add(x,17), y ),
                    "ARGS: x = -10, f = 0.100000; More tests: vv = \"str\", add(x,17) = 7, y = 15" );

    test( TOSTR_JOIN( "JOIN:", x, "; More tests:", vv, add(x,17) ), //
                    "JOIN:-10; More tests:str7" );

    int res = 4 + add(x,17);
    test( TOSTR_EXPR( "EXPR:", res, "=", 4, "+", add(x,17) ),
 			"EXPR: res{11} = 4 + add(x,17){7} " );

    test( TOSTR_FMT("FMT: {1}{0}", 11, ss), //
                    "FMT: std_str11");

    test( TOSTR_ARGS( "HEX:", tsv::util::tostr::Details::NormalHex, x, f, "; More tests:", vv, add(x,17), tsv::util::tostr::Details::Normal, y  ),
                    // todo: Minor issue - initial ','. Not think should take care of.
                    "HEX:, x = -0xa, f = 0.100000; More tests: vv = \"str\", add(x,17) = 0x7, y = 15" );

    test( TOSTR_ARGS( "ARGS+:", x, f, "; More tests:", tsv::util::tostr::Mode::Join /*until end or next mode*/, vv, add(x,17), tsv::util::tostr::Mode::Args, y ),
                    "ARGS+: x = -10, f = 0.100000; More tests:str7, y = 15" );

    test( TOSTR_ARGS( "ARGS+JOIN:", x, f, "; More tests:", vv, "--> ", tsv::util::tostr::Mode::JoinOnce, add(x,17), y ),
                    "ARGS+JOIN: x = -10, f = 0.100000; More tests: vv = \"str\"--> 7, y = 15" );

     // test empty macro
    test( TOSTR_ARGS() + TOSTR_JOIN() + TOSTR_EXPR(), "");

    // test a lot of arguments
    int a1 = 11, a2 = 22, a3=33, a4=44;
    test( TOSTR_ARGS("HUGE:", 2,3,4,5,6,7,8,9,a1,1,2,3,4,5,6,7,8,9,a2,1,2,3,4,5,6,7,8,9,a3,1,2,3,4,5,6,7,8,9,a4,1,2,3,4,5,6),
          // no space because both 0 and 1 args are non-vars
          "HUGE:23456789, a1 = 11123456789, a2 = 22123456789, a3 = 33123456789, a4 = 44123456");


    std::cout << "\n";

    return isOkTotal;
}

}
