file(GLOB_RECURSE srcs CMAKE_CONFIGURE_DEPENDS include/*.hpp core/*.cpp)


find_package(Boost REQUIRED COMPONENTS  system) 

include_directories(${Boost_INCLUDE_DIRS})

add_executable(Server  ${srcs})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}) 

target_include_directories(Server PUBLIC include)

target_link_libraries(Server Boost::system)

target_link_libraries(Server logger)

