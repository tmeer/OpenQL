INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/programs
  ${CMAKE_SOURCE_DIR}/tests
)


ADD_EXECUTABLE(test_commute_vars test_commute_vars.cc)
TARGET_LINK_LIBRARIES(test_commute_vars ${LEMON_LIBRARIES} )
