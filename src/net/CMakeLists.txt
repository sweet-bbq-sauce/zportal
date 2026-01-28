add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/address")

set(SOURCES
    ${SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/connection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ring.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/socket.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tun.cpp"
    PARENT_SCOPE
)