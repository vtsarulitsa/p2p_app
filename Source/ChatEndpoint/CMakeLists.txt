add_library(ChatEndpoint ChatEndpoint.cpp ChatEndpoint.hpp)
target_link_libraries(ChatEndpoint Qt6::Widgets)
target_include_directories(ChatEndpoint PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
