include(GoogleTest)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp"
)

function(add_gtest file)
    file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}" "${file}")

    get_filename_component(stem "${rel}" NAME_WE)
    get_filename_component(dir "${rel}" DIRECTORY)

    if(dir STREQUAL "")
        set(name "${stem}")
    else()
        string(REPLACE "/" "_" dir_ "${dir}")
        string(REPLACE "\\" "_" dir_ "${dir_}")
        set(name "${dir_}_${stem}")
    endif()

    add_executable("${name}" "${file}")
    target_link_libraries("${name}" PRIVATE zportal GTest::gtest_main)
    gtest_discover_tests("${name}" DISCOVERY_TIMEOUT 60)
endfunction()

foreach(t IN LISTS TEST_SOURCES)
    add_gtest(${t})
endforeach()