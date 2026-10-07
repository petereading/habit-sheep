FetchContent_Declare(ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ArduinoJson)
add_executable(HabitSheepTest
  HabitSheepTest.cpp
  ${REPO_ROOT}/src/HabitSheepStore.cpp
  ${REPO_ROOT}/src/SheepStateStore.cpp
  ${REPO_ROOT}/src/HabitEventLog.cpp
  ${REPO_ROOT}/src/HabitReset.cpp
  ${REPO_ROOT}/src/HabitTimer.cpp
)
target_include_directories(HabitSheepTest PRIVATE stubs ${REPO_ROOT}/src)
target_link_libraries(HabitSheepTest PRIVATE crosspoint_test_common GTest::gtest_main ArduinoJson)
gtest_discover_tests(HabitSheepTest)
