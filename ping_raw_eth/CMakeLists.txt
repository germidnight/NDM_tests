cmake_minimum_required(VERSION 3.5)

project(ping2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ping2 main.cpp
    ethernet.cpp ethernet.h icmp.h main.cpp
    ip.h
    utils.h)

include(GNUInstallDirs)
install(TARGETS ping2
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
