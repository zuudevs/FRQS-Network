cmake_minimum_required(VERSION 3.20)
project(
    FRQS_NET
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

if(MSVC)
    # 1. Compile Options (Flag pengaturan compiler)
    add_compile_options(
        /W4             # Warning Level 4
        /permissive-    # Strict mode
        /utf-8          # Fix encoding
        /EHsc           # Fix error "try-catch" tadi
    )
    
    # 2. Compile Definitions (Macro preprocessor)
    # Gunakan ini biar clang-cl paham kalau ini Definisi, bukan file.
    add_compile_definitions(
        NOMINMAX            # Fix std::min/max conflict
        _WIN32_WINNT=0x0A00 # Target Windows 10/11
    )
else()
    add_compile_options(
        -Wall 
        -Wextra 
        -Wpedantic 
        -Wconversion 
        -Wshadow
    )
endif()

add_executable(FRQS_NET
    src/main.cpp
    src/net/ipv4.cpp
    src/net/sockaddr.cpp
    src/net/socket.cpp
    src/net/http_client.cpp
    src/utils/filesystem_utils.cpp
    src/utils/logger.cpp
    src/utils/thread_pool.cpp
    src/utils/input_injector.cpp
    src/utils/config.cpp
    src/utils/screen_capturer.cpp
    src/http/mime_types.cpp
    src/http/request.cpp
    src/http/response.cpp
    src/http/multipart_parser.cpp
    src/core/server.cpp
)

target_include_directories(FRQS_NET PRIVATE 
    "${CMAKE_SOURCE_DIR}/include"
)

# --- LINKING LIBRARIES (JANGAN DIHAPUS) ---
# Kamu butuh ini karena pakai Socket, Screen Capture, dan Input Injection
if(WIN32)
    target_link_libraries(FRQS_NET PRIVATE 
        Ws2_32  # Buat Networking (Winsock)
        Gdi32   # Buat Screen Capture (BitBlt, dll)
        User32  # Buat Input Injector (SendInput) & Screen info
    )
endif()