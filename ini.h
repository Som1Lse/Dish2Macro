#ifndef INI_H_INCLUDED
    #define INI_H_INCLUDED

#include <ios>
#include <string>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <string_view>

// A VERY rudimentary ini lexer.

enum class ini_token_type {
    Eof,
    Section,
    KeyValue,
};

struct ini_token {
    ini_token_type Type;

    std::string_view Key;
    std::string_view Value;
};

template <typename ItT>
inline ItT TrimLeft(ItT Begin,ItT End){
    auto IsSpace = [](auto c){
        return '\0' <= c && c <= ' ';
    };

    return std::find_if_not(Begin,End,IsSpace);
}

template <typename ItT>
inline ItT TrimRight(ItT Begin,ItT End){
    return TrimLeft(std::make_reverse_iterator(End),std::make_reverse_iterator(Begin)).base();
}

template <typename ItT>
inline std::basic_string_view<typename ItT::value_type> StringView(ItT Begin,ItT End){
    return {&*Begin,static_cast<std::size_t>(End-Begin)};
}

class ini_lexer {
public:
    explicit ini_lexer(const char* Filename)
            :File(Filename,std::ios::binary),Line(),LineNum(0),Filename(Filename) {
        if(!File){
            throw std::runtime_error(std::string("Unable to open configuration file: ")+Filename);
        }
    }

    ini_token Next(){
        ini_token r = {};

        while(std::getline(File,Line)){
            LineNum += 1;

            auto Begin = TrimLeft(Line.begin(),Line.end());
            auto End = TrimRight(Begin,std::find(Begin,Line.end(),';'));

            if(Begin == End){
                continue;
            }

            if(Begin[0] == '['){
                if(End[-1] != ']'){
                    Error("Invalid section name. Starts with '[' but does not end with ']'.");
                }

                r.Type = ini_token_type::Section;

                ++Begin;
                --End;
            }else{
                auto Middle = std::find(Begin,End,'=');
                if(Middle == End){
                    Error("Expected '=' followed by a value but found nothing.");
                }

                r.Type = ini_token_type::KeyValue;

                r.Value = StringView(TrimLeft(Middle+1,End),End);

                if(r.Value.empty()){
                    Error("Empty value string.");
                }

                End = TrimRight(Begin,Middle);
            }

            std::for_each(Begin,End,[](auto& c){
                c = ('A' <= c && c <= 'z')?c|32:c;
            });

            r.Key = StringView(Begin,End);

            if(r.Key.empty()){
                Error("Empty key string.");
            }

            break;
        }

        return r;
    }

    std::string Name() const {
        return Filename+(":"+std::to_string(LineNum));
    }

    void Error(const char* Message) const {
        throw std::runtime_error(Name()+": "+Message);
    }

private:
    std::ifstream File;
    std::string Line;

    std::intmax_t LineNum;

    const char* Filename;
};

#endif // INI_H_INCLUDED
