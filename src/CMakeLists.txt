set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/address.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/connection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/socket.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tun.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tunnel.cpp"
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED IMPORTED_TARGET liburing)

add_library(zportal ${SOURCES})
target_include_directories(zportal PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(zportal PUBLIC PkgConfig::LIBURING)