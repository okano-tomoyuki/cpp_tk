set(ns_name tk)
foreach(src_name object)
    set(lib_name ${ns_name}-${src_name})
    add_library(${lib_name}
        ${src_name}.cpp
    )
    target_link_libraries(${lib_name}
        PRIVATE
            tcl
    )
endforeach()