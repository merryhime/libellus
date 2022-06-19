include(AddResources)

add_resources(test_resource
    NAMESPACE libellus
    HEADER_PATH resources
    FILES
        repository.cpp
)

add_executable(libellus
    main.cpp
    repository.cpp
    repository.hpp
)
target_link_libraries(libellus PRIVATE merry::mcl PkgConfig::poppler PkgConfig::libgit2 test_resource ${Boost_LIBRARIES})
target_include_directories(libellus PRIVATE .)
