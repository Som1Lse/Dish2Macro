#define _CRT_SECURE_NO_WARNINGS

#include <atomic>

#include <cassert>
#include <cstdio>
#include <cwchar>

#include <windows.h>

HHOOK Hook;
std::atomic<bool> ShouldJump = false;
unsigned KeyCode;

bool IsGameInFocus(){
    auto Window = GetForegroundWindow();

    wchar_t ClassName[35];
    auto Size = GetClassNameW(Window,ClassName,35);
    if((Size != 12 || std::wcscmp(ClassName,L"Dishonored 2") != 0) &&
       (Size != 33 || std::wcscmp(ClassName,L"Dishonored: Death of the Outsider") != 0)){
        return false;
    }

    POINT Point;
    if(!GetCursorPos(&Point)){
        abort();
    }

    if(Window != WindowFromPoint(Point)){
        return false;
    }

    return true;
}

LRESULT CALLBACK LowLevelMouseProc(int Code,WPARAM WParam,LPARAM LParam){
    assert(Code == HC_ACTION);

    unsigned ButtonCode = 0;
    bool IsDown = false;
    switch(WParam){
        case WM_LBUTTONDOWN: {
            IsDown = true;
            //[[fallthrough]];
        }
        case WM_LBUTTONUP: {
            ButtonCode = VK_LBUTTON;
            break;
        }
        case WM_RBUTTONDOWN: {
            IsDown = true;
            //[[fallthrough]];
        }
        case WM_RBUTTONUP: {
            ButtonCode = VK_RBUTTON;
            break;
        }
        case WM_MBUTTONDOWN: {
            IsDown = true;
            //[[fallthrough]];
        }
        case WM_MBUTTONUP: {
            ButtonCode = VK_MBUTTON;
            break;
        }
        case WM_XBUTTONDOWN: {
            IsDown = true;
            //[[fallthrough]];
        }
        case WM_XBUTTONUP: {
            auto& Info = *reinterpret_cast<MSLLHOOKSTRUCT*>(LParam);

            ButtonCode = VK_XBUTTON1+HIWORD(Info.mouseData)-XBUTTON1;
            break;
        }
    }

    if(ButtonCode == KeyCode && IsGameInFocus()){
        ShouldJump = IsDown;

        return 1;//stop propagation
    }

    return CallNextHookEx(Hook,Code,WParam,LParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int Code,WPARAM WParam,LPARAM LParam){
    assert(Code == HC_ACTION);

    auto& Info = *reinterpret_cast<KBDLLHOOKSTRUCT*>(LParam);

    if(Info.vkCode == KeyCode && IsGameInFocus()){
        ShouldJump = (WParam == WM_KEYDOWN || WParam == WM_SYSKEYDOWN);

        return 1;//stop propagation
    }

    return CallNextHookEx(Hook,Code,WParam,LParam);
}

void SendJump(){
    mouse_event(MOUSEEVENTF_WHEEL,0,0,-WHEEL_DELTA,0);
}

void CALLBACK TimerProc(void*,BOOLEAN){
    if(!IsGameInFocus()){
        ShouldJump = false;
    }else if(ShouldJump){
        SendJump();
    }
}

int main(){
    auto ConfigFilename = "Dish2Macro.cfg";
    auto File = std::fopen(ConfigFilename,"rb");
    if(!File){
        std::fprintf(stderr,"Unable to open \"%s\".\n",ConfigFilename);
        std::getchar();
        return 0;
    }

    if(std::fscanf(File,"0x%x",&KeyCode) != 1 && std::fscanf(File,"%u",&KeyCode) != 1){
        std::fprintf(stderr,"Unable to read key code from \"%s\".\n",ConfigFilename);
        std::getchar();
        return 0;
    }

    std::fclose(File);

    auto IsMouseButton = (KeyCode >= VK_LBUTTON && KeyCode <= VK_RBUTTON) ||
                         (KeyCode >= VK_MBUTTON && KeyCode <= VK_XBUTTON2);

    if(IsMouseButton){
        Hook = SetWindowsHookExW(WH_MOUSE_LL,LowLevelMouseProc,nullptr,0);
    }else{
        Hook = SetWindowsHookExW(WH_KEYBOARD_LL,LowLevelKeyboardProc,nullptr,0);
    }

    if(!Hook){
        std::fprintf(stderr,"Unable to set hook.\nError code: 0x%08lX\n",GetLastError());
        std::getchar();
        return 0;
    }

    HANDLE Timer;
    if(!CreateTimerQueueTimer(&Timer,nullptr,TimerProc,nullptr,0,5,WT_EXECUTEDEFAULT)){
        std::fprintf(stderr,"Unable to create timer.\nError code: 0x%08lX\n",GetLastError());
        std::getchar();
        return 0;
    }

    for(;;){
        MSG Message;
        while(GetMessageW(&Message,0,0,0)){
            TranslateMessage(&Message);
            DispatchMessageW(&Message);
        }
    }

    return 0;
}
