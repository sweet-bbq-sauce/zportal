include(GNUInstallDirs)

add_executable(zportald "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(zportald PRIVATE zportal)

if(WITH_ASAN_UBSAN)
    set_asan_ubsan(zportald)
endif()

install(TARGETS zportald
    EXPORT zportaldTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)