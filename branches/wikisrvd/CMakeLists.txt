cmake_minimum_required(VERSION 2.8)
project(wikisrvd C CXX)

include(CheckTypeSize)
check_type_size(off64_t OFF64_T)
if(HAVE_OFF64_T)
   #add_definitions(-D_LARGEFILE64_SOURCE=1)
   add_definitions(-D_FILE_OFFSET_BITS=64)
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
