module_version(1.0)

add_library(overlay MODULE 
    overlay.cpp)

target_link_libraries(overlay omega)
declare_native_module(overlay)
