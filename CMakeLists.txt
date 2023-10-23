cmake_minimum_required(VERSION 3.10)

project(xatlas)

# Create a variable to allow developers to set the path to the downloaded sdk.
# no default seeing as it doesn't install to any pre defined location.
set(LXSDK_PATH "" CACHE PATH "Path to root of downloaded LXSDK")

# Get all source and headers for lxsdk
file(GLOB LXSDK_SOURCES ${LXSDK_PATH}/common/*.cpp)
file(GLOB LXSDK_HEADERS ${LXSDK_PATH}/include/*.h?)

# CRT_SECURE_NO_WARNINGS on windows,
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Should create our library so we can now focus on our own project.
add_library(lxsdk STATIC ${LXSDK_SOURCES})
set_target_properties(lxsdk PROPERTIES LIBRARY_OUTPUT_DIRECTORY lib)
target_include_directories(lxsdk PRIVATE ${LXSDK_PATH}/include)

# This is the plug-in we create, shared makes it on windows to a .dll which is
# what we expect for a plug-in
set(XA_PATH "" CACHE PATH "Path to the root of xatlas")
add_library(xatlas SHARED source/xatlas_command.cpp ${XA_PATH}/source/xatlas/xatlas.h ${XA_PATH}/source/xatlas/xatlas.cpp)

# We also must include the headers for the sdk
target_include_directories(xatlas PRIVATE ${LXSDK_PATH}/include ${XA_PATH}/source/xatlas)

target_link_libraries(xatlas lxsdk)

set(PLUGIN_DIR "win64")

# Set the output to the folder Modo will search, 
# $<0:> is just to remove any config subfolders like DEBUG and RELEASE
set_target_properties(xatlas 
  PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/${PLUGIN_DIR}/$<0:>
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/${PLUGIN_DIR}/$<0:> # windows apparently needs this set as well
)
