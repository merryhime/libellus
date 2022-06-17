add_executable(libellus main.cpp)
target_link_libraries(libellus PRIVATE merry::mcl ${Boost_LIBRARIES})
