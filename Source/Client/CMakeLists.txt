add_library(Client Client.cpp Client.hpp)
target_link_libraries(Client Qt6::Widgets)
target_include_directories(Client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
