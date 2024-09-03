find_package(Boost REQUIRED)

add_library_unity(
  duckdb_common_ipc
  OBJECT
  shared_memory_manager.cpp)

set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:duckdb_common_ipc>
    PARENT_SCOPE)
