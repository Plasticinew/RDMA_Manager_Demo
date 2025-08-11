enable_testing()
set(GTEST_LIB gtest gtest_main)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(flattingClient flattingClient.cc)
target_link_libraries(flattingClient rdmanager)

add_executable(flattingServer flattingServer.cc)
target_link_libraries(flattingServer rdmanager)



