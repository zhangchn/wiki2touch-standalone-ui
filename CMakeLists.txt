cmake_minimum_required(VERSION 2.8)
project(wikisrvd C CXX)

include(CheckTypeSize)
include(CheckIncludeFiles)
include(CheckFunctionExists)

check_include_file(bzlib.h HAVE_BZLIB_H)
if(!HAVE_BZLIB_H)
    message(FATAL_ERROR "bzlib.h is required. If built in Linux, try install the package `libbz2-dev' or alike.")
endif()
check_type_size(off64_t OFF64_T)
if(HAVE_OFF64_T)
    #add_definitions(-D_LARGEFILE64_SOURCE=1)
    add_definitions(-D_FILE_OFFSET_BITS=64)
endif()
check_function_exists(srandomdev HAVE_SRANDOMDEV)
check_function_exists(srandom HAVE_SRANDOM)
if(HAVE_SRANDOMDEV)
    add_definitions(-DHAVE_SRANDOMDEV=1)
elseif(HAVE_SRANDOM)
    add_definitions(-DHAVE_SRANDOM=1)
endif()

add_executable(wikisrvd
				CPPStringUtils.cpp
				ImageIndex.cpp
				MathIndex.cpp
				StopWatch.cpp
				TitleIndex.cpp
				WikiMarkupGetter.cpp
				ConfigFile.cpp
				Main.cpp
				Settings.cpp
				StringUtils.cpp
				WikiArticle.cpp
				WikiMarkupParser.cpp)
TARGET_LINK_LIBRARIES(wikisrvd bz2)
