cmake_minimum_required(VERSION 3.12.0)
project(Dish2Macro VERSION 2.1.0 LANGUAGES CXX)

add_executable(Dish2Macro main.cpp)

target_compile_features(Dish2Macro PRIVATE cxx_std_17)

if(MSVC)
    target_compile_definitions(Dish2Macro PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries(Dish2Macro PRIVATE winmm)

install(TARGETS Dish2Macro
    RUNTIME DESTINATION .
)

install(FILES Dish2Macro.ini
    DESTINATION .
)
