cmake_minimum_required(VERSION 3.10)
project(AIDungeon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

set(USOCKETS_DIR ${CMAKE_SOURCE_DIR}/uWebSockets/uSockets/src)
set(UWEBSOCKETS_DIR ${CMAKE_SOURCE_DIR}/uWebSockets/src)

add_library(uSockets STATIC
    ${USOCKETS_DIR}/bsd.c
    ${USOCKETS_DIR}/context.c
    ${USOCKETS_DIR}/loop.c
    ${USOCKETS_DIR}/socket.c
    ${USOCKETS_DIR}/eventing/epoll_kqueue.c
    ${USOCKETS_DIR}/crypto/openssl.c
    ${USOCKETS_DIR}/crypto/sni_tree.cpp
)

target_compile_definitions(uSockets PRIVATE LIBUS_USE_OPENSSL)

target_include_directories(uSockets PUBLIC
    ${USOCKETS_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(uSockets PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads
)

add_executable(server
    main.cpp
    server/GameState.cpp
    server/MapGenerator.cpp
    server/Combat.cpp
    server/Inventory.cpp
    server/FogOfWar.cpp
    server/AI.cpp
)

target_include_directories(server PRIVATE
    ${CMAKE_SOURCE_DIR}/server
    ${UWEBSOCKETS_DIR}
    ${USOCKETS_DIR}
)

target_link_libraries(server PRIVATE
    uSockets
    Threads::Threads
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
)

add_executable(tests
    tests/TestRunner.cpp
    tests/TestMap.cpp
    tests/TestCombat.cpp
    tests/TestInventory.cpp
    tests/TestFog.cpp
    tests/TestGameState.cpp
    server/GameState.cpp
    server/MapGenerator.cpp
    server/Combat.cpp
    server/Inventory.cpp
    server/FogOfWar.cpp
    server/AI.cpp
)

target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/server
)

target_link_libraries(tests PRIVATE
    Threads::Threads
)