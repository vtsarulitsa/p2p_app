add_library(ChatWindow ChatWindow.cpp ChatWindow.hpp)
target_link_libraries(ChatWindow Qt6::Widgets Client Server)
target_include_directories(ChatWindow PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
