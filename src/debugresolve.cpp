/**
  Purpose: Resolving symbols and calltrace
  Author: Taranenko Sergey
  Date: 02-Aug-2017 ... 19-Oct-2024
  License: BSD. See License.txt
*/

#ifndef BACKTRACE_AVAILABLE
#define BACKTRACE_AVAILABLE 1
#endif

#ifndef BACKTRACE_USE_ADDR2LINE
#define BACKTRACE_USE_ADDR2LINE 1
#endif

#ifndef ADDR2LINE_PATH
#define ADDR2LINE_PATH "/usr/bin/addr2line"
#endif

#include "tostr_fmt_include.h"
#include "debugresolve.h"
#include "tostr.h"      // for ::tsv::util::tostr::hex_addr and TOSTR_FMT
//#include "debuglog.h"

#include <string>
#include <mutex>
#include <cstdlib>
#include <cstring>      //strlen
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <unistd.h>
#include <sstream>

#if BACKTRACE_AVAILABLE && __has_include(<execinfo.h>)
#include <execinfo.h>
#else
#undef BACKTRACE_AVAILABLE
#define BACKTRACE_AVAILABLE 0
#endif

#if !BACKTRACE_AVAILABLE
#undef BACKTRACE_USE_ADDR2LINE
#define BACKTRACE_USE_ADDR2LINE 0
#endif

#if BACKTRACE_USE_ADDR2LINE
#include <signal.h>     // kill()
#endif

#if defined(__GNUG__) || defined(__clang__)
#include <cxxabi.h>
#endif


//TODO: suppress or redirect pipe stderr
// [doens't help because address is a separate line] TODO: add to addr2line key --addresses. In some circrumstances addr2line do not answer anything on given addr, but with that option it at least echo with addr so this at least just "addr\n" and prevent hanging up
//TODO: Should be fallback if fail to run ADDR2LINE or not defined - to recover just func names
//TODO: ignoreList (if substring found in the path or startswith). request +15 just in case. ignored are counted as a number but not displayed and doesn't includeded into size(depth)
//TODO: cached ok - ignored means empty values in the cache(? then we need move out cache from addr2line) or be unordered_map of good/bad signatures


namespace
{
    std::mutex debugResolverMutex;
}


namespace tsv::debuglog
{

namespace resolve::settings
{
    // Backtrace specific stopwords. If function match to this word, break backtrace
    std::unordered_set<std::string> backtraceStopWords = 
           {
             "??",
             "main"
           };

    // Backtrace tunings
    bool btEnable = true;          // if false - resolveAddr2Name() returns "??" and getStackTrace() returns "Backtrace feature is unavailable"
    bool btIncludeLine = true;     // if true, include to stacktrace "at file:line"
    bool btIncludeAddr = true;     // if true, include to stacktrace addr
    bool btShortList = true;       // if true, remember already printed stacktraces and just say it's number on repeat
    bool btShortListOnly = false;  // if true, do not print full stack - only short line
    int  btNumHeadFuncs = 4;       // how many first functions include into collapsed stacktrace
}


#if defined(__GNUG__) || defined(__clang)
std::string demangle(const char* name) {

    int status = -4; // some arbitrary value to eliminate the compiler warning

    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle( name, nullptr, nullptr, &status ),
        std::free
    };

    return (status==0) ? res.get() : name ;
}

#else

// does nothing if not g++
std::string demangle(const char* name) {
    return name;
}
#endif

/***************** BEGIN OF local aux functions *******************************/


namespace {
namespace symbol_resolve {

/***************************************************************************
     Symbol resolving ( call external utility addr2line and parse output )
***************************************************************************/

#if BACKTRACE_USE_ADDR2LINE

// PURPOSE: split string "s" to vector "elems" by delimiter "delim"
int ssplit( const std::string &s, char delim, std::vector<std::string> &elems )
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems.size();
}

// PURPOSE: squeeze ".." from path
// Auxilary function to make path shorter
std::string squeezePath( const std::string& path )
{
    std::vector<std::string> v;
    ssplit( path, '/', v );
    for ( int i = 2; i < static_cast<int>(v.size()); )
    {
        if ( i <= 1 )
          { i++; continue; }
        if ( !v[i].length() )
         {  v.erase(v.begin()+i); continue; }
        if ( v[i]!=".." )
          { i++; continue; }

        v.erase( v.begin()+i-1, v.begin()+i+1 );
        i--;
    }

    std::string result;
    for ( int i = 1; i < static_cast<int>(v.size()); i++ )
        result += std::string("/")+v[i];
    return result;
}

/***************************************************************************
        Auxilary class which actually run child "addr2line" and
            communicate with it to resolve address
***************************************************************************/
class Addr2LineResolver
{
   public:
        Addr2LineResolver()
        {
           child_pid_ = 0;
           // cleanup other session
           if ( system("killall -9 " ADDR2LINE_PATH " 2>/dev/null") < 0 )
                perror("Fail to killall:");
        }

        ~Addr2LineResolver()
        {
            if ( child_pid_ > 0 )
                kill( child_pid_, 9 );
            else if ( child_pid_ < 0)
                kill( -child_pid_, 9);
            if ( child_pid_ != 0 )
            {
                close( pipefd_[0] );
                close( pipefd_[1] );
            }
        }


        struct CacheEntry
        {
            std::string funcName_;
            std::string pathName_;
            std::string getFuncAndLine() { return funcName_ + pathName_; }
            std::string getSymbol( bool includeLine ) { return includeLine ? (funcName_ + pathName_) : funcName_; }
        };

        CacheEntry request(const void* addr);

        static bool isStopWord( const std::string& funcName )
        {
             using namespace tsv::debuglog::resolve::settings;
             //return resolve::settings::isStopWord( funcname );
	     return (backtraceStopWords.find(funcName) != backtraceStopWords.end());
        }

   private:
        std::unordered_map<const void*, CacheEntry> addrCache_;
        char  buf_[512];
        pid_t child_pid_;       // 0=do not exists yet, <0=failed
        int   pipefd_[2];       // [0]=to say child, [1]=listen child

   private:
        static pid_t popen2( const char *command, int *infp, int *outfp );
        void pipe_say();
        void pipe_getline();
        static bool checkstopwords();

};

// Main method: ask child and parse answer about name/line
Addr2LineResolver::CacheEntry Addr2LineResolver::request(const void* addr )
{
    // Protection check
    if ( !addr )
        return { "nullptr", "" };

    auto it = addrCache_.find( addr );
    if ( it != addrCache_.end() )
         return it->second;

    if ( child_pid_ == 0 )
    {
        int n = snprintf(buf_, sizeof(buf_), ADDR2LINE_PATH " -f -C -e `readlink /proc/%d/exe`", getpid() );
        if (static_cast<std::size_t>(n+1) >= sizeof(buf_))
        {
            //SAY_DBG( "Unable to run addr2line - command buffer overflow" );
            child_pid_=-1;
        }
        else
        {
            child_pid_ = popen2( buf_, &pipefd_[0], &pipefd_[1] );
            if ( child_pid_ <= 0)
            {
                //SAY_DBG( "Unable to exec: rv=%d\n", child_pid_ );
                child_pid_=-1;
            }
        }
    }
    if (child_pid_ <= 0)
         return { "{no info}", "" };

    sprintf(buf_, "%p\n", addr);
    pipe_say();

    pipe_getline();
    std::string funcName( buf_ );

    pipe_getline();

    std::string path;
    if ( path.find( "??:" ) != 0 )
    {
        path =  buf_ ;

        if ( path.find( "/.." ) != std::string::npos )
            path = squeezePath( path );

        static const std::string at_str(" at ");
        path =  at_str + path;
    }

    CacheEntry entry { funcName, path };

    addrCache_[addr] = entry;
    return entry;
}

// Run command and bind with pipes to descriptors *infp/*outfp
pid_t Addr2LineResolver::popen2( const char *command, int *infp, int *outfp )
{
    int p_stdin[2], p_stdout[2];
    pid_t pid;

    enum {PIPEREAD=0, PIPEWRITE=1};

    if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        return -1;

    pid = fork();

    if (pid < 0)
        return pid;
    else if (pid == 0)
    {
        close(p_stdin[PIPEWRITE]);
        dup2(p_stdin[PIPEREAD], PIPEREAD);
        close(p_stdout[PIPEREAD]);
        dup2(p_stdout[PIPEWRITE], PIPEWRITE);

        execl("/bin/sh", "sh", "-c", command, nullptr );
        perror("execl");
        exit(1);
    }

    if ( infp == nullptr )
        close(p_stdin[PIPEWRITE]);
    else
        *infp = p_stdin[PIPEWRITE];

    if ( outfp == nullptr )
        close(p_stdout[PIPEREAD]);
    else
        *outfp = p_stdout[PIPEREAD];

    return pid;
}

// send string buf_ to child
void Addr2LineResolver::pipe_say()
{
        if ( child_pid_ <= 0 )
                return;
        int ln = write( pipefd_[0], buf_, strlen( buf_ ) ); // write message to the process
        if ( ln < 1 )
        {
            perror("Fail to pipe write:");
            child_pid_ = -child_pid_;
        }

}

// get string from child to buf_ (and cut off terminal \n )
void Addr2LineResolver::pipe_getline()
{
        buf_[0] = 0;
        if ( child_pid_ <= 0 )
                return;

        std::size_t idx = 0;
        char ch;
        int len;
        for(;;)
        {
            len = read( pipefd_[1], &ch, 1 );
            if ( len < 1 )
            {
                child_pid_ = -child_pid_;
                buf_[idx] = 0;
                perror("fail read pipe");
                return;
            }

            if ( ch == '\n' )
            {
                buf_[idx] = 0;
                return;
            }
            if ( idx > ( sizeof(buf_) - 5 ) )
                continue;
            buf_[idx++] = ch;
        }
}

auto resolveAddr(const void* addr)
{
    static Addr2LineResolver a2l_resolver;
    return a2l_resolver.request(addr);
}

#endif // BACKTRACE_USE_ADDR2LINE

#if BACKTRACE_AVAILABLE
/***************************************************************************
        Create stacktraces

 NOTE: For performance and simplicity reason Key is just 64bit-hash
       so collision are highly unlike but possible
***************************************************************************/

// AUX: Simple quick hash
uint64_t FNV1aHash ( const unsigned char *buf, uint64_t len )
{
  uint64_t hval = 0x811c9dc5;

    for ( ; len; len--)
    {
      hval ^= static_cast<uint64_t>( *buf++ );
      hval *= 1099511628211;
    }

  return hval;
}

// AUX: Convert array of pointers to hash (to catch repeating)
uint64_t makeKey( void* ar[], int bufsize )
{
    return FNV1aHash( reinterpret_cast<const unsigned char*>(ar), bufsize*sizeof(void*) );
}

// PURPOSE:   Create short notation of stacktrace ( just few func names from begin and end )
// ARGUMENTS: vector of function names
std::string collapseNames( const std::vector<std::string>& func_names )
{
    const int lastIdx = func_names.size()-1;
    std::string res;
    std::string_view::size_type found;

    res.reserve(30 * (lastIdx > resolve::settings::btNumHeadFuncs ? resolve::settings::btNumHeadFuncs : lastIdx));
    for ( int i=0; i<=lastIdx; i++)
    {
        if ( i == resolve::settings::btNumHeadFuncs && lastIdx > resolve::settings::btNumHeadFuncs )
            res.append(" - ...");
        if ( i >= resolve::settings::btNumHeadFuncs && i < lastIdx )
            continue;
        std::string_view fname( func_names[i] );
        found = fname.find_first_of( " (" );
        if (found == std::string_view::npos)
            res.append(" - ").append(fname).append("()");
        else
            res.append(" - ").append(fname.substr(0,found)).append("()");
    }
    if (res.length()<3)
        return res;
    return res.substr(3);
}

#endif
}   // namespace symbol_resolve
}   // anonymous namespace

/***************** END OF local aux functions *******************************/


std::string resolveAddr2Name(const void* addr, bool addLineNum /*=false*/, bool includeHexAddr /*= false */)
{
    if (!BACKTRACE_AVAILABLE || !resolve::settings::btEnable)
    {
        if (!includeHexAddr)
            return "?\?";
        return ::tsv::util::tostr::hex_addr(addr) + " ?\?";
    }

#if BACKTRACE_USE_ADDR2LINE
    std::lock_guard<std::mutex> lock(debugResolverMutex);
    auto symbolEntry = symbol_resolve::resolveAddr(addr);
    if ( !includeHexAddr )
        return symbolEntry.getSymbol( addLineNum );
    return ::tsv::util::tostr::hex_addr( addr ) + " " +symbolEntry.getSymbol( addLineNum );
#endif
}


// Get stack backtrace
// ARGUMENTS:
//      depth   = how many levels print
//      skip    = how many first levels skipped
// RETURN VALUE:
//      vector of values to display
//===================================================
std::vector<std::string> getBackTrace( int depth /*=-1*/, int skip /*=0*/)
{
    std::vector<std::string> return_value;

    if (!BACKTRACE_AVAILABLE || !resolve::settings::btEnable)
        return_value.push_back( "Backtrace feature is not available" );

#if BACKTRACE_AVAILABLE
    static int localSkip = 0;  // offset to skip this function (0 is unitialized)

    // Prepare values
    depth = depth < 0 ? 100 : depth;
    skip  = skip < 0  ? 0 : skip;

    // skip this function
    skip += !localSkip ? 1 : localSkip;

    depth += skip;
    depth =  depth > 100 ? 100 : depth;

    std::lock_guard<std::mutex> lock(debugResolverMutex);

    // VLA syntax is supported by Clang/GCC
//todo: warning: ISO C++ forbids variable length array �array� [-Wvla]
    void* array[depth+2];
    // Get backtrace using GNU C std library (execinfo.h)
    int size = backtrace( array, depth < 2 ? 2 : depth );
    if ( depth < size )
       depth = size;

    // It is possible that backtrace() call could be also included into stacktrace
    // For example, LLVM with sanitizer includes it as ___interceptor_backtrace.
    // So dynamically detect on first call how many start entries really should be skipped
    if (!localSkip && size>1)
    {
        auto thisSymbol = symbol_resolve::resolveAddr( reinterpret_cast<void*>(tsv::debuglog::getBackTrace) );
        auto symbolEntry = symbol_resolve::resolveAddr( array[1] );
        localSkip = (symbolEntry.funcName_ == thisSymbol.funcName_) ? 2 : 1;
    }

    // Remember printed backtraces and later use its id only
    if ( resolve::settings::btShortList || resolve::settings::btShortListOnly )
    {
	// cachedStackTrace[ calltrace_hash ] = { short_notation_str, callstack_id_int }
//todo: adapt to any width of ptr. not only to 64bit
        static std::unordered_map< uint64_t, std::pair< std::string, int > > cachedStackTrace;

        uint64_t key = symbol_resolve::makeKey( array+skip, depth-skip );
        auto it = cachedStackTrace.find( key );
        if ( it != cachedStackTrace.end() )
        {
            // This stacktrace was already mentioned -- USE SHORT NOTATION ONLY (to make shorter output)
            auto& value = it->second;
            return_value.push_back( TOSTR_FMT("StackTrace#{} - repeated: {}", value.second, value.first) );
            return return_value;
        }
        else if ( (depth-skip)==1 )
        {
            // This stacktrace wasn't mentioned before (special case with single entry). Remember it with path
            auto symbolEntry = symbol_resolve::resolveAddr(array[skip]);
            auto shortName = symbolEntry.getSymbol( resolve::settings::btIncludeLine );
            int stackTraceId = cachedStackTrace.size()+1;
            cachedStackTrace[ key ] = std::make_pair( shortName, stackTraceId );

            return_value.push_back( TOSTR_FMT( " .. StackTrace#{} : {}", stackTraceId, shortName ) );
        }
        else
        {
            // This stacktrace wasn't mentioned before. Remember it

            // (a) create function name list
            std::vector<std::string> tracedNames;
            for ( int i = skip; i < depth; i++ )
            {
                auto symbolEntry = symbol_resolve::resolveAddr(array[i]);
                if ( !symbolEntry.funcName_.length() )
                   break;
                tracedNames.push_back( symbolEntry.funcName_ );
                if (resolve::settings::backtraceStopWords.find(symbolEntry.funcName_) != resolve::settings::backtraceStopWords.end())
                   break;
            }

            // (b) Create short notation and remember it
            std::string shortName( symbol_resolve::collapseNames( tracedNames ) );
            int stackTraceId = cachedStackTrace.size()+1;
            cachedStackTrace[ key ] = std::make_pair( shortName, stackTraceId );

            return_value.push_back( TOSTR_FMT( " .. StackTrace#{} : {}", stackTraceId, shortName ) );
        }
    }

    // If only short notation is requested, return it
    if ( resolve::settings::btShortListOnly )
        return return_value;

    // Fill lines-by-line stacktrace
    for ( int i = skip; i < depth; i++ )
    {
        auto symbolEntry = symbol_resolve::resolveAddr(array[i]);
        if ( !symbolEntry.funcName_.length() )
           break;

        if ( resolve::settings::btIncludeAddr )
            return_value.push_back( TOSTR_FMT( " .. #{:02}[{:x}] {}", i, array[i], symbolEntry.getSymbol( resolve::settings::btIncludeLine )) );
        else
            return_value.push_back( TOSTR_FMT( " .. #{:02} {}", i, symbolEntry.getSymbol( resolve::settings::btIncludeLine ) ) );
        if (resolve::settings::backtraceStopWords.find(symbolEntry.funcName_) != resolve::settings::backtraceStopWords.end())
            break;
    }
#endif
    return return_value;
}


}   // namespace tsv::debug
