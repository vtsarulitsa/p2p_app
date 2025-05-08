add_library(Message Message.cpp Message.hpp)
target_link_libraries(Message Qt6::Widgets)
target_include_directories(Message PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
