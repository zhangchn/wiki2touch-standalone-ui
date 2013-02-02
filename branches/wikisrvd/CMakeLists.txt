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
