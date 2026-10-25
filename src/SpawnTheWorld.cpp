// Spawn The World - native Enshrouded DLL (x64)
// Spawn The World v1.0.0 native spawn implementation.
// No CE, no EML edits, no modified spawn timers or distance values.
// Only intended for the exact verified enshrouded.exe build.
// Asynchronous bounded transform analysis; the game-thread hook only captures short contexts and queues prepared commands.

#ifdef _MSC_VER
#pragma warning(disable: 4100)
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed long long i64;
typedef unsigned long DWORD;
typedef unsigned long long SIZE_T;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef int BOOL;
typedef const char* LPCSTR;
#define WINAPI __stdcall
#define API __declspec(dllimport)
#define DLL_EXPORT extern "C" __declspec(dllexport)
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_READ 0x20
#define PAGE_READWRITE 0x04
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define FILE_SHARE_READ 1
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_ATTRIBUTE_NORMAL 0x80
#define INVALID_HANDLE_VALUE ((HANDLE)(-1))
#define VK_F12 0x7B

extern "C" {
API HMODULE WINAPI GetModuleHandleA(LPCSTR);
API DWORD WINAPI GetModuleFileNameA(HMODULE, char*, DWORD);
API DWORD WINAPI GetTempPathA(DWORD, char*);
API BOOL WINAPI VirtualProtect(LPVOID, SIZE_T, DWORD, DWORD*);
API LPVOID WINAPI VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);
API BOOL WINAPI VirtualFree(LPVOID, SIZE_T, DWORD);
API BOOL WINAPI FlushInstructionCache(HANDLE,LPCVOID,SIZE_T);
API HANDLE WINAPI GetCurrentProcess();
API HANDLE WINAPI CreateThread(LPVOID, SIZE_T, DWORD (WINAPI*)(LPVOID), LPVOID, DWORD, DWORD*);
API BOOL WINAPI CloseHandle(HANDLE);
API void WINAPI Sleep(DWORD);
API u64 WINAPI GetTickCount64();
API BOOL WINAPI DisableThreadLibraryCalls(HMODULE);
API HANDLE WINAPI CreateFileA(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
API BOOL WINAPI ReadFile(HANDLE,LPVOID,DWORD,DWORD*,LPVOID);
API SIZE_T WINAPI VirtualQuery(LPCVOID,LPVOID,SIZE_T);
API BOOL WINAPI ReadProcessMemory(HANDLE,LPCVOID,LPVOID,SIZE_T,SIZE_T*);
API BOOL WINAPI WriteFile(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
API short WINAPI GetAsyncKeyState(int);
API BOOL WINAPI QueryPerformanceCounter(i64*);
API BOOL WINAPI QueryPerformanceFrequency(i64*);

}

extern "C" int _fltused = 0;
extern "C" __declspec(noinline) void* memcpy(void* dst,const void* src,SIZE_T n){
  volatile u8* d=(volatile u8*)dst;const volatile u8* s=(const volatile u8*)src;
  for(SIZE_T i=0;i<n;i++)d[i]=s[i];return dst;
}
extern "C" long __cdecl _InterlockedCompareExchange(volatile long*,long,long);
extern "C" long __cdecl _InterlockedExchange(volatile long*,long);
extern "C" long __cdecl _InterlockedIncrement(volatile long*);
#pragma intrinsic(_InterlockedCompareExchange, _InterlockedExchange, _InterlockedIncrement)
static long atomicCAS(volatile long* p,long expected,long desired){return _InterlockedCompareExchange(p,desired,expected);}
static void atomicStore(volatile long* p,long v){_InterlockedExchange(p,v);}
static long atomicIncrement(volatile long* p){return _InterlockedIncrement(p);}
static volatile long running=1;
static volatile long ready=0;
static volatile long activeGeneration=0;
static volatile long spin=0;
static volatile long totalQueued=0;
static u64 deadline=0;
static volatile u8* hookEnabled=0;
static void* hookGateway=0;
static volatile long preparedJobs=0;
static volatile long rejectedJobs=0;
static volatile long maxPreparationMicroseconds=0;
static volatile long capturesWritten=0;
static i64 qpcFrequency=0;
static volatile long hookMaxMicroseconds=0;
static volatile long executionMaxMicroseconds=0;
static volatile long hookActiveCalls=0;
static volatile long staleCmdSkips=0;
static const u32 kMaxHits=4500;
static u32 hookHits=0;
static u32 seenCount=0;
static u64 seen[2048];
static long seenGen=0;
static HMODULE selfModule=0;
static HANDLE logfile=INVALID_HANDLE_VALUE;
static char logPath[512];
static char listPath[512];
static u32 targetCount=0;
struct Target { u8 guid[16]; char name[37]; };
static Target targets[4096];
// Small open-addressed index: avoids comparing every spawned GUID against all
// 865+ whitelist records on the game update thread.
static u16 guidLookup[8192];
// Single-producer (inside spin-locked hook) / single-consumer (worker) log ring.
// No disk IO on the game update thread. An overflow drops DIAGNOSTIC events
// only; it never changes whether a spawn command is executed.
struct HookEvent { u64 dyn; u16 idx; u8 kind; u8 pad[5]; };
static HookEvent hookEvents[8192];
static volatile long eventWrite=0;
static volatile long eventRead=0;
static volatile long lostLogEvents=0;

static const u64 OFFSET_UPDATE=0x322E40ULL;
static const u8 expected[12]={0x40,0x55,0x57,0x41,0x56,0x48,0x81,0xEC,0xB0,0x00,0x00,0x00};
static u8 originalBytes[12];
static void* hookTarget=0;
static void* trampoline=0;
static int hooked=0;
typedef u8 (__fastcall *UpdateFn)(void*,void*);
static UpdateFn originalUpdate=0;

static void copyBytes(void* dst, const void* src, u64 n) {
  u8* a=(u8*)dst;const u8* b=(const u8*)src;
  for(u64 i=0;i<n;i++) a[i]=b[i];
}
static int eqBytes(const void* a,const void* b,u64 n) {
  const u8* x=(const u8*)a;const u8* y=(const u8*)b;
  for(u64 i=0;i<n;i++) if(x[i]!=y[i])return 0;
  return 1;
}
static u32 slen(const char* s) {u32 n=0;while(s && s[n])++n;return n;}
static void cat(char* dst,u32 cap,const char* s) {u32 d=slen(dst),i=0;while(s && s[i] && d+1<cap)dst[d++]=s[i++];dst[d]=0;}
static void hexAppend(char* dst,u32 cap,u64 v) {
  char tmp[17];u32 n=0;do{u32 a=(u32)(v&15);tmp[n++]=(char)(a<10?'0'+a:'A'+a-10);v>>=4;}while(v&&n<16);
  char out[19];u32 j=0;out[j++]='0';out[j++]='x';while(n)out[j++]=tmp[--n];out[j]=0;cat(dst,cap,out);
}
static void decAppend(char* dst,u32 cap,u64 v){char tmp[24];u32 n=0;do{tmp[n++]=(char)('0'+v%10);v/=10;}while(v&&n<24);char out[24];u32 j=0;while(n)out[j++]=tmp[--n];out[j]=0;cat(dst,cap,out);}
static void writeLog(const char* msg){
  if(logfile==INVALID_HANDLE_VALUE)return;
  DWORD n=0;WriteFile(logfile,msg,slen(msg),&n,0);WriteFile(logfile,"\r\n",2,&n,0);
}
static int validPointer(const void* p){u64 n=(u64)p;return n>0x10000 && n<0x7FFFFFFFFFFFULL;}
// On Windows / MSVC, the in-process spawner references are known and live on
// the game update thread. SEH limits damage when a stale pointer is observed.
struct MemRegion {
  LPVOID base;
  LPVOID allocationBase;
  DWORD allocProtect;
  DWORD pad0;
  SIZE_T regionSize;
  DWORD state;
  DWORD protect;
  DWORD type;
  DWORD pad1;
};
static int memRead(void* out, const void* src, u64 n){
  if(!validPointer(src) || n>65536) return 0;
  SIZE_T actual=0;
  // ReadProcessMemory safely rejects stale pointers without dereferencing them
  // in the mod worker. No ReadProcessMemory is performed in the game hook.
  return ReadProcessMemory(GetCurrentProcess(),src,out,(SIZE_T)n,&actual) && actual==n;
}
static int rd8(const void* src,u8* v){return memRead(v,src,1);}
static int rd32(const void* src,u32* v){return memRead(v,src,4);}
static int rd64(const void* src,u64* v){return memRead(v,src,8);}
static const void* at(const void* p,u64 off){return (const void*)((u64)p+off);}
static int parseHex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static int parseGuid(const char* s,u8 out[16]){
  const int order[16]={3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};
  const int loc[16]={0,2,4,6,9,11,14,16,19,21,24,26,28,30,32,34};
  for(int i=0;i<16;i++){
    int pos=loc[order[i]],hi=parseHex(s[pos]),lo=parseHex(s[pos+1]);
    if(hi<0||lo<0)return 0;
    out[i]=(u8)(hi*16+lo);
  }
  return s[8]=='-' && s[13]=='-' && s[18]=='-' && s[23]=='-';
}
static u32 hashGuid(const u8* guid){
  u32 h=2166136261u;
  for(int i=0;i<16;i++){h^=guid[i];h*=16777619u;}
  return h;
}
static void buildGuidLookup(){
  for(u32 i=0;i<targetCount;i++){
    u32 p=hashGuid(targets[i].guid)&8191u;
    while(guidLookup[p])p=(p+1)&8191u;
    guidLookup[p]=(u16)(i+1);
  }
}
static int lookupGuid(const u8* guid){
  u32 p=hashGuid(guid)&8191u;
  for(u32 attempt=0;attempt<8192;attempt++){
    u32 n=guidLookup[p];
    if(!n)return -1;
    if(eqBytes(targets[n-1].guid,guid,16))return (int)n-1;
    p=(p+1)&8191u;
  }
  return -1;
}
static int findGuid(const void* cfg){
  u32 off=0,count=0;
  if(!rd32(cfg,&off)||!rd32(at(cfg,4),&count)||off<168||off>65536||count<1||count>64)return -1;
  for(u32 i=0;i<count;i++){
    const void* entry=at(cfg,(u64)off+(u64)i*36);
    u32 eo=0,n=0;
    if(!rd32(entry,&eo)||!rd32(at(entry,4),&n)||eo<36||eo>65536||n<1||n>32)continue;
    for(u32 j=0;j<n;j++){
      u8 guid[16];if(!memRead(guid,at(entry,(u64)eo+j*16),16))continue;
      int match=lookupGuid(guid);
      if(match>=0)return match;
    }
  }
  return -1;
}
static int validTransform(const u8* raw) {
  float f[8];copyBytes(f,raw,32);
  for(int i=0;i<8;i++)if(f[i]!=f[i]||f[i]<-1000000.f||f[i]>1000000.f)return 0;
  if(f[3]<0.1f||f[3]>20.f)return 0;
  if(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]<100.f)return 0;
  float q=f[4]*f[4]+f[5]*f[5]+f[6]*f[6]+f[7]*f[7];
  return q>=0.97f&&q<=1.03f;
}
static int getTransform(const void* ctx,const void* commands,u8 out[32]){
  if(memRead(out,at(commands,8),32)&&validTransform(out))return 1;
  // Read just ONCE per candidate pointer, rather than 17 kernel calls for
  // every possible delta. The worker does this off the engine/game thread.
  int count=0,x0=0,y0=0,z0=0;
  u8 saved[32];
  for(u32 slot=0;slot<=0x118;slot+=8){
    u64 p=0;
    if(!rd64(at(ctx,slot),&p)||!validPointer((void*)p))continue;
    u8 block[96];
    if(!memRead(block,(const void*)p,96))continue;
    for(u32 delta=0;delta<=0x40;delta+=4){
      const u8* transform=block+delta;
      if(!validTransform(transform))continue;
      float pos[3];copyBytes(pos,transform,12);
      int x=(int)(pos[0]+(pos[0]>=0?0.5f:-0.5f));
      int y=(int)(pos[1]+(pos[1]>=0?0.5f:-0.5f));
      int z=(int)(pos[2]+(pos[2]>=0?0.5f:-0.5f));
      if(count && (x!=x0||y!=y0||z!=z0))return 0;
      x0=x;y0=y;z0=z;copyBytes(saved,transform,32);count++;
    }
  }
  if(!count)return 0;
  copyBytes(out,saved,32);return 1;
}
static int readConfig(void* ctx,void** cfg,void** dyn,void** commands){
  u64 a=0,b=0,c=0;
  if(!rd64(at(ctx,0x08),&a)||!rd64(at(ctx,0x88),&b)||!rd64(at(ctx,0xB8),&c))return 0;
  if(!validPointer((void*)a)||!validPointer((void*)b)||!validPointer((void*)c))return 0;
  *cfg=(void*)a;*dyn=(void*)b;*commands=(void*)c;return 1;
}
static int isSpawnerAllowed(const void* cfg,const void* dyn){
  i64 mn=0,mx=0;u64 next=0;u8 allow=0;
  if(!memRead(&mn,at(cfg,0x20),8)||!memRead(&mx,at(cfg,0x28),8)||!rd64(dyn,&next)||!rd8(at(cfg,0x5D),&allow))return 0;
  return mn>=0&&mx>=mn&&mx<=86400000000000LL&&allow!=0;
}
static void emitResult(const char* outcome,const char* name,u64 dyn){
  char s[256];s[0]=0;cat(s,sizeof(s),outcome);cat(s,sizeof(s)," guid=");cat(s,sizeof(s),name);
  cat(s,sizeof(s)," dyn=");hexAppend(s,sizeof(s),dyn);writeLog(s);
}
// Hook-generated events are written to an in-memory ring, never to disk.
static void recordHookEvent(u8 kind,u32 idx,u64 dyn){
  long w=eventWrite,r=eventRead;
  if((unsigned long)(w-r)>=8192u){atomicIncrement(&lostLogEvents);return;}
  HookEvent *e=&hookEvents[((u32)w)&8191u];
  e->dyn=dyn;e->idx=(u16)idx;e->kind=kind;
  atomicStore(&eventWrite,w+1);
}
static void drainHookEvents(){
  long r=eventRead,w=atomicCAS(&eventWrite,0,0);
  while(r!=w){
    const HookEvent *e=&hookEvents[((u32)r)&8191u];
    const char* msg=e->kind==1?"QUEUED SYNTHETIC":e->kind==2?"SKIP queue busy":"SKIP stale data";
    if(e->idx<targetCount)emitResult(msg,targets[e->idx].name,e->dyn);
    ++r;
  }
  atomicStore(&eventRead,r);
}
static int locked(){return atomicCAS(&spin,0,1)==0;}
static void unlock(){atomicStore(&spin,0);}
static void maxAtomic(volatile long* dst,long v){
  long old=atomicCAS(dst,0,0);
  while(v>old){long observed=atomicCAS(dst,old,v);if(observed==old)break;old=observed;}
}
static long toMicroseconds(i64 elapsed){
  if(elapsed<0||qpcFrequency<=0)return 0;
  i64 n=(elapsed*1000000LL)/qpcFrequency;
  return n>0x7FFFFFFFLL?0x7FFFFFFF:(long)n;
}

// Shared job table: a job is published by the game hook, prepared by the
// worker and finally consumed by the NEXT engine update for that same spawner.
// 0 = unused, 1 = submitted, 4 = computing, 2 = prepared, 3 = used, 5 = skip.
// Only the game hook writes directly into a game spawn command buffer.
struct SpawnJob {
  volatile long state;
  long gen;
  u64 cfg,dyn,commands;
  u8 context[0x120]; // copied while still live as the actual engine argument
  u8 command[48];
  u16 targetIdx;
  u16 pad;
};
static SpawnJob jobs[4096];
static volatile long workerCursor=0;

static u32 jobHash(u64 dyn){
  u64 n=dyn>>4;
  n^=n>>30;n*=0xBF58476D1CE4E5B9ULL;
  n^=n>>27;
  return (u32)(n^(n>>31))&4095u;
}
static SpawnJob* findJob(u64 dyn,long gen){
  u32 base=jobHash(dyn);
  for(u32 probe=0;probe<512;probe++){
    SpawnJob* j=&jobs[(base+probe)&4095u];
    long state=atomicCAS(&j->state,0,0);
    if(j->dyn==dyn && j->gen==gen && state!=0)return j;
    if(state==0 || (j->gen!=gen && state!=4))return j;
  }
  return 0;
}
static void processOneJob(SpawnJob* j){
  i64 prepStart=0,prepEnd=0;
  if(qpcFrequency>0)QueryPerformanceCounter(&prepStart);
  int idx=findGuid((void*)j->cfg);
  if(idx<0 || !isSpawnerAllowed((void*)j->cfg,(void*)j->dyn)){
    atomicStore(&j->state,5);return;
  }
  u32 count=0;
  if(!rd32(at((void*)j->cfg,4),&count)||count==0||count>64){
    atomicStore(&j->state,5);return;
  }
  u8 transform[32];
  if(!getTransform(j->context,(void*)j->commands,transform)){
    // No unverified transform is ever written into the engine queue.
    atomicIncrement(&rejectedJobs);
    atomicStore(&j->state,5);return;
  }
  for(u32 i=0;i<48;i++)j->command[i]=0;
  j->command[0]=1;
  copyBytes(j->command+8,transform,32);
  j->targetIdx=(u16)idx;
  atomicIncrement(&preparedJobs);
  if(qpcFrequency>0){QueryPerformanceCounter(&prepEnd);
    maxAtomic(&maxPreparationMicroseconds,toMicroseconds(prepEnd-prepStart));}
  atomicStore(&j->state,2);
}
static void prepareJobs(){
  // A bounded off-thread batch ensures the worker also services input and logs.
  u32 cursor=(u32)atomicCAS(&workerCursor,0,0);
  for(u32 i=0;i<512;i++){
    SpawnJob* j=&jobs[(cursor+i)&4095u];
    if(atomicCAS(&j->state,1,4)==1)processOneJob(j);
  }
  atomicStore(&workerCursor,(long)((cursor+512)&4095));
}
static DWORD WINAPI preparationThread(LPVOID){
  while(running){prepareJobs();Sleep(1);}
  return 0;
}
static u8 __fastcall hookUpdate(void* first, void* ctx){
  // NO VirtualQuery / file IO / GUID scan / transform scan / heap allocation.
  // Nothing heavyweight runs on the intercepted game thread.
  int injected=0;
  i64 before=0,after=0;
  long gen=atomicCAS(&activeGeneration,0,0);
  if(gen>0 && running && ctx && locked()){
    if(qpcFrequency>0)QueryPerformanceCounter(&before);
    atomicIncrement(&hookActiveCalls);
    if(GetTickCount64()<=deadline){
      u64 cfg=*(const u64*)((const u8*)ctx+0x08);
      u64 dyn=*(const u64*)((const u8*)ctx+0x88);
      u64 commands=*(const u64*)((const u8*)ctx+0xB8);
      if(validPointer((void*)cfg)&&validPointer((void*)dyn)&&validPointer((void*)commands)){
        SpawnJob* j=findJob(dyn,gen);
        if(j){
          long st=atomicCAS(&j->state,0,0);
          if(st==2 && j->gen==gen && j->cfg==cfg && j->dyn==dyn && j->commands==commands){
            u8 count=*((volatile u8*)(commands+0x240));
            if(count==0 && atomicCAS(&j->state,2,3)==2){
              copyBytes((void*)commands,j->command,48);
              *((volatile u8*)(commands+0x240))=1;
              atomicIncrement(&totalQueued);
              recordHookEvent(1,j->targetIdx,dyn);
              injected=1;
            }
          }else if(st==0 || (j->gen!=gen && st!=4)){
            j->cfg=cfg;j->dyn=dyn;j->commands=commands;j->gen=gen;
            copyBytes(j->context,ctx,0x120);
            atomicIncrement(&capturesWritten);
            atomicStore(&j->state,1);
          }
        }
      }
    }
    if(qpcFrequency>0){
      QueryPerformanceCounter(&after);
      maxAtomic(&hookMaxMicroseconds,toMicroseconds(after-before));
    }
    unlock();
  }
  // The original engine call happens on its own thread, outside our lock.
  return originalUpdate(first,ctx);
}

static void absoluteJump(u8* loc,void* dest){
  loc[0]=0x48;loc[1]=0xB8;u64 addr=(u64)dest;copyBytes(loc+2,&addr,8);loc[10]=0xFF;loc[11]=0xE0;
}
static void indirectJump(u8* atAddress,void* destination){
  // jmp qword ptr [rip+0]; <8-byte target>; does NOT clobber RAX.
  atAddress[0]=0xFF;atAddress[1]=0x25;
  atAddress[2]=0;atAddress[3]=0;atAddress[4]=0;atAddress[5]=0;
  u64 p=(u64)destination;copyBytes(atAddress+6,&p,8);
}
static int installHook(void* addr){
  if(!eqBytes(addr,expected,12))return 0;
  hookTarget=addr;
  copyBytes(originalBytes,addr,12);
  // Prepare trampoline writable but NOT executable. Mark RX once complete.
  u8* tramp=(u8*)VirtualAlloc(0,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
  if(!tramp)return 0;
  copyBytes(tramp,originalBytes,12);
  indirectJump(tramp+12,(void*)((u64)addr+12));
  DWORD trampOld=0;
  if(!VirtualProtect(tramp,4096,PAGE_EXECUTE_READ,&trampOld)){
    VirtualFree(tramp,0,MEM_RELEASE);return 0;
  }
  FlushInstructionCache(GetCurrentProcess(),tramp,26);
  trampoline=tramp;originalUpdate=(UpdateFn)tramp;

  // Assembly gateway:  80 3D disp32 00    cmp byte ptr [rip+disp],0
  //                    74 0E              je fast_path
  //                    FF 25 00000000     jmp [rip+0]  (detour ptr)
  //                    <8 byte hook ptr>
  // fast_path:         FF 25 00000000     jmp [rip+0]  (trampoline ptr)
  //                    <8 byte trampoline ptr>
  // flag:              db 0
  // Inactive => engine prologue runs without any C++ callback and without
  // touching any caller registers (except flags, overwritten by SUB RSP).
  // Two separate pages: executable gateway code (RW during assembly, then RX)
  // and an always-writable enable byte (RW).  Never write into RX code pages.
  u8* gw=(u8*)VirtualAlloc(0,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
  if(!gw)return 0;
  // cmp byte ptr [RIP + 0xFF9], 0 ; RIP after instruction = gw+7
  // gw+7+0xFF9 = gw+4096 (control flag on SECOND, writable page).
  // Seven bytes = 80 3D F9 0F 00 00 00 (including IMM8=00).
  const u8 stub[38]={
    0x80,0x3D,0xF9,0x0F,0x00,0x00,0x00,0x74,0x0E,
    0xFF,0x25,0,0,0,0, 0,0,0,0,0,0,0,0,
    0xFF,0x25,0,0,0,0, 0,0,0,0,0,0,0,0, 0
  };
  copyBytes(gw,stub,sizeof(stub));
  u64 detour=(u64)&hookUpdate,orig=(u64)tramp;
  copyBytes(gw+15,&detour,8);copyBytes(gw+29,&orig,8);
  hookGateway=gw;hookEnabled=(volatile u8*)(gw+4096);
  DWORD gatewayOld=0;
  if(!VirtualProtect(gw,4096,PAGE_EXECUTE_READ,&gatewayOld)){
    hookGateway=0;hookEnabled=0;VirtualFree(gw,0,MEM_RELEASE);return 0;
  }
  FlushInstructionCache(GetCurrentProcess(),gw,38);

  DWORD old=0;
  if(!VirtualProtect(addr,12,PAGE_EXECUTE_READWRITE,&old))return 0;
  u8 replacement[12];absoluteJump(replacement,gw);
  copyBytes(addr,replacement,12);
  FlushInstructionCache(GetCurrentProcess(),addr,12);
  DWORD unused=0;VirtualProtect(addr,12,old,&unused);
  hooked=1;return 1;
}
static void removeHook(){
  if(!hooked)return;
  DWORD old=0;
  if(VirtualProtect(hookTarget,12,PAGE_EXECUTE_READWRITE,&old)){
    copyBytes(hookTarget,originalBytes,12);
    FlushInstructionCache(GetCurrentProcess(),hookTarget,12);
    DWORD unused=0;VirtualProtect(hookTarget,12,old,&unused);
  }
  hooked=0;
  // Do NOT free a trampoline that a game thread might still be executing.
}
// Embedded enemy GUID table used by the release build.
extern "C" const char stwEmbeddedGuids[];
extern "C" const unsigned int stwEmbeddedGuidsLength;
static u32 loadEmbeddedGuids(){
  targetCount=0;
  u32 size=stwEmbeddedGuidsLength, pos=0;
  const char* buf=stwEmbeddedGuids;
  while(pos<size && targetCount<4096){
    u32 begin=pos;
    while(pos<size&&buf[pos]!='\n'&&buf[pos]!='\r')pos++;
    u32 len=pos-begin;
    if(len==36){
      char line[37];copyBytes(line,buf+begin,36);line[36]=0;
      u8 gid[16];
      if(parseGuid(line,gid)){
        int exists=0;
        for(u32 i=0;i<targetCount;i++)if(eqBytes(targets[i].guid,gid,16)){exists=1;break;}
        if(!exists){copyBytes(targets[targetCount].guid,gid,16);copyBytes(targets[targetCount].name,line,37);targetCount++;}
      }
    }
    while(pos<size&&(buf[pos]=='\n'||buf[pos]=='\r'))pos++;
  }
  buildGuidLookup();
  return targetCount;
}
static u32 loadGuids(const char* filename){
  HANDLE h=CreateFileA(filename,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(h==INVALID_HANDLE_VALUE)return loadEmbeddedGuids();
  const u32 maxBytes=4096*48;
  char* buf=(char*)VirtualAlloc(0,maxBytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
  if(!buf){CloseHandle(h);return loadEmbeddedGuids();}
  DWORD size=0;int success=ReadFile(h,buf,maxBytes-1,&size,0);
  CloseHandle(h);
  if(!success){VirtualFree(buf,0,MEM_RELEASE);return loadEmbeddedGuids();}
  buf[size]=0;targetCount=0;
  u32 pos=0;
  while(pos<size&&targetCount<4096){
    u32 begin=pos;while(pos<size&&buf[pos]!='\n'&&buf[pos]!='\r')pos++;
    char line[128];u32 len=pos-begin;
    if(len==36){copyBytes(line,buf+begin,36);line[36]=0;
      u8 gid[16];if(parseGuid(line,gid)){
        int exists=0;for(u32 i=0;i<targetCount;i++)if(eqBytes(targets[i].guid,gid,16)){exists=1;break;}
        if(!exists){copyBytes(targets[targetCount].guid,gid,16);copyBytes(targets[targetCount].name,line,37);targetCount++;}
      }
    }
    while(pos<size&&(buf[pos]=='\n'||buf[pos]=='\r'))pos++;
  }
  VirtualFree(buf,0,MEM_RELEASE);
  if(!targetCount) return loadEmbeddedGuids();
  buildGuidLookup();
  return targetCount;
}
static DWORD WINAPI worker(LPVOID){
  char fullpath[512];fullpath[0]=0;
  GetModuleFileNameA(selfModule,fullpath,sizeof(fullpath));
  u32 n=slen(fullpath),cut=0;for(u32 i=0;i<n;i++)if(fullpath[i]=='\\'||fullpath[i]=='/')cut=i+1;
  listPath[0]=0;for(u32 i=0;i<cut && i<sizeof(listPath)-1;i++)listPath[i]=fullpath[i];listPath[cut]=0;
  cat(listPath,sizeof(listPath),"enemies.txt");
  char temp[512];temp[0]=0;GetTempPathA(sizeof(temp),temp);
  logPath[0]=0;cat(logPath,sizeof(logPath),temp);cat(logPath,sizeof(logPath),"OneButtonSpawner-native.log");
  logfile=CreateFileA(logPath,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  writeLog("Spawn The World v1.0.0 -- bounded game hook, worker transform analysis");
  u32 loaded=loadGuids(listPath);
  char s[256];s[0]=0;cat(s,sizeof(s),"Enemy GUIDs loaded: ");decAppend(s,sizeof(s),loaded);writeLog(s);
  if(loaded==0){writeLog("ERROR: no enemy GUIDs available, nothing hooked");return 0;}
  // Extra delay allows the main game startup to complete.
  for(int j=0;j<100 && running;j++)Sleep(20);
  void* module=GetModuleHandleA("enshrouded.exe");
  if(!module){writeLog("ERROR: enshrouded.exe not found");return 0;}
  void* entry=(void*)((u64)module+OFFSET_UPDATE);
  if(!eqBytes(entry,expected,12)){
    writeLog("ERROR: Unknown EXE version, hook refused; no memory modified");return 0;
  }
  if(!installHook(entry)){writeLog("ERROR: Unable to install native hook");return 0;}
  QueryPerformanceFrequency(&qpcFrequency);
  HANDLE pThread=CreateThread(0,0,preparationThread,0,0,0);
  if(!pThread){writeLog("ERROR: preparation worker failed; F12 disabled");
    if(hookEnabled)*hookEnabled=0;return 0;}
  CloseHandle(pThread);
  writeLog("HOOK READY: F12: context snapshot on engine thread, GUID/transform on worker, command injected on next update");
  atomicStore(&ready,1);
  int wasDown=0;
  int capturing=0;
  long totalAtCaptureStart=0;
  u64 stopAt=0;
  long capturedStart=0;
  while(running){
    drainHookEvents();
    u64 now=GetTickCount64();
    int completedThisPoll=0;
    if(capturing && now>=stopAt){
      // Only the gateway flag changes. Native detour stays installed but the
      // game takes its direct jump to its original code when idle.
      *hookEnabled=0;
      capturing=0;
      completedThisPoll=1;
      // Work already in progress (rare) may finish while gate switches.
      // Do not start any new capture during the same polling iteration.
      char msg[256];msg[0]=0;
      cat(msg,sizeof(msg),"CAPTURE FINISHED generation=");decAppend(msg,sizeof(msg),(u64)capturedStart);
      cat(msg,sizeof(msg)," queued-this-press=");decAppend(msg,sizeof(msg),(u64)(atomicCAS(&totalQueued,0,0)-totalAtCaptureStart));
      cat(msg,sizeof(msg)," max-hook-us=");decAppend(msg,sizeof(msg),(u64)atomicCAS(&hookMaxMicroseconds,0,0));
      cat(msg,sizeof(msg)," checked-calls=");decAppend(msg,sizeof(msg),(u64)atomicCAS(&hookActiveCalls,0,0));
      cat(msg,sizeof(msg)," snapshotted=");decAppend(msg,sizeof(msg),(u64)atomicCAS(&capturesWritten,0,0));
      cat(msg,sizeof(msg)," prepared=");decAppend(msg,sizeof(msg),(u64)atomicCAS(&preparedJobs,0,0));
      cat(msg,sizeof(msg)," rejected=");decAppend(msg,sizeof(msg),(u64)atomicCAS(&rejectedJobs,0,0));
      cat(msg,sizeof(msg)," max-prep-us=");decAppend(msg,sizeof(msg),(u64)atomicCAS(&maxPreparationMicroseconds,0,0));
      writeLog(msg);
    }
    int down=(GetAsyncKeyState(VK_F12)&0x8000)!=0;
    if(down&&!wasDown){
      if(capturing || completedThisPoll){
        writeLog("F12 ignored: previous capture was active or just finished");
      }else{
        // Reset diagnostics and make the window visible to the hook only
        // AFTER publishing the new generation. This removes overlap.
        atomicStore(&hookMaxMicroseconds,0);
        atomicStore(&executionMaxMicroseconds,0);
        atomicStore(&hookActiveCalls,0);
        atomicStore(&capturesWritten,0);
        atomicStore(&preparedJobs,0);
        atomicStore(&rejectedJobs,0);
        atomicStore(&maxPreparationMicroseconds,0);
        totalAtCaptureStart=atomicCAS(&totalQueued,0,0);
        capturedStart=atomicIncrement(&activeGeneration);
        deadline=now+750;
        stopAt=deadline;
        capturing=1;
        *hookEnabled=1;
        char msg[128];msg[0]=0;
        cat(msg,sizeof(msg),"F12 accepted, capture generation=");decAppend(msg,sizeof(msg),(u64)capturedStart);
        writeLog(msg);
      }
    }
    wasDown=down;
    Sleep(20);
  }
  if(hookEnabled)*hookEnabled=0;
  drainHookEvents();
  return 0;
}
extern "C" void stwInitializeSecurityCookie();
DLL_EXPORT u32 __stdcall OBS_GetVersion(){return 0x00040000u;}
extern "C" BOOL WINAPI DllMain(HMODULE m, DWORD reason, LPVOID){
  if(reason==DLL_PROCESS_ATTACH){
    // Initialise /GS cookie before ANY C++ worker thread starts.
    stwInitializeSecurityCookie();
    selfModule=m;DisableThreadLibraryCalls(m);
    running=1;
    HANDLE t=CreateThread(0,0,worker,0,0,0);
    if(t)CloseHandle(t);
  }else if(reason==DLL_PROCESS_DETACH){
    running=0;
    if(hookEnabled)*hookEnabled=0;
    // Usually process shutdown; do not invoke complex cleanup from loader lock.
  }
  return 1;
}
