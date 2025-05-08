add_library(Server Server.cpp Server.hpp)
target_link_libraries(Server Qt6::Widgets)
target_include_directories(Server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
