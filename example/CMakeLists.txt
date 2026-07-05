foreach(src_name 
    sample1 
    sample2 
    calc 
    eyes 
    clock 
    scrolled_text
    # photo_image
    tree_view
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
endforeach()

add_subdirectory(theme)
