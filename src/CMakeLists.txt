add_executable(libellus
    main.cpp
    repository.cpp
    repository.hpp
)
target_link_libraries(libellus PRIVATE merry::mcl PkgConfig::poppler PkgConfig::libgit2 ${Boost_LIBRARIES})
target_include_directories(libellus PRIVATE .)
