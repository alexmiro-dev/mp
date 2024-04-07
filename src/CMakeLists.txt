
add_library(mp INTERFACE 
    include/memory_pool/types.hpp
    include/memory_pool/slot_status_registry.hpp
    include/memory_pool/allocator.hpp
)

add_library(memory_pool::mp ALIAS mp)

set_property(TARGET mp PROPERTY EXPORT_NAME memory_pool)

target_include_directories(
    mp
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_subdirectory(test)

