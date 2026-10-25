// Spawn The World v1.0.0 single-DLL Windows export forwarding.
// Core DllMain is in SpawnTheWorld.cpp and is the ONLY DllMain.
// We do not LoadLibrary on a separate mod DLL. Every Windows export is
// forwarded to an absolute path under System32, avoiding self-recursion.
#if defined(OBS_SINGLE_DINPUT8) == defined(OBS_SINGLE_VERSION)
#error Select one Windows proxy DLL type
#endif
typedef unsigned short WCHAR;
typedef unsigned long DWORD;
typedef void* HMODULE;
typedef const WCHAR* LPCWSTR;
typedef const char* LPCSTR;
#define WINAPI __stdcall
#define IMPORT __declspec(dllimport)
extern "C" {
IMPORT DWORD WINAPI GetSystemDirectoryW(WCHAR*,DWORD);
IMPORT HMODULE WINAPI LoadLibraryW(LPCWSTR);
IMPORT void* WINAPI GetProcAddress(HMODULE,LPCSTR);
void* __cdecl _InterlockedCompareExchangePointer(void* volatile*,void*,void*);
}
#pragma intrinsic(_InterlockedCompareExchangePointer)
#ifdef OBS_SINGLE_DINPUT8
static const WCHAR kWinDll[]={'d','i','n','p','u','t','8','.','d','l','l',0};
static const char* const kFunctions[]={"DirectInput8Create", "DllCanUnloadNow", "DllGetClassObject", "DllRegisterServer", "DllUnregisterServer", "GetdfDIJoystick"};
#else
static const WCHAR kWinDll[]={'v','e','r','s','i','o','n','.','d','l','l',0};
static const char* const kFunctions[]={"GetFileVersionInfoA", "GetFileVersionInfoByHandle", "GetFileVersionInfoExA", "GetFileVersionInfoExW", "GetFileVersionInfoSizeA", "GetFileVersionInfoSizeExA", "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW", "GetFileVersionInfoW", "VerFindFileA", "VerFindFileW", "VerInstallFileA", "VerInstallFileW", "VerLanguageNameA", "VerLanguageNameW", "VerQueryValueA", "VerQueryValueW"};
#endif
extern "C" __declspec(noinline) void* memset(void* dest,int value,unsigned long long count){
  volatile unsigned char* p=(volatile unsigned char*)dest;
  for(unsigned long long i=0;i<count;i++)p[i]=(unsigned char)value;
  return dest;
}
static void* volatile g_systemDll=0;
static HMODULE findSystemDll(){
  if(g_systemDll)return (HMODULE)g_systemDll;
  WCHAR path[1024]={0};
  DWORD n=GetSystemDirectoryW(path,1024);
  if(!n||n+25>=1024)return 0;
  if(n&&path[n-1]!='\\'&&path[n-1]!='/')path[n++]='\\';
  for(unsigned i=0;kWinDll[i];i++)path[n++]=kWinDll[i];
  path[n]=0;
  HMODULE real=LoadLibraryW(path);
  if(!real)return 0;
  void* old=_InterlockedCompareExchangePointer(&g_systemDll,real,0);
  return old?(HMODULE)old:real;
}
extern "C" __declspec(noinline) void* __cdecl ProxyResolve(unsigned exportNumber){
  if(exportNumber>=sizeof(kFunctions)/sizeof(kFunctions[0]))return 0;
  HMODULE real=findSystemDll();
  return real?GetProcAddress(real,kFunctions[exportNumber]):0;
}
