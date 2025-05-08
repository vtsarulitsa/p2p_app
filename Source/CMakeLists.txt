cmake_minimum_required(VERSION 3.16)
project(p2p_chat_app VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt (adjust version as needed)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Include subdirectories
add_subdirectory(Client)
add_subdirectory(Server)
add_subdirectory(ChatWindow)
add_subdirectory(Message)

# Main application
add_executable(p2p_app main.cpp)

# Link all modules and Qt
target_link_libraries(p2p_app
    Qt6::Widgets
    ChatWindow
    Message
)
