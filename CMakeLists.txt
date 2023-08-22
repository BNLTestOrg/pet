if(DEFINED ENV{BASEDIR})
set(BASEDIR $ENV{BASEDIR})
else()
set(BASEDIR ${CMAKE_SOURCE_DIR})
endif()

cmake_minimum_required(VERSION 3.18)
include(${BASEDIR}/libs/makefiles/cmake/Prelude.cmake)

project(pet)
enable_shared_libs()
include(${BASEDIR}/libs/makefiles/cmake/App.cmake)

app(pet 
    SRCS pet.cxx 
    LIBS pet agsPage UI UITable utils basics cdevCns name UIUtils gpm 
    SYSLIBS 
)
include(Additions.cmake)
