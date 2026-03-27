cmake_minimum_required(VERSION 3.24)
include(FetchContent)

project(DynCA)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
string(REPLACE "-DNDEBUG" "-D_GLIBCXX_ASSERTIONS" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
string(APPEND CMAKE_CXX_FLAGS_DEBUG " -D_GLIBCXX_ASSERTIONS")

option(ENABLE_CXX_COMPILER_CHECK "enable cmake compiler check" ON)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12)
        message(FATAL_ERROR "Please make sure you are using g++ >= 12. Current version is: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
        message(FATAL_ERROR "Please make sure you are using clang++ >= 16. Current version is: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
elseif(ENABLE_CXX_COMPILER_CHECK)
    message(FATAL_ERROR "Please use standard g++ or clang++ to compile this project. Ignore this by setting `ENABLE_CXX_COMPILER_CHECK' to `OFF'.")
endif()

include_directories("src")
include_directories("3rd")

file(GLOB_RECURSE SRC_DIR "src/*.cpp")
set_property(SOURCE ${SRC_DIR} PROPERTY COMPILE_FLAGS "-Wall -Werror")
add_executable(${PROJECT_NAME} ${SRC_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE pthread)

option(ENABLE_PROF "enable performance profile by -fno-omit-frame-pointer")
if (ENABLE_PROF)
    set_property(SOURCE ${SRC_DIR} PROPERTY COMPILE_FLAGS "-g -fno-omit-frame-pointer -fno-optimize-sibling-calls")
endif()

# jemalloc
option(ENABLE_JEMALLOC "use jemalloc instead of std malloc" ON)
if (ENABLE_JEMALLOC)
    if (DEFINED JEMALLOC_LIB_DIRECTORY)
        target_link_directories(${PROJECT_NAME} PRIVATE ${JEMALLOC_LIB_DIRECTORY})
        target_link_libraries(${PROJECT_NAME} PRIVATE jemalloc)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(JEMALLOC jemalloc REQUIRED)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${JEMALLOC_LIBRARIES})
    endif()
endif()

# fmt
find_package(fmt 10.0 QUIET)
if (fmt_FOUND AND fmt_VERSION VERSION_GREATER_EQUAL 10.0)
else ()
    FetchContent_Declare(fmt
        URL https://github.com/fmtlib/fmt/archive/refs/tags/10.0.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(fmt)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

# clipp
FetchContent_Declare(clipp
    URL https://github.com/muellan/clipp/tarball/2c32b2f1f7cc530b1ec1f62c92f698643bb368db
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(clipp)
# Replace `std::result_of<Fn(Args...)>::type`
execute_process(COMMAND sed -i "s/std::result_of<Fn\(Args...\)>::type/std::invoke_result_t<Fn, Args...>/g" ${CMAKE_BINARY_DIR}/_deps/clipp-src/include/clipp.h)
execute_process(COMMAND sed -i "s/std::result_of<Fn\(\)>::type/std::invoke_result_t<Fn>/g" ${CMAKE_BINARY_DIR}/_deps/clipp-src/include/clipp.h)
# Add [[maybe_unused]] attribute to disable warning.
execute_process(COMMAND sed -i "1409c [[maybe_unused]] int i = 0;" ${CMAKE_BINARY_DIR}/_deps/clipp-src/include/clipp.h)
target_link_libraries(${PROJECT_NAME} PRIVATE clipp::clipp)

# reflectcpp
FetchContent_Declare(reflectcpp
    URL https://github.com/getml/reflect-cpp/archive/refs/tags/v0.23.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(reflectcpp)
target_link_libraries(${PROJECT_NAME} PRIVATE reflectcpp::reflectcpp)

# minisat (with extension)
add_subdirectory(3rd/minisat)
target_link_libraries(${PROJECT_NAME} PRIVATE minisat-lib-static)

# cadical
add_subdirectory(3rd/cadical)
add_dependencies(${PROJECT_NAME} libcadical)
target_link_libraries(${PROJECT_NAME} PRIVATE cadical)

# tools
add_executable(convert_ca_cnf tools/convert_ca_cnf.cpp src/io/casa.cpp src/io/cnf.cpp src/io/ddnnf.cpp)
target_link_libraries(convert_ca_cnf fmt::fmt)
add_executable(convert_ca_casa tools/convert_ca_casa.cpp src/io/casa.cpp src/io/cnf.cpp src/io/ddnnf.cpp)
target_link_libraries(convert_ca_casa fmt::fmt)
add_executable(check_tuples tools/check_tuples.cpp)
target_link_libraries(check_tuples fmt::fmt)
add_executable(convert_cnf tools/convert_cnf.cpp src/io/casa.cpp src/io/cnf.cpp src/io/ddnnf.cpp)
target_link_libraries(convert_cnf fmt::fmt)
add_executable(convert_casa tools/convert_casa.cpp src/io/casa.cpp src/io/cnf.cpp src/io/ddnnf.cpp)
target_link_libraries(convert_casa fmt::fmt)
