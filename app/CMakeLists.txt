include(GNUInstallDirs)

add_executable(zportald "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(zportald PRIVATE zportal)

if(WITH_ASAN_UBSAN)
    set_asan_ubsan(zportald)
endif()

install(TARGETS zportald
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
