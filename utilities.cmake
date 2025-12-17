macro(installmsg message_text)
    install(CODE "message(STATUS \"${message_text}\")")
endmacro()

function(aux_include_directory dir var)
    file(GLOB _files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${dir}/*.h)
    set(${var} ${_files} PARENT_SCOPE)  # 使用 PARENT_SCOPE 明确设置外部变量
endfunction()

function(check_qt_build_type should_be_static)
    get_target_property(qt_build_type Qt6::Core TYPE)
    if(NOT qt_build_type)
        message(WARNING "没有检查到Qt6::Core")
    endif()
    if( (should_be_static AND (NOT qt_build_type STREQUAL "STATIC_LIBRARY"))
        OR
        ((NOT should_be_static) AND (NOT qt_build_type STREQUAL "SHARED_LIBRARY")) )
        message(WARNING "不匹配的库类型 ${qt_build_type}")
    endif()
endfunction()
