#include "ini.h"

#include <atomic>
#include <charconv>
#include <string_view>
#include <system_error>

#include <iostream>

#include <cassert>

#include <windows.h>

namespace {

// Global variables are necessary, because Windows hooks have a terrible API.
HHOOK MouseHook;
HHOOK KeyboardHook;

constexpr unsigned SpamDownBit = 0x1;
constexpr unsigned SpamUpBit   = 0x2;
std::atomic<unsigned> SpamFlags(0);

DWORD Interval = 5;

DWORD DownKeyCode = 0;
DWORD UpKeyCode   = 0;

template <typename T>
inline auto ParseInt(std::string_view Value,T* r,const char* Message){
    auto Begin = Value.data();
    auto End = Begin+Value.size();

    int Base = 10;

    if(Begin[0] == '0' && (Begin[1]|32) == 'x'){
        Begin += 2;
        Base = 16;
    }

    auto [Ptr,Error] = std::from_chars(Begin,End,*r,Base);
    if(Error != std::errc()){
        throw std::system_error(std::make_error_code(Error),Message);
    }

    return Value.begin()+(Ptr-Value.data());
}

void ReadConfiguration(ini_lexer Ini){
    auto Token = Ini.Next();
    if(Token.Type != ini_token_type::Section || Token.Key != "general"){
        Ini.Error("Expected [general] section.");
    }

    bool HasInterval = false;

    while(Token.Type != ini_token_type::Eof){
        Token = Ini.Next();

        switch(Token.Type){
            case ini_token_type::Eof: {
                break;
            }
            case ini_token_type::Section: {
                Ini.Error("Unexpected section.");
            }
            case ini_token_type::KeyValue: {
                if(Token.Key == "interval"){
                    if(HasInterval){
                        Ini.Error("Event interval specified twice.");
                    }

                    if(ParseInt(Token.Value,&Interval,Ini.Name().c_str()) != Token.Value.end()){
                        Ini.Error("Invalid value for the event interval.");
                    }

                    if(Interval == 0){
                        Ini.Error("Invalid value for the event interval.");
                    }

                    HasInterval = true;
                }else if(Token.Key == "upbind"){
                    if(UpKeyCode != 0){
                        Ini.Error("Scroll up key bind specified twice.");
                    }

                    if(ParseInt(Token.Value,&UpKeyCode,Ini.Name().c_str()) != Token.Value.end()){
                        Ini.Error("Invalid value for scroll up key bind.");
                    }

                    if(UpKeyCode == 0 || UpKeyCode > 0xFE){
                        Ini.Error("Invalid value for scroll up key bind.");
                    }
                }else if(Token.Key == "downbind"){
                    if(DownKeyCode != 0){
                        Ini.Error("Scroll down key bind specified twice.");
                    }

                    if(ParseInt(Token.Value,&DownKeyCode,Ini.Name().c_str()) != Token.Value.end()){
                        Ini.Error("Invalid value for scroll down key bind.");
                    }

                    if(DownKeyCode == 0 || DownKeyCode > 0xFE){
                        Ini.Error("Invalid value for scroll down key bind.");
                    }
                }else{
                    Ini.Error("Invalid key/value pair.");
                }

                break;
            }
        }
    }

    if(DownKeyCode == UpKeyCode){
        if(DownKeyCode == 0){
            Ini.Error("No key was bound to either scroll direction.");
        }else{
            Ini.Error("Scroll up and scroll down both have the same binding.");
        }
    }
}

bool IsGameInFocus(){
    auto Window = GetForegroundWindow();

    constexpr int BufferSize = 35;
    wchar_t Buffer[BufferSize];

    std::wstring_view Str = {
        Buffer,
        static_cast<std::size_t>(GetClassNameW(Window,Buffer,BufferSize))
    };

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

bool HandleKey(DWORD KeyCode,bool IsDown){
    unsigned Mask = 0;

    if(KeyCode != 0){
        if(KeyCode == DownKeyCode){
            Mask = SpamDownBit;
        }else if(KeyCode == UpKeyCode){
            Mask = SpamUpBit;
        }
    }

    if(Mask == 0 || !IsGameInFocus()){
        return false;
    }

    if(IsDown){
        SpamFlags |=  Mask;
    }else{
        SpamFlags &= ~Mask;
    }

    return true;
}

LRESULT CALLBACK LowLevelMouseProc(int Code,WPARAM WParam,LPARAM LParam){
    if(Code < HC_ACTION){
        return CallNextHookEx(MouseHook,Code,WParam,LParam);
    }

    assert(Code == HC_ACTION);

    DWORD KeyCode = 0;
    bool IsDown = false;
    switch(WParam){
        case WM_LBUTTONDOWN: {
            IsDown = true;

            [[fallthrough]];
        }
        case WM_LBUTTONUP: {
            KeyCode = VK_LBUTTON;
            break;
        }
        case WM_RBUTTONDOWN: {
            IsDown = true;

            [[fallthrough]];
        }
        case WM_RBUTTONUP: {
            KeyCode = VK_RBUTTON;
            break;
        }
        case WM_MBUTTONDOWN: {
            IsDown = true;

            [[fallthrough]];
        }
        case WM_MBUTTONUP: {
            KeyCode = VK_MBUTTON;
            break;
        }
        case WM_XBUTTONDOWN: {
            IsDown = true;

            [[fallthrough]];
        }
        case WM_XBUTTONUP: {
            auto& Info = *reinterpret_cast<MSLLHOOKSTRUCT*>(LParam);
            KeyCode = VK_XBUTTON1+HIWORD(Info.mouseData)-XBUTTON1;

            break;
        }
    }

    if(HandleKey(KeyCode,IsDown)){
        return 1;
    }

    return CallNextHookEx(MouseHook,Code,WParam,LParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int Code,WPARAM WParam,LPARAM LParam){
    if(Code < HC_ACTION){
        return CallNextHookEx(KeyboardHook,Code,WParam,LParam);
    }

    assert(Code == HC_ACTION);

    auto& Info = *reinterpret_cast<KBDLLHOOKSTRUCT*>(LParam);

    auto IsDown = (WParam == WM_KEYDOWN || WParam == WM_SYSKEYDOWN);

    if(HandleKey(Info.vkCode,IsDown)){
        return 1;
    }

    return CallNextHookEx(KeyboardHook,Code,WParam,LParam);
}

DWORD GetWheelDelta(unsigned Flags){
    DWORD r = 0;

    if((Flags&SpamDownBit) != 0){
        r -= WHEEL_DELTA;
    }

    if((Flags&SpamUpBit) != 0){
        r += WHEEL_DELTA;
    }

    return r;
}

void CALLBACK TimerProc(UINT,UINT,DWORD_PTR,DWORD_PTR,DWORD_PTR){
    if(IsGameInFocus()){
        auto WheelDelta = GetWheelDelta(SpamFlags);

        if(WheelDelta != 0){
            INPUT Input = {};
            Input.type = INPUT_MOUSE;
            Input.mi.mouseData = WheelDelta;
            Input.mi.dwFlags = MOUSEEVENTF_WHEEL;

            SendInput(1,&Input,sizeof(Input));
        }
    }else{
        SpamFlags = 0;
    }
}

bool IsMouseButton(DWORD KeyCode){
    return
        (VK_LBUTTON <= KeyCode && KeyCode <= VK_RBUTTON) ||
        (VK_MBUTTON <= KeyCode && KeyCode <= VK_XBUTTON2);
}

bool IsKeyboardKey(DWORD KeyCode){
    return KeyCode != 0 && !IsMouseButton(KeyCode);
}

}

int main(int argc,char** argv){
    try {
        auto ConfigFilename = "Dish2Macro.ini";

        if(argc >= 2){
            ConfigFilename = argv[1];
        }

        ReadConfiguration(ini_lexer(ConfigFilename));

        if(IsMouseButton(DownKeyCode) || IsMouseButton(UpKeyCode)){
            MouseHook = SetWindowsHookExW(WH_MOUSE_LL,LowLevelMouseProc,nullptr,0);

            if(!MouseHook){
                throw std::system_error(GetLastError(),std::system_category());
            }
        }

        if(IsKeyboardKey(DownKeyCode) || IsKeyboardKey(UpKeyCode)){
            KeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL,LowLevelKeyboardProc,nullptr,0);

            if(!KeyboardHook){
                throw std::system_error(GetLastError(),std::system_category());
            }
        }

        if(!timeSetEvent(Interval,1,TimerProc,0,TIME_PERIODIC|TIME_CALLBACK_FUNCTION)){
            throw std::system_error(GetLastError(),std::system_category());
        }

        for(;;){
            MSG Message;
            while(GetMessageW(&Message,nullptr,0,0)){
                TranslateMessage(&Message);
                DispatchMessageW(&Message);
            }
        }
    }catch(std::exception& e){
        if(MouseHook){
            UnhookWindowsHookEx(MouseHook);
        }

        if(KeyboardHook){
            UnhookWindowsHookEx(KeyboardHook);
        }

        std::cerr << "Error: " << e.what() << "\n\nPress Enter to exit...\n";

        std::cin.get();

        return 1;
    }
}
