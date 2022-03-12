# CMakeList.txt : CMake project for sudoku_solver, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("sudoku_solver")

find_package(SFML COMPONENTS system window graphics CONFIG REQUIRED)

add_executable (sudoku_solver "sudoku_solver.cpp" "sudoku_solver.h")

target_compile_features(sudoku_solver PRIVATE cxx_std_20)

target_link_libraries(sudoku_solver PRIVATE sfml-system sfml-network sfml-graphics sfml-window)

file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})