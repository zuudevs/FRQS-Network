cmake_minimum_required(VERSION 3.20)
project(
	FRQS_NET
	VERSION 1.0.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

if(MSVC)
    add_compile_options(
		/W4 
		/permissive-
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
	src/utils/filesystem_utils.cpp
	src/utils/logger.cpp
	src/utils/thread_pool.cpp
	src/http/mime_types.cpp
	src/http/request.cpp
	src/http/response.cpp
	src/core/server.cpp
)

target_include_directories(FRQS_NET PRIVATE 
    "${CMAKE_SOURCE_DIR}/include"
)