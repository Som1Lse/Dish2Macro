#define _CRT_SECURE_NO_WARNINGS

#include <cassert>
#include <cstdio>
#include <cwchar>

#include <windows.h>

HHOOK KeyboardHook;
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

LRESULT CALLBACK LowLevelKeyboardProc(int Code,WPARAM WParam,LPARAM LParam){
    assert(Code == HC_ACTION);

    KBDLLHOOKSTRUCT& Info = *reinterpret_cast<KBDLLHOOKSTRUCT*>(LParam);

    if(Info.vkCode == KeyCode){
        ShouldJump = (WParam == WM_KEYDOWN || WParam == WM_SYSKEYDOWN);

        if(IsDishonored2InFocus()){
            return 1;//stop propagation
        }
    }

    return CallNextHookEx(KeyboardHook,Code,WParam,LParam);
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

    if(std::fscanf(File,"%u",&KeyCode) != 1){
        std::fprintf(stderr,"Unable to read key code from \"%s\".\n",ConfigFilename);
        std::getchar();
        return 0;
    }

    std::fclose(File);

    KeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL,LowLevelKeyboardProc,nullptr,0);
    if(!KeyboardHook){
        std::fprintf(stderr,"Unable to install keyboard hook.\nError code: 0x%08lX\n",GetLastError());
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
