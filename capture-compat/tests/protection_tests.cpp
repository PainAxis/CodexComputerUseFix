#include "../src/page_protection.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using namespace capture_compat;
int calls=0, failures=0, succeed_at=0;
BOOL WINAPI MockProtect(LPVOID,SIZE_T,DWORD,PDWORD) { return ++calls >= succeed_at; }
void MockFatal() { ++failures; }
void Check(bool v) { if (!v) std::abort(); }
int main() {
    for (int success : {1,2,3,4}) {
        calls=failures=0; succeed_at=success;
        bool result=RestorePageProtection(nullptr,1,PAGE_READONLY,&MockProtect,&MockFatal);
        Check(result==(success<=3)); Check(calls==(success<=3?success:3)); Check(failures==(success>3?1:0));
    }
    void* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    Check(page!=nullptr);
    Check(RestorePageProtection(page,4096,PAGE_READONLY));
    MEMORY_BASIC_INFORMATION state{}; Check(VirtualQuery(page,&state,sizeof(state))!=0);
    Check(state.Protect==PAGE_READONLY); Check(VirtualFree(page,0,MEM_RELEASE)!=0);
    std::puts("PASS: protection restoration, transient retries, persistent-failure handler, real page permissions");
}

