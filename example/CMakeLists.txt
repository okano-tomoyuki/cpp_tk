foreach(src_name 
    sample1 
    calc 
    eyes 
    clock 
    multithread_text
    calendar_demo
)
    add_executable(${src_name}
        ${src_name}.cpp
    )

    target_include_directories(${src_name}
        PRIVATE
            ${PROJECT_SOURCE_DIR}
    )

    target_link_libraries(${src_name}
        PRIVATE
            ${PROJECT_NAME}
    )

    cpp_tk_configure_target(${src_name} example)
endforeach()

add_subdirectory(theme)
