cmake_minimum_required (VERSION 2.8.6)
project (perfjava)

find_package(JNI)
if (JNI_FOUND)
    message (STATUS "JNI_INCLUDE_DIRS=${JNI_INCLUDE_DIRS}")
    message (STATUS "JNI_LIBRARIES=${JNI_LIBRARIES}")
    message (STATUS "JAVA_INCLUDE_PATH=${JAVA_INCLUDE_PATH}")
    message (STATUS "JAVA_INCLUDE_PATH2=${JAVA_INCLUDE_PATH2}")
endif()
INCLUDE_DIRECTORIES(${JAVA_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${JAVA_INCLUDE_PATH2})

add_library(perfjava SHARED perf-java-agent.c perf-map-file.c)

find_package(Java REQUIRED)
include(UseJava)

set(CMAKE_JAVA_INCLUDE_PATH ${JAVA_INCLUDE_PATH}/../lib/tools.jar)

