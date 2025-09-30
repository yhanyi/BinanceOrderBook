cmake_minimum_required(VERSION 3.20)
project(orderbook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIRS})

set(SOURCES
  src/main.cpp
  src/orderbook.cpp
  src/wsclient.cpp
  src/httpclient.cpp
)

add_executable(orderbook ${SOURCES})

target_link_libraries(orderbook
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${OPENSSL_LIBRARIES}
)
