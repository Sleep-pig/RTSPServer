file(GLOB_RECURSE srcs CONFIGURE_DEPENDS core/*.cpp include/*.hpp)
add_library(logger STATIC ${srcs})

target_include_directories(logger PUBLIC include)
