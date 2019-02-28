#include <atomic>
#include <string>
#include <string_view>
#include <system_error>

#include <cassert>
#include <cstdio>
#include <cwchar>

#include <windows.h>

using namespace std::literals;

// Global variables are necessary, because Windows hooks have a terrible API.
HHOOK Hook;
std::atomic<bool> ShouldJump(false);

DWORD KeyCode = VK_SPACE;

DWORD WheelDelta = -WHEEL_DELTA;
DWORD WheelEvent = MOUSEEVENTF_WHEEL;

bool IsGameInFocus(){
    auto Window = GetForegroundWindow();

    constexpr int BufferSize = 35;
    wchar_t Buffer[BufferSize];

    std::wstring_view Str = {Buffer,static_cast<std::size_t>(GetClassNameW(Window,Buffer,BufferSize))};
    if(Str == L"LaunchUnrealUWindowsClient"){
        Str = {Buffer,static_cast<std::size_t>(GetWindowTextW(Window,Buffer,BufferSize))};

        if(Str != L"Dishonored"){
            return false;
        }
    }else if(Str != L"Dishonored 2" && Str != L"Dishonored: Death of the Outsider"){
        return false;
    }

    POINT Point;

    [[maybe_unused]]
    auto r = GetCursorPos(&Point);
    assert(r);

    if(Window != WindowFromPoint(Point)){
        return false;
    }

    return true;
}

LRESULT CALLBACK LowLevelMouseProc(int Code,WPARAM WParam,LPARAM LParam){
    if(Code < HC_ACTION){
        return CallNextHookEx(Hook,Code,WParam,LParam);
    }

    assert(Code == HC_ACTION);

    unsigned ButtonCode = 0;
    bool IsDown = false;
    switch(WParam){
        case WM_LBUTTONDOWN: {
            IsDown = true;

            [[fallthrough]];
        }
        case WM_LBUTTONUP: {
            ButtonCode = VK_LBUTTON;
            break;
        }
        case WM_RBUTTONDOWN: {
            IsDown = true;

            [[fallthrough]];
        }
        case WM_RBUTTONUP: {
            ButtonCode = VK_RBUTTON;
            break;
        }
        case WM_MBUTTONDOWN: {
            IsDown = true;

            [[fallthrough]];
        }
        case WM_MBUTTONUP: {
            ButtonCode = VK_MBUTTON;
            break;
        }
        case WM_XBUTTONDOWN: {
            IsDown = true;

            [[fallthrough]];
        }
        case WM_XBUTTONUP: {
            auto& Info = *reinterpret_cast<MSLLHOOKSTRUCT*>(LParam);

            ButtonCode = VK_XBUTTON1+HIWORD(Info.mouseData)-XBUTTON1;
            break;
        }
    }

    if(ButtonCode == KeyCode && IsGameInFocus()){
        ShouldJump = IsDown;

        return 1;// Stop propagation.
    }

    return CallNextHookEx(Hook,Code,WParam,LParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int Code,WPARAM WParam,LPARAM LParam){
    if(Code < HC_ACTION){
        return CallNextHookEx(Hook,Code,WParam,LParam);
    }

    assert(Code == HC_ACTION);

    auto& Info = *reinterpret_cast<KBDLLHOOKSTRUCT*>(LParam);

    auto IsDown = (WParam == WM_KEYDOWN || WParam == WM_SYSKEYDOWN);

    if(Info.vkCode == KeyCode && IsGameInFocus()){
        ShouldJump = IsDown;

        return 1;// Stop propagation.
    }

    return CallNextHookEx(Hook,Code,WParam,LParam);
}

void SendJump(){
    mouse_event(WheelEvent,0,0,WheelDelta,0);
}

void CALLBACK TimerProc(void*,BOOLEAN){
    if(!ShouldJump){
        return;
    }

    if(!IsGameInFocus()){
        ShouldJump = false;

        return;
    }

    SendJump();
}

struct file_deleter {
    void operator()(std::FILE* File) const noexcept {
        std::fclose(File);
    }
};

using unique_file = std::unique_ptr<std::FILE,file_deleter>;

void ReadConfiguration(const char* Filename){
    unique_file File(std::fopen(Filename,"rb"));
    if(!File){
        throw std::runtime_error("Unable to open configuration file \""s+Filename+"\".");
    }

    char WheelDir = 'd';

    if(std::fscanf(File.get(),"0x%x %c",&KeyCode,&WheelDir) < 1 && std::fscanf(File.get(),"%u %c",&KeyCode,&WheelDir) < 1){
        throw std::runtime_error("Unable to read key code from configuration file \""s+Filename+"\".");
    }

    // `|32` convers ASCII letters to lower case.
    // None of the Dishonored games support left or right scrolling, so there is no point to supporting them here.
    switch(WheelDir|32){
        case 'u': WheelDelta =  WHEEL_DELTA; break;
        case 'd': WheelDelta = -WHEEL_DELTA; break;
        default: throw std::runtime_error("Invalid wheel direction in configuration file"s+Filename+"\".");
    }
}

int main(int argc,char** argv){
    try {
        auto ConfigFilename = "Dish2Macro.cfg";

        if(argc >= 2){
            ConfigFilename = argv[1];
        }

        ReadConfiguration(ConfigFilename);

        auto IsMouseButton =
            (KeyCode >= VK_LBUTTON && KeyCode <= VK_RBUTTON) ||
            (KeyCode >= VK_MBUTTON && KeyCode <= VK_XBUTTON2);

        if(IsMouseButton){
            Hook = SetWindowsHookExW(WH_MOUSE_LL,LowLevelMouseProc,nullptr,0);
        }else{
            Hook = SetWindowsHookExW(WH_KEYBOARD_LL,LowLevelKeyboardProc,nullptr,0);
        }

        if(!Hook){
            throw std::system_error(GetLastError(),std::system_category());
        }

        // Execute every 5 milliseconds, 200 times a second.
        DWORD Period = 5;

        HANDLE Timer;
        if(!CreateTimerQueueTimer(&Timer,nullptr,TimerProc,nullptr,0,Period,WT_EXECUTEDEFAULT)){
            throw std::system_error(GetLastError(),std::system_category());
        }

        for(;;){
            MSG Message;
            while(GetMessageW(&Message,0,0,0)){
                TranslateMessage(&Message);
                DispatchMessageW(&Message);
            }
        }
    }catch(std::exception& e){
        std::fprintf(stderr,"Error: %s\n",e.what());
        std::getchar();

        return 1;
    }

    return 0;
}
