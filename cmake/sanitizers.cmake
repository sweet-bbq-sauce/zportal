function(set_asan_ubsan target)
    target_compile_options(${target} PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -g
        -O1
    )

    target_link_options(${target} PRIVATE
        -fsanitize=address,undefined
    )
endfunction()