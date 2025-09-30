cmake_minimum_required(VERSION 3.20)
project(orderbook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

include(FetchContent)

find_package(Boost 1.70 CONFIG QUIET COMPONENTS system)
if(NOT Boost_FOUND)
    message(STATUS "Boost not found locally, fetching from source.")
    FetchContent_Declare(
        Boost
        URL https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz
        URL_HASH SHA256=b62bd839ea6c28265af9a1f68393eda37fab3611425d3b28882d8e424535ec9d
    )
    set(BOOST_INCLUDE_LIBRARIES system asio beast)
    set(BOOST_ENABLE_CMAKE ON)
    FetchContent_MakeAvailable(Boost)
    
    set(Boost_FOUND TRUE)
    set(BOOST_FETCHED TRUE)
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

set(SOURCES
    src/main.cpp
    src/orderbook.cpp
    src/wsclient.cpp
    src/httpclient.cpp
)

add_executable(orderbook ${SOURCES})

if(BOOST_FETCHED)
    target_link_libraries(orderbook
        Boost::system
        Boost::asio
        ${CMAKE_THREAD_LIBS_INIT}
        ${OPENSSL_LIBRARIES}
    )
else()
    target_link_libraries(orderbook
        ${Boost_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${OPENSSL_LIBRARIES}
    )
endif()
