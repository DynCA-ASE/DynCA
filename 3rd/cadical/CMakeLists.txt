cmake_minimum_required(VERSION 3.21)

project(cadical VERSION 3.0.0)

set(CADICAL_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
set(CADICAL_INCLUDE_HEADER "${CADICAL_INCLUDE_DIR}/cadical.hpp")
set(CADICAL_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CADICAL_LIBRARY "${CADICAL_LIB_DIR}/libcadical.a")
execute_process(
    COMMAND mkdir -p ${CADICAL_INCLUDE_DIR} ${CADICAL_LIB_DIR}
)

include(ExternalProject)
ExternalProject_Add(libcadical
    DOWNLOAD_COMMAND   bash ${CMAKE_CURRENT_SOURCE_DIR}/download.sh
    PREFIX             ${CMAKE_CURRENT_BINARY_DIR}
    SOURCE_DIR         src/cadical-rel-3.0.0
    CONFIGURE_COMMAND  cd <SOURCE_DIR> && ./configure CXX=${CMAKE_CXX_COMPILER}
    BUILD_COMMAND      make -C <SOURCE_DIR>/build -j libcadical.a
    INSTALL_COMMAND    rm -f ${CADICAL_LIBRARY} ${CADICAL_INCLUDE_HEADER}
                    && ln -s <SOURCE_DIR>/build/libcadical.a ${CADICAL_LIBRARY}
                    && ln -s <SOURCE_DIR>/src/cadical.hpp ${CADICAL_INCLUDE_HEADER}
    BUILD_BYPRODUCTS   ${CADICAL_LIBRARY} ${CADICAL_INCLUDE_HEADER}
)

add_library(cadical STATIC IMPORTED GLOBAL)
set_target_properties(cadical PROPERTIES
    IMPORTED_LOCATION "${CADICAL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CADICAL_INCLUDE_DIR}"
)
