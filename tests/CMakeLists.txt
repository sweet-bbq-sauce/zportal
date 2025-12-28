include(GoogleTest)

set(TESTS
    "${CMAKE_CURRENT_SOURCE_DIR}/address.cpp"
)

function(add_gtest file)
    get_filename_component(name "${file}" NAME_WE)
    add_executable(${name} ${file})
    target_link_libraries(${name} PRIVATE zportal GTest::gtest_main)
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 60)
endfunction()

foreach(t IN LISTS TESTS)
    add_gtest(${t})
endforeach()