add_executable(udf_server udf_server.cpp)
target_link_libraries(udf_server rt duckdb pthread arrow_python arrow ${PYTHON_LIBRARIES})


