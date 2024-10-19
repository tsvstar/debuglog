#include <iostream>
#include <functional>
#include <string>
#include "../tostr.h"

namespace tsv::debuglog::tests
{

// Declaration from main.cpp
bool test( bool& isOkTotal, const char* prefix, std::string val, const char* compare = nullptr, bool equal = true );
bool test( bool& isOkTotal, const char* prefix, std::string val, std::function<bool(std::string_view)> compare);
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
    std::string ss( "std_string");

    bool isOk = true;

    std::cout << " ==== Test toStr() and TOSTR_* =======\n\n";


    test( isOk, "", toStr(x), "10" );
    test( isOk, "", toStr(15), "15" );
    test( isOk, "", toStr("literal"), "literal");
    test( isOk, "", toStr(vv), "str");
    test( isOk, "", toStr(ss), "std_string");
    test( isOk, "", toStr(ss, ENUM_TOSTR_REPR), "\"std_string\"");

    std::cout << "\nPointers:\n";
    std::string vv_addr( hex_addr(vv) );
    test( isOk, "", removeAddr(hex_addr(vv)), "0xADDR" );
    test( isOk, "", toStr( &vv ) ); // todo
    test( isOk, "", toStr( (void*)vv ), vv_addr.c_str() );
    test( isOk, "", toStr( nullptr ), "nullptr");
    test( isOk, "", toStr( &ss ) );   // todo

    std::cout << "\nObjects:\n";
    test( isOk, "", toStr( c ) );    // unknown object - say type. It differ depends on compiler
                                        // so just print not test
    test( isOk, "", toStr( &c ) );   // address could differ, so just print not test

    std::cout << "\n\nMacro:\n";
    test( isOk, "", TOSTR_ARGS( x, "15", vv, add(x,13) ),
 		"x = 10, 15 vv = \"str\", add(x,13) = 23" );
    test( isOk, "", TOSTR_JOIN( x, "15", vv, add(x,13) ), "1015str23" );

    int res = 3 + add(x,13);
    test( isOk, "", TOSTR_EXPR( res, "=", 3, "+", add(x,13) ),
 			"res{26} = 3 + add(x,13){23} " );

    std::cout << "\n";

    return isOk;
}

}
