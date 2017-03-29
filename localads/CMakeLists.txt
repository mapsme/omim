project(localads)

set(
  SRC
  campaign.hpp
  campaign_serialization.cpp
  campaign_serialization.hpp
)

add_library(${PROJECT_NAME} ${SRC})

omim_add_pybindings_subdirectory(pylocalads)
omim_add_test_subdirectory(localads_tests)

