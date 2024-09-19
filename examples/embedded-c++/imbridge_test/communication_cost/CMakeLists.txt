

add_executable(pf1_conmunication pf1.cpp)
target_link_libraries(pf1_conmunication duckdb)

add_executable(pf7_conmunication pf7.cpp)
target_link_libraries(pf7_conmunication duckdb)

add_executable(uc10_conmunication uc10.cpp)
target_link_libraries(uc10_conmunication duckdb)