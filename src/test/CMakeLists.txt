
function(create_test sourceFileName)
    set(dependencies "${ARGN}")
    set(binaryName ${sourceFileName}_ut)
    add_executable(${binaryName} ${sourceFileName}_test.cpp)
    target_link_libraries(${binaryName} ${dependencies} Boost::ut)
endfunction()

create_test(slot_status_registry memory_pool::mp)
create_test(allocator memory_pool::mp)

