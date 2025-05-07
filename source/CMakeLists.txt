
include_directories(${PROJECT_SOURCE_DIR}/include)

set(BASE_SOURCE vContext.cc vQP.cc PigeonContext.cc DynamicContext.cc)

add_library(rdmanager ${BASE_SOURCE})

target_link_libraries(rdmanager ibverbs rdmacm pthread rt mlx5)

add_executable(Client Client.cc)
target_link_libraries(Client rdmanager)

add_executable(Server Server.cc)
target_link_libraries(Server rdmanager)

add_executable(Bench Bench.cc)
target_link_libraries(Bench rdmanager)
