set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/address.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tun.cpp"
)

find_package(spdlog CONFIG REQUIRED)

add_library(zportal ${SOURCES})
target_include_directories(zportal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
)
target_link_libraries(zportal PUBLIC spdlog::spdlog)