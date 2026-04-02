add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/address")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/connection")

set(SOURCES
    ${SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/resolve.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/socket.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tun.cpp"
    PARENT_SCOPE
)