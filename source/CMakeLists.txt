
include_directories(${PROJECT_SOURCE_DIR}/include)

set(BASE_SOURCE dev_manager.cc rdma_conn.cc rdma_server.cc)

add_library(rdmanager ${BASE_SOURCE})

target_link_libraries(rdmanager ibverbs rdmacm pthread rt)

