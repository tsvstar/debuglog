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

// Declaration from main.cpp
bool test( bool& isOkTotal, const char* prefix, std::string val, const char* expected = nullptr, bool removeAddrFlag = false, bool checkEquality = true );
bool test( bool& isOkTotal, const char* prefix, const std::string& val,  std::function<bool(std::string_view)> fn);
std::string removeAddr(std::string);
bool isPointer(std::string_view content)
{
    return content.size()>2 && (content.substr(0,2) == "0x");
}


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


bool test_tostr()
{
    // Set predefined set of settings with minimal variativness of output
    settings::showNullptrType = true;
    settings::showUnknownValueAsRef = true;
    settings::showFnPtrContent = true;
    settings::showPtrContent = true;
    settings::showEnumInteger = true;
    resolve::settings::btEnable = false;


    int x = -10;
    int y = 15;
    double f = 0.10;
    TempClass c;
    const char* vv = "str";
    std::string ss( "std_str");
    TempClass* tc_null = nullptr;
    TestEnum e {TestEnum::V2};
    KnownClass k1{44, 55};

    bool isOk = true;

    std::cout << " ==== Test toStr() and TOSTR_* =======\n\n";


    test( isOk, "", toStr(x), "-10" );
    test( isOk, "", toStr(15), "15" );
    test( isOk, "", toStr("literal"), "literal");
    test( isOk, "", toStr(vv), "str");
    test( isOk, "", toStr(ss), "std_str");
    test( isOk, "", toStr(ss, ENUM_TOSTR_REPR), "\"std_str\"");
#if !defined(PREVENT_MAGIC_ENUM) || !defined(PREVENT_REFLECT_ENUM)
    test( isOk, "", toStr(e), "tsv::debuglog::tests::TestEnum::V2(4)");
#else
    test( isOk, "", toStr(e), "tsv::debuglog::tests::TestEnum::4");
#endif

    std::cout << "\nPointers:\n";
    std::string vv_addr( hex_addr(vv) );
    test( isOk, "", hex_addr(vv), "0xADDR", true );
    test( isOk, "", toStr(&vv), "0xADDR", true );          // Do not show content for pointer to pointer
    test( isOk, "", toStr((void*)vv), vv_addr.c_str());    // void* - show just pointer
    test( isOk, "", toStr(nullptr), "nullptr");
    test( isOk, "", toStr(tc_null), "nullptr (tsv::debuglog::tests::TempClass)" );   // Nullptr with type - shows type
    test( isOk, "", toStr(&ss), "0xADDR (std_str)", true );   // Pointer to known type shows its content
    test( isOk, "", toStr(&x), "0xADDR (-10)", true );         // (for POD types too)
    test( isOk, "", toStr(&c), "0xADDR (tsv::debuglog::tests::TempClass)", true );   // for unknown type - say its typename
#if !defined(PREVENT_MAGIC_ENUM) || !defined(PREVENT_REFLECT_ENUM)
    test( isOk, "", toStr(&e), "0xADDR (tsv::debuglog::tests::TestEnum::V2(4))", true);
#else
    test( isOk, "", toStr(&e), "0xADDR (tsv::debuglog::tests::TestEnum::4)", true);
#endif

// TODO known_pointers

    std::cout << "\nObjects of unknown type:\n";
    // Unknown object - say type. Do mark is that value or pointer
    test( isOk, "", toStr(c), "*0xADDR (tsv::debuglog::tests::TempClass)", true );
    test( isOk, "", toStr(&c), "0xADDR (tsv::debuglog::tests::TempClass)", true  );
    // Example of user-defined handler of the class. Depending on details representation is differ.
    test( isOk, "", toStr(k1), "BaseAppearance#44", true );
    test( isOk, "", toStr(k1,ENUM_TOSTR_EXTENDED), "ExtAppearance#44/55", true );


    std::cout << "\n\nComplex STL:\n";
    std::optional<std::string> emptyOpt;
    std::optional<std::string> valueOpt {"valueOpt"};
    std::variant<int, std::string> intVariant {14};
    std::variant<int, std::string> strVariant {"strVariant"};
    test( isOk, "", toStr(emptyOpt), "no_value");
    test( isOk, "", toStr(valueOpt), "valueOpt");
    test( isOk, "", toStr(intVariant), "14");
    test( isOk, "", toStr(strVariant), "strVariant");

    std::cout << "\n\nSmart pointers:\n";
    std::unique_ptr<std::string> emptyUnique;
    auto valueUnique = std::make_unique<std::string>("valueUnique");
    std::shared_ptr<std::string> emptyShared;
    auto valueShared = std::make_shared<std::string>("valueShared");
    auto valueShared2 = valueShared;
    std::weak_ptr<std::string> valueWeak{valueShared};

    test( isOk, "", toStr(emptyUnique), "unique_ptr:nullptr (std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >)", true);
    test( isOk, "", toStr(valueUnique), "unique_ptr:0xADDR (valueUnique)", true);
    test( isOk, "", toStr(emptyShared), "shared_ptr(0):nullptr (std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >)", true);
    test( isOk, "", toStr(valueShared), "shared_ptr(2):0xADDR (valueShared)", true);
    test( isOk, "", toStr(valueWeak), "weak_ptr(2):0xADDR (valueShared)", true);

    std::cout << "\nFunction pointers:\n";
    auto lambda = [](TempClass* ) -> int { return 0;};
    int (*fpNull)(int, int) = nullptr;
    int (*addPtr)(int, int) = add;
    std::function<int(int,int)> fpNull2;
    std::function<int(int,int)> addPtr2 { add };
    std::function<int(TempClass*)> lambdaPtr { lambda };

    test( isOk, "", toStr(add), "0xADDR (int (*)(int, int)/?\?)", true);
    test( isOk, "", toStr(addPtr), "0xADDR (int (*)(int, int)/?\?)", true);
    test( isOk, "", toStr(fpNull), "nullptr (int (*)(int, int))" );
    test( isOk, "", toStr(lambda), "*0xADDR (tsv::debuglog::tests::test_tostr()::{lambda(tsv::debuglog::tests::TempClass*)#1})", true); 
    test( isOk, "", toStr(addPtr2), "0xADDR (int (*)(int, int)/?\?)", true);
    test( isOk, "", toStr(fpNull2), "nullptr (int (*)(int, int))");
    test( isOk, "", toStr(lambdaPtr), "(callable type: tsv::debuglog::tests::test_tostr()::{lambda(tsv::debuglog::tests::TempClass*)#1})");


    std::cout << "\n\nMacro:\n";

    test( isOk, "", TOSTR_ARGS( x, f, "; More tests:", vv, add(x,17), y ),
                    "x = -10, f = 0.100000; More tests: vv = \"str\", add(x,17) = 7, y = 15" );

    test( isOk, "", TOSTR_JOIN( x, "; More tests:", vv, add(x,17) ), //
                    "-10; More tests:str7" );

    int res = 4 + add(x,17);
    test( isOk, "", TOSTR_EXPR( res, "=", 4, "+", add(x,17) ),
 			"res{11} = 4 + add(x,17){7} " );

    test( isOk, "", TOSTR_FMT("{1}{0}", 11, ss), //
                    "std_str11");

    test( isOk, "", TOSTR_ARGS( tsv::util::tostr::Details::NormalHex, x, f, "; More tests:", vv, add(x,17), tsv::util::tostr::Details::Normal, y  ),
                    "x = -0xa, f = 0.100000; More tests: vv = \"str\", add(x,17) = 0x7, y = 15" );

    test( isOk, "", TOSTR_ARGS( x, f, "; More tests:", tsv::util::tostr::Mode::Join /*until end or next mode*/, vv, add(x,17), tsv::util::tostr::Mode::Args, y ),
                    "x = -10, f = 0.100000; More tests:str7, y = 15" );

    test( isOk, "", TOSTR_ARGS( x, f, "; More tests:", vv, "--> ", tsv::util::tostr::Mode::JoinOnce, add(x,17), y ),
                    "x = -10, f = 0.100000; More tests: vv = \"str\"--> 7, y = 15" );


    std::cout << "\n";

    return isOk;
}

}
