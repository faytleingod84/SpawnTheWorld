// Minimal Windows x64 /GS runtime for an intentionally CRT-free mod build.
// This TU MUST be compiled with /GS- to avoid checking a cookie before it
// has been initialized in DllMain. See README before altering build flags.
typedef unsigned long long u64;
typedef long long i64;
typedef unsigned long DWORD;
extern "C" {
__declspec(dllimport) u64 __stdcall GetTickCount64();
__declspec(dllimport) int __stdcall QueryPerformanceCounter(i64*);
__declspec(dllimport) DWORD __stdcall GetCurrentThreadId();
__declspec(dllimport) DWORD __stdcall GetCurrentProcessId();
}
#pragma intrinsic(__fastfail)
static const u64 DEFAULT_COOKIE=0x00002B992DDFA232ULL;
extern "C" u64 __security_cookie=DEFAULT_COOKIE;
extern "C" u64 __security_cookie_complement=~DEFAULT_COOKIE;

extern "C" void stwInitializeSecurityCookie(){
  // Entropy mixing before any protected code can execute.  Mirrors MSVC's
  // requirement for a non-default, process-specific security cookie, rather
  // than hardcoding a fixed value as the stock no-CRT build otherwise would.
  i64 counter=0;
  QueryPerformanceCounter(&counter);
  u64 seed=(u64)counter ^ (GetTickCount64()<<21)
         ^ ((u64)GetCurrentProcessId()<<17)
         ^ ((u64)GetCurrentThreadId()<<33)
         ^ (u64)(void*)&counter ^ (u64)(void*)&stwInitializeSecurityCookie;
  seed ^= (seed >> 29); seed *= 0x9E3779B97F4A7C15ULL;
  seed ^= (seed >> 31);
  // MSVC x64 masks off the top 16 bits to avoid noncanonical addresses.
  seed &= 0x0000FFFFFFFFFFFFULL;
  if (seed==DEFAULT_COOKIE || seed==0) seed ^= 0x00004711A5B3C0DEULL;
  __security_cookie=seed;
  __security_cookie_complement=~seed;
}
extern "C" void __security_check_cookie(u64 check){
  if (check != __security_cookie) __fastfail(2); // FAST_FAIL_STACK_COOKIE_CHECK_FAILURE
}
