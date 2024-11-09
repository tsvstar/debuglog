#pragma once

/**
  Purpose: Display named variables and expression
  Author: Taranenko Sergey (tsvstar@gmail.com)
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

#include <vector>
#include "tostr_handler.h"

// make possible macros overriding
#undef TOSTR_ARGS
#undef TOSTR_JOIN
#undef TOSTR_EXPR
#undef TOSTR_FMT


/******************** MACRO TO USE ***********************************/

// Print to string list of arguments with their names
//   Example:  std::cout << TOSTR_ARGS( str_var, " abc", 3, !func(arg) ) << "\n";
//   Output:   strvar = "VALUE1" abc, 3 = 3, !func(arg) = VALUE2
#define TOSTR_ARGS(...) ::tsv::util::tostr::Printer( ::tsv::util::tostr::Mode::Args, { MACRO_TOSTR__EXPAND_EACH_VAL(__VA_ARGS__) } ).do_print( __VA_ARGS__ )

// Print to string converted concatenation of all given values ( implicitly converted to string using toStr() )
//   Example:  std::cout << TOSTR_JOIN( str_var, " abc", 3, !func(arg) ) << "\n";
//   Output:   VALUE1 abc3VALUE2
#define TOSTR_JOIN(...) ::tsv::util::tostr::Printer( ::tsv::util::tostr::Mode::Join, {} ).do_print( __VA_ARGS__ )

// Combination of TOSTR_ARGS() and TOSTR_JOIN() - use literals/arithmetical as is, but name_of_var with its value for vars/calls/..
// Use it for quickly represent simple formulas
//   Example:  std::cout << TOSTR_EXPR( res, "=", 3, "+", !func(arg) ) << "\n";
//   Output:  res{VALUE_RES} = 3 + !func(arg){RETURN_VALUE_OF_CALL}
#define TOSTR_EXPR(...) ::tsv::util::tostr::Printer( ::tsv::util::tostr::Mode::Expr, { MACRO_TOSTR__EXPAND_EACH_VAL(__VA_ARGS__) } ).do_print( __VA_ARGS__ )

#if __has_include(<fmt/format.h>)
#define TOSTR_FMT(...) fmt::format( __VA_ARGS__ )
#elif __has_include(<format>)
#define TOSTR_FMT(...) std::format( __VA_ARGS__ )
#else
#define TOSTR_FMT(...) static_assert(false, "Can't use TOSTR_FMT - no format library detected") )
#endif

// It is possible to switch mode dynamically (give tsv::util::tostr::Mode::enum) or detalization (give tsv::util::tostr::Details::Extended).
// Extended representation of value could be different for user-defined complex class printers.

/************************* Implementation ***********************************/

namespace tsv::util::tostr
{

enum Mode : short {
        Args,
        Expr,
        Join,
        JoinOnce   // special: temporarily switch to "Join" mode for the one next valuable argument
     };

enum Details : short {
        Normal,
        NormalHex,
        Extended
     }; 

class Printer
{
public:

    std::vector<const char*> names_;  // List of variable names (used for Mode::Args, Mode::Expr)
    Mode mode_;                       // Printing mode
    Details detailed_ = Details::Normal;
    bool joinOnce_ = false;

    std::size_t index_;               // current index of argument
    std::string acc_str_;             // string which accumulate output of do_print()

    Printer( Mode mode, std::vector<const char*> names )
         : names_( names ), mode_( mode )
     {
     }

    template<typename... StrTail>
     std::string do_print( StrTail&&... tail )
    {
        index_ = 0;
        acc_str_ = "";
        return print( std::forward<StrTail>( tail )... );
    }

private:
    // StartsWith
    static std::string_view toStrDetectLiteral_;
 
    static bool startsWith(std::string_view base, std::string_view lookup );

    // Final print
    std::string print()
    {
       return std::move( acc_str_ );
    }

    template<typename... Tail>
    std::string print(Mode head, Tail&&... tail)
    {
        if (head==Mode::JoinOnce)
           joinOnce_ = true;
        else
           mode_ = head;
        index_++;
        return print( std::forward<Tail>( tail )... );
    }

    template<typename... Tail>
    std::string print(Details head, Tail&&... tail)
    {
        detailed_ = head;
        index_++;
        return print( std::forward<Tail>( tail )... );
    }

    // Recursive parse templates
    // Note: use formatters instead of plain branches to make only one instantiation of toStr
    //   [to minimize error output in case if something goes wrong]
    template<typename Head, typename... Tail>
    std::string print(const Head& head, Tail&&... tail)
    {
        int modeToStr = ENUM_TOSTR_DEFAULT; // token representation mode
        bool asisFlag = true;               // print as is or not? (literal token)
        bool excludeName = true;            // exclude varname from output
        const char* betweenToken = " = ";   // token inserted between name and value
        const char* suffix = "";            // token added after value

        // 1. Tune output format + print separator if needed
        if ( mode_ == Mode::Args && !joinOnce_)
        {
            if ( index_ >= names_.size())
            {
                asisFlag = false;
            }
            else
            {
                char first = names_[index_][0];     // first symbol of #arg (detect literals and numbers)
                asisFlag = (first == '"' || first=='\'' || ( first >= '0' && first <= '9' )
                            || startsWith(names_[index_], toStrDetectLiteral_));
                excludeName = asisFlag;
            }

            // add separator
            if ( !asisFlag && !acc_str_.empty() )
            {
                char prev = names_[index_-1][0];
                acc_str_ += ( prev != '"' ? ", " : " " );
            }

            modeToStr = ( detailed_==Details::Normal  ? ENUM_TOSTR_REPR : 
                          (detailed_==Details::NormalHex ? ENUM_TOSTR_REPR_HEX : ENUM_TOSTR_EXTENDED) );
        }
        else if ( mode_ == Mode::Expr && !joinOnce_)
        {
            if ( index_ >= names_.size())
            {
                asisFlag = false;
            }
            else
            {
                char first = names_[index_][0];     // first symbol of #arg (detect literals and numbers)
                asisFlag = (first == '"' || first=='\'' || ( first >= '0' && first <= '9' )
                            || startsWith(names_[index_], toStrDetectLiteral_));
                excludeName = asisFlag;
            }
            modeToStr = ( detailed_==Details::Normal  ? ENUM_TOSTR_REPR : 
                          (detailed_==Details::NormalHex ? ENUM_TOSTR_REPR_HEX : ENUM_TOSTR_EXTENDED) );
            if ( !excludeName )
            {
                betweenToken = "{";         //between name and value
                suffix = "} ";              //after value
            }
            else
            {
                //betweenToken = "nomatter";
                suffix = " ";
            }
        }
        else
        {
            // tune format
            //modeToStr = ENUM_TOSTR_DEFAULT;// use default: print values as STR
            modeToStr = ( (detailed_==Details::NormalHex &&   std::is_integral<Head>::value) ? ENUM_TOSTR_REPR_HEX : ENUM_TOSTR_DEFAULT);
            //excludeName = true;            // use default: no varnames needed
            //asisFlag = ASIS;               // use default: always print as is
            //betweenToken = "no matter";    // use default: because have no matter
            //suffix = "";                   // use default: because have no matter
        }

        // 2. Do output
        if ( !excludeName )
            acc_str_ += std::string(names_[index_]) + betweenToken;
        acc_str_ +=  toStr( head, asisFlag ? ENUM_TOSTR_DEFAULT : modeToStr ) + suffix;

        index_++;
        joinOnce_ = false;
        return print( std::forward<Tail>( tail )... );
    }

};

}  // namespace tsv::util::tostr


/****************  Auxilarly macros ******************/

#define MACRO_TOSTR__CHOOSE_NTH(a50,a49,a48,a47,a46,a45,a44,a43,a42,a41,a40,a39,a38,a37,a36,a35,a34,a33,a32,a31,a30,a29,a28,a27,a26,a25,a24,a23,a22,a21,a20,a19,a18,a17,a16,a15,a14,a13,a12,a11,a10,a09,a08,a07,a06,a05,a04,a03,a02,a01, num, ...) num
//                                                                        (a50,a49,a48,a47,a46,a45,a44,a43,a42,a41,a40,a39,a38,a37,a36,a35,a34,a33,a32,a31,a30,a29,a28,a27,a26,a25,a24,a23,a22,a21,a20,a19,a18,a17,a16,a15,a14,a13,a12,a11,a10,a09,a08,a07,a06,a05,a04,a03,a02,a01, num
#define MACRO_TOSTR__GET_ARG_NUM(...) MACRO_TOSTR__CHOOSE_NTH( __VA_ARGS__,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 09, 08, 07, 06, 05, 04, 03, 02, 01, 00 )
#define MACRO_TOSTR__EXPAND_EACH_00()
#define MACRO_TOSTR__EXPAND_EACH_01(a01) #a01
#define MACRO_TOSTR__EXPAND_EACH_02(a01,a02) #a01, #a02
#define MACRO_TOSTR__EXPAND_EACH_03(a01,a02,a03) #a01, #a02, #a03
#define MACRO_TOSTR__EXPAND_EACH_04(a01,a02,a03,a04) #a01, #a02, #a03, #a04
#define MACRO_TOSTR__EXPAND_EACH_05(a01,a02,a03,a04,a05) #a01, #a02, #a03, #a04, #a05
#define MACRO_TOSTR__EXPAND_EACH_06(a01,a02,a03,a04,a05,a06) #a01, #a02, #a03, #a04, #a05, #a06
#define MACRO_TOSTR__EXPAND_EACH_07(a01,a02,a03,a04,a05,a06,a07) #a01, #a02, #a03, #a04, #a05, #a06, #a07
#define MACRO_TOSTR__EXPAND_EACH_08(a01,a02,a03,a04,a05,a06,a07,a08) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08
#define MACRO_TOSTR__EXPAND_EACH_09(a01,a02,a03,a04,a05,a06,a07,a08,a09) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09
#define MACRO_TOSTR__EXPAND_EACH_10(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10
#define MACRO_TOSTR__EXPAND_EACH_11(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11
#define MACRO_TOSTR__EXPAND_EACH_12(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12
#define MACRO_TOSTR__EXPAND_EACH_13(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13
#define MACRO_TOSTR__EXPAND_EACH_14(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14
#define MACRO_TOSTR__EXPAND_EACH_15(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15
#define MACRO_TOSTR__EXPAND_EACH_16(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16
#define MACRO_TOSTR__EXPAND_EACH_17(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17
#define MACRO_TOSTR__EXPAND_EACH_18(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17,a18) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17, #a18
#define MACRO_TOSTR__EXPAND_EACH_19(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17, #a18, #a19
#define MACRO_TOSTR__EXPAND_EACH_20(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17, #a18, #a19, #a20
#define MACRO_TOSTR__EXPAND_EACH_21(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17, #a18, #a19, #a20, #a21
#define MACRO_TOSTR__EXPAND_EACH_22(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17, #a18, #a19, #a20, #a21, #a22
#define MACRO_TOSTR__EXPAND_EACH_23(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17, #a18, #a19, #a20, #a21, #a22, #a23
#define MACRO_TOSTR__EXPAND_EACH_24(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17, #a18, #a19, #a20, #a21, #a22, #a23, #a24
#define MACRO_TOSTR__EXPAND_EACH_25(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17, #a18, #a19, #a20, #a21, #a22, #a23, #a24, #a25
#define MACRO_TOSTR__EXPAND_EACH_N(a01,a02,a03,a04,a05,a06,a07,a08,a09,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,...) #a01, #a02, #a03, #a04, #a05, #a06, #a07, #a08, #a09, #a10, #a11, #a12, #a13, #a14, #a15, #a16, #a17, #a18, #a19, #a20, #a21, #a22, #a23, #a24, #a25, MACRO_TOSTR__EXPAND_EACH_VAL_AGAIN(__VA_ARGS__)
#define MACRO_TOSTR__TOKEN_PASTE_IMPL(x,y) x ## y
#define MACRO_TOSTR__TOKEN_PASTE(x,y) MACRO_TOSTR__TOKEN_PASTE_IMPL(x,y)
#define MACRO_TOSTR__EXPAND_EACH_VAL(...) MACRO_TOSTR__TOKEN_PASTE(MACRO_TOSTR__EXPAND_EACH_, MACRO_TOSTR__GET_ARG_NUM(__VA_ARGS__))( __VA_ARGS__ )
#define MACRO_TOSTR__EXPAND_EACH_VAL_AGAIN(...) MACRO_TOSTR__TOKEN_PASTE(MACRO_TOSTR__EXPAND_EACH_, MACRO_TOSTR__GET_ARG_NUM(__VA_ARGS__))( __VA_ARGS__ )
