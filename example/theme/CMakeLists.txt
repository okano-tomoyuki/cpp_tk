foreach(src_name
    sv_ttk_demo
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
