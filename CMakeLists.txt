cmake_minimum_required(VERSION 3.16)
project(stalcraft_cheat)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -march=native -flto")

# Пути к зависимостям
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

# Определяем макрос для ImGui (использовать GLEW)
add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GLEW)

# Включаем папку с заголовками
include_directories(include)

# Собираем все .cpp файлы из папки include (там лежат исходники ImGui и бэкендов)
file(GLOB IMGUI_SRC "include/*.cpp")

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/memory.cpp
    src/aimbot.cpp
    src/overlay.cpp
    src/hiding.cpp
    src/pattern_scanner.cpp
    ${IMGUI_SRC}
)

target_link_libraries(${PROJECT_NAME}
    OpenGL::GL
    GLEW::GLEW
    glfw
    Threads::Threads
    ${X11_LIBRARIES}
    dl
    rt
)

# Копируем скрипт запуска
configure_file(scripts/run.sh run.sh COPYONLY)
