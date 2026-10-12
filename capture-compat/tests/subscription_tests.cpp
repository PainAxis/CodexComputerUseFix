// Include the implementation to exercise its actual hook entry points and registry.
#include "../src/compat.cpp"
#include <thread>
#include <cstdlib>
using namespace capture_compat;
void Check(bool ok,const char* text) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",text); std::exit(1); } }
std::atomic<int> destroyed{0};
class Handler final : public FrameHandler, public IAgileObject {
    std::atomic<ULONG> refs{1};
public:
    ~Handler() { ++destroyed; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if (!out) return E_POINTER; *out=nullptr;
        if (id==__uuidof(IUnknown)||id==__uuidof(FrameHandler)) *out=static_cast<FrameHandler*>(this);
        else if(id==__uuidof(IAgileObject)) *out=static_cast<IAgileObject*>(this);
        else return E_NOINTERFACE; AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override {auto n=--refs;if(!n) delete this;return n;}
    HRESULT STDMETHODCALLTYPE Invoke(capture::IDirect3D11CaptureFramePool*,IInspectable*) override {return S_OK;}
};
class Pool final : public capture::IDirect3D11CaptureFramePool, public ABI::Windows::Foundation::IClosable {
    std::atomic<ULONG> refs{1};
    std::atomic<FrameHandler*> held{nullptr};
public:
    HANDLE entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    HANDLE resume=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    bool block=false, fail=false;
    std::atomic<int> removes{0};
    ~Pool() {CloseHandle(entered);CloseHandle(resume); auto h=held.exchange(nullptr);if(h)h->Release();}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(IInspectable)||id==__uuidof(capture::IDirect3D11CaptureFramePool)) *out=static_cast<capture::IDirect3D11CaptureFramePool*>(this);
        else if(id==__uuidof(ABI::Windows::Foundation::IClosable)) *out=static_cast<ABI::Windows::Foundation::IClosable*>(this);
        else return E_NOINTERFACE;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetIids(ULONG* n,IID** v) override{*n=0;*v=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* s) override{*s=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* t) override{*t=BaseTrust;return S_OK;}
    HRESULT STDMETHODCALLTYPE Recreate(ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice*,ABI::Windows::Graphics::DirectX::DirectXPixelFormat,INT32,ABI::Windows::Graphics::SizeInt32) override{return S_OK;}
    HRESULT STDMETHODCALLTYPE TryGetNextFrame(capture::IDirect3D11CaptureFrame** f) override{*f=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE add_FrameArrived(FrameHandler* h,EventRegistrationToken* t) override {
        if(fail)return E_FAIL; h->AddRef();held=h;t->value=7;SetEvent(entered);
        if(block)Check(WaitForSingleObject(resume,5000)==WAIT_OBJECT_0,"resume native registration");
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE remove_FrameArrived(EventRegistrationToken) override {++removes;auto h=held.exchange(nullptr);if(h)h->Release();return S_OK;}
    HRESULT STDMETHODCALLTYPE CreateCaptureSession(capture::IGraphicsCaptureItem*,capture::IGraphicsCaptureSession**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_DispatcherQueue(ABI::Windows::System::IDispatcherQueue** q) override{*q=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE Close() override {auto h=held.exchange(nullptr);if(h)h->Release();return S_OK;}
};
void Empty() {LONG live,calls,errors;GetDispatchStatus(live,calls,errors);Check(live==0,"all wrappers released");Check(subscriptions==nullptr,"registry empty");Check(closing_pools==nullptr,"closing registry empty");}
int main() {
    InterlockedExchange(&deferred_capture,1);
    for(int iteration=0;iteration<100;++iteration) {
        auto p=new Pool; auto b=static_cast<capture::IDirect3D11CaptureFramePool*>(p);auto c=static_cast<ABI::Windows::Foundation::IClosable*>(p);
        Check(PatchSlot(b,8,reinterpret_cast<void*>(&AddFrame)),"hook add");
        Check(PatchSlot(b,9,reinterpret_cast<void*>(&RemoveFrame)),"hook remove");
        Check(PatchSlot(c,6,reinterpret_cast<void*>(&ClosePool)),"hook close");
        p->block=true;auto h=new Handler;EventRegistrationToken t{};HRESULT result=E_UNEXPECTED;
        std::thread registering([&]{result=AddFrame(b,h,&t);});
        Check(WaitForSingleObject(p->entered,5000)==WAIT_OBJECT_0,"native registration entered");
        Check(ClosePool(c)==S_OK,"close during pending registration");
        SetEvent(p->resume);registering.join();
        Check(result==RO_E_CLOSED,"racing registration reports closed");Check(p->removes==1,"late native token removed");
        h->Release();b->Release();Empty();
    }
    for(bool fail : {false,true}) {
        auto p=new Pool;p->fail=fail;auto b=static_cast<capture::IDirect3D11CaptureFramePool*>(p);auto h=new Handler;EventRegistrationToken t{};
        Check(AddFrame(b,h,&t)==(fail?E_FAIL:S_OK),"normal/failed registration result");
        if(!fail)Check(RemoveFrame(b,t)==S_OK,"normal unregister");
        h->Release();b->Release();Empty();
    }
    Check(destroyed==102,"all original handlers destroyed exactly once");
    std::puts("PASS: 100 deterministic AddFrame/Close races, registration failure, normal unregister, lifetime cleanup");
}
