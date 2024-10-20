#include "tostr.h"
#include "tostr_fmt_include.h"

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

bool test_tostr()
{
    // Set predefined set of settings with minimal variativness of output
    settings::showNullptrType = true;
    settings::showUnknownValueAsRef = true;
    settings::showFnPtrContent = true;
    settings::showPtrContent = true;
    settings::showEnumInteger = true;


    int x = 10;
    TempClass c;
    const char* vv = "str";
    std::string ss( "std_str");
    TempClass* tc_null = nullptr;

    bool isOk = true;

    std::cout << " ==== Test toStr() and TOSTR_* =======\n\n";


    test( isOk, "", toStr(x), "10" );
    test( isOk, "", toStr(15), "15" );
    test( isOk, "", toStr("literal"), "literal");
    test( isOk, "", toStr(vv), "str");
    test( isOk, "", toStr(ss), "std_str");
    test( isOk, "", toStr(ss, ENUM_TOSTR_REPR), "\"std_str\"");

    std::cout << "\nPointers:\n";
    std::string vv_addr( hex_addr(vv) );
    test( isOk, "", hex_addr(vv), "0xADDR", true );
    test( isOk, "", toStr(&vv), "0xADDR", true );          // Do not show content for pointer to pointer
    test( isOk, "", toStr((void*)vv), vv_addr.c_str());    // void* - show just pointer
    test( isOk, "", toStr(nullptr), "nullptr");
    test( isOk, "", toStr(tc_null), "nullptr (tsv::debuglog::tests::TempClass)" );   // Nullptr with type - shows type
    test( isOk, "", toStr(&ss), "0xADDR (std_str)", true );   // Pointer to known type shows its content
    test( isOk, "", toStr(&x), "0xADDR (10)", true );         // (for POD types too)
    test( isOk, "", toStr(&c), "0xADDR (tsv::debuglog::tests::TempClass)", true );   // for unknown type - say its typename

    std::cout << "\nObjects of unknown type:\n";
    // Unknown object - say type. Do mark is that value or pointer
    test( isOk, "", toStr(c), "*0xADDR (tsv::debuglog::tests::TempClass)", true );
    test( isOk, "", toStr(&c), "0xADDR (tsv::debuglog::tests::TempClass)", true  );

    std::cout << "\n\nComplex STL:\n";
    std::optional<std::string> emptyOpt;
    std::optional<std::string> valueOpt {"valueOpt"};
    std::variant<int, std::string> intVariant {14};
    std::variant<int, std::string> strVariant {"strVariant"};
    test( isOk, "", toStr(emptyOpt));
    test( isOk, "", toStr(valueOpt));
    //todo: why generic T& handler is used??
    test( isOk, "", toStr(intVariant));
    test( isOk, "", toStr(strVariant));


    std::cout << "\n\nSmart pointers:\n";
    std::unique_ptr<std::string> emptyUnique;
    auto valueUnique = std::make_unique<std::string>("valueUnique");
    std::shared_ptr<std::string> emptyShared;
    auto valueShared = std::make_shared<std::string>("valueShared");
    auto valueShared2 = valueShared;
    std::weak_ptr<std::string> valueWeak{valueShared};

    //todo: why generic T& handler is used??
    test( isOk, "", toStr(emptyUnique));
    test( isOk, "", toStr(valueUnique));
    test( isOk, "", toStr(emptyShared));
    test( isOk, "", toStr(valueShared));
    test( isOk, "", toStr(valueWeak));

    //TODO - make sure that function pointers tsv::debuglog::resolveAddr2Name doesn't resolve really
    std::cout << "\nFunction pointers:\n";
    auto lambda = [](TempClass* ) -> int { return 0;};
    int (*fpNull)(int, int) = nullptr;
    int (*addPtr)(int, int) = add;
    std::function<int(int,int)> fpNull2;
    std::function<int(int,int)> addPtr2 { add };

    test( isOk, "", toStr(add), "0xADDR (int (*)(int, int)/?\?)", true);
    test( isOk, "", toStr(addPtr), "0xADDR (int (*)(int, int)/?\?)", true);
    test( isOk, "", toStr(fpNull), "nullptr (int (*)(int, int))" );
    test( isOk, "", toStr(lambda));  //todo - show content. check clang/gcc equality
    test( isOk, "", toStr(addPtr2)); //todo - show content
    test( isOk, "", toStr(fpNull2)); //todo - show content


    std::cout << "\n\nMacro:\n";

    test( isOk, "", TOSTR_ARGS( x, "More tests:", vv, add(x,13) ),
 		"x = 10, More tests: vv = \"str\", add(x,13) = 23" );

    test( isOk, "", TOSTR_JOIN( x, "15", vv, add(x,13) ), "1015str23" );

    int res = 3 + add(x,13);
    test( isOk, "", TOSTR_EXPR( res, "=", 3, "+", add(x,13) ),
 			"res{26} = 3 + add(x,13){23} " );

    test( isOk, "", TOSTR_FMT("{1}{0}", 11, ss), "std_str11");

    //todo: level, hex, joinonce


    std::cout << "\n";

    return isOk;
}

}
