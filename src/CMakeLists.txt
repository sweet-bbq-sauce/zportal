include(GNUInstallDirs)
include(CheckSymbolExists)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/net")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/iouring")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tools")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tunnel")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED IMPORTED_TARGET liburing)

add_library(zportal ${SOURCES})
target_include_directories(zportal PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(zportal PUBLIC PkgConfig::LIBURING)
target_compile_definitions(zportal PUBLIC
    ZPORTAL_VERSION="${PROJECT_VERSION}"
    ZPORTAL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ZPORTAL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ZPORTAL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    PROJECT_HOMEPAGE_URL="${PROJECT_HOMEPAGE_URL}"
    PROJECT_NAME="${PROJECT_NAME}"
)
set_target_properties(zportal PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS zportal
    EXPORT zportalTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/zportal"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp"
)

set(CMAKE_REQUIRED_INCLUDES ${LIBURING_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${LIBURING_LIBRARIES})

check_symbol_exists(io_uring_setup_buf_ring "liburing.h" HAVE_IO_URING_SETUP_BUF_RING)
check_symbol_exists(io_uring_buf_ring_init "liburing.h" HAVE_IO_URING_BUF_RING_INIT)

if(HAVE_IO_URING_SETUP_BUF_RING)
    target_compile_definitions(zportal PRIVATE HAVE_IO_URING_SETUP_BUF_RING=1)
else()
    target_compile_definitions(zportal PRIVATE HAVE_IO_URING_SETUP_BUF_RING=0)
endif()

if(HAVE_IO_URING_BUF_RING_INIT)
    target_compile_definitions(zportal PRIVATE HAVE_IO_URING_BUF_RING_INIT=1)
else()
    target_compile_definitions(zportal PRIVATE HAVE_IO_URING_BUF_RING_INIT=0)
endif()