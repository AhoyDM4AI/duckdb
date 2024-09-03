find_package(Arrow REQUIRED)
find_package(Boost REQUIRED)

include_directories(${Arrow_INCLUDE_DIRS})

add_library_unity(
  duckdb_common_arrow
  OBJECT
  arrow_appender.cpp
  arrow_converter.cpp
  arrow_wrapper.cpp
  physical_arrow_collector.cpp
  arrow_query_result.cpp
  arrow_transform_util.cpp)
add_subdirectory(appender)
set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:duckdb_common_arrow>
    PARENT_SCOPE)
