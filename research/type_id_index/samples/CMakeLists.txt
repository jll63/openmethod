cmake_minimum_required(VERSION 3.10)
project(typeinfo_samples CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Boost packages from vcpkg
find_package(Boost REQUIRED COMPONENTS filesystem system program_options json)

# collect-libraries executable
add_executable(collect-libraries src/collect-libraries.cpp)
target_link_libraries(collect-libraries PRIVATE Boost::filesystem Boost::system Boost::json)

# collect-type-infos executable
add_executable(collect-type-infos src/collect-type-infos.cpp)
target_link_libraries(collect-type-infos PRIVATE Boost::filesystem Boost::system Boost::program_options Boost::json)
