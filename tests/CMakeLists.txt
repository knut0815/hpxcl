# Copyright (c) 2011 Bryce Adelstein-Lelbach
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

set(subdirs "")

if(HPXCL_BUILD_BENCHMARK)
  set(subdirs
    ${subdirs} performance)
endif()

if(HPXCL_BUILD_TESTS)
  set(subdirs
    ${subdirs} regressions)
endif()

if(HPXCL_BUILD_TESTS)
  set(subdirs
    ${subdirs} unit)
endif()

foreach(subdir ${subdirs})
  add_hpx_pseudo_target(tests.${subdir})
  add_subdirectory(${subdir})
  add_hpx_pseudo_dependencies(tests tests.${subdir})
endforeach()

