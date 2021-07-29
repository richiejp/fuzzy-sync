cmake_minimum_required(VERSION 3.17)

project(fuzzy-sync VERSION 0.1 LANGUAGES C)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

add_compile_definitions(_FORTIFY_SOURCE=2)
add_compile_options(
  -O1 -Wall -Wextra -Werror
  -g -fno-omit-frame-pointer #-fsanitize=address
  -Wno-unused-const-variable)
string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

link_libraries(Threads::Threads)
#add_link_options(-fsanitize=address)
include_directories(include)

enable_testing()

function(fzsync_test name)
  add_executable(${name} test/${name}.c)
  add_test(${name} ${name} ${ARGN})
endfunction(fzsync_test)

fzsync_test(a_rare_data_race -f timings.csv)
fzsync_test(basic)
fzsync_test(multi)
