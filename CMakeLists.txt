cmake_minimum_required(VERSION 3.10)
project(TC_Language_Compiler_Backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tc_backend
    src/main.cpp
    src/automaton.cpp
    src/nfa.cpp
    src/lexer.cpp
    src/parser.cpp
    src/semantic.cpp
    src/translator.cpp
)

target_include_directories(tc_backend PRIVATE include)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/UI/win_main.cpp")
    add_executable(tc_gui WIN32
        UI/win_main.cpp
        UI/ui_app.cpp
        src/automaton.cpp
        src/nfa.cpp
        src/lexer.cpp
        src/parser.cpp
        src/semantic.cpp
        src/translator.cpp
    )

    target_include_directories(tc_gui PRIVATE include)

    if (WIN32)
        target_link_libraries(tc_gui PRIVATE Comctl32 gdi32 user32)
    endif()
endif()

