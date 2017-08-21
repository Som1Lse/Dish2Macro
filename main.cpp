#define _CRT_SECURE_NO_WARNINGS

#include <cassert>
#include <cstdio>
#include <cwchar>

#include <windows.h>

HHOOK Hook;
bool ShouldJump = false;
unsigned KeyCode;

bool IsDishonored2InFocus(){
    auto Window = GetForegroundWindow();

    wchar_t ClassName[14];
    if(GetClassNameW(Window,ClassName,14) != 12 || std::wcscmp(ClassName,L"Dishonored 2") != 0){
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

    if(ButtonCode == KeyCode){
        ShouldJump = IsDown;

        if(IsDishonored2InFocus()){
            return 1;//stop propagation
        }
    }

    return CallNextHookEx(Hook,Code,WParam,LParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int Code,WPARAM WParam,LPARAM LParam){
    assert(Code == HC_ACTION);

    auto& Info = *reinterpret_cast<KBDLLHOOKSTRUCT*>(LParam);

    if(Info.vkCode == KeyCode){
        ShouldJump = (WParam == WM_KEYDOWN || WParam == WM_SYSKEYDOWN);

        if(IsDishonored2InFocus()){
            return 1;//stop propagation
        }
    }

    return CallNextHookEx(Hook,Code,WParam,LParam);
}

void SendJump(){
    if(IsDishonored2InFocus()){
        mouse_event(MOUSEEVENTF_WHEEL,0,0,-WHEEL_DELTA,0);
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

    if((KeyCode >= VK_LBUTTON && KeyCode <= VK_RBUTTON) || (KeyCode >= VK_MBUTTON && KeyCode <= VK_XBUTTON2)){
        Hook = SetWindowsHookExW(WH_MOUSE_LL,LowLevelMouseProc,nullptr,0);
    }else{
        Hook = SetWindowsHookExW(WH_KEYBOARD_LL,LowLevelKeyboardProc,nullptr,0);
    }

    if(!Hook){
        std::fprintf(stderr,"Unable to keyboard hook.\nError code: 0x%08lX\n",GetLastError());
        std::getchar();
        return 0;
    }

    for(;;){
        Sleep(5);

        MSG Message;
        while(PeekMessage(&Message,0,0,0,PM_REMOVE)){
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        if(!IsDishonored2InFocus()){
            ShouldJump = false;
        }

        if(ShouldJump){
            SendJump();
        }
    }

    return 0;
}
