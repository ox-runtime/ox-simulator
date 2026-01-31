set(API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/http_server.cpp
    PARENT_SCOPE
)

set(API_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/crow
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/include
    PARENT_SCOPE
)