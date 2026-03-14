cmake_minimum_required(VERSION 3.16)
project(stalcraft_cheat)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -march=native -flto")

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)
find_package(Curses REQUIRED)

include_directories(include)

# Основной чит
add_executable(stalcraft_cheat
    src/main.cpp
    src/memory.cpp
    src/aimbot.cpp
    src/terminal_menu.cpp
    src/hiding.cpp
    src/offset_finder.cpp
)

target_link_libraries(stalcraft_cheat
    OpenGL::GL
    GLEW::GLEW
    glfw
    Threads::Threads
    ${X11_LIBRARIES}
    ${CURSES_LIBRARIES}
    dl
    rt
)

# Структурный анализатор
add_executable(struct_finder
    src/auto_struct_finder.cpp
    src/memory.cpp
)

target_link_libraries(struct_finder
    Threads::Threads
)

# Wallhack
add_executable(wallhack
    src/main_wallhack.cpp
    src/memory.cpp
    src/wallhack.cpp
    src/offset_finder.cpp
    src/hiding.cpp
)

target_link_libraries(wallhack
    Threads::Threads
    ${X11_LIBRARIES}
)

# Простой ВХ
add_executable(simple_wh
    src/simple_wh.cpp
    src/memory.cpp
    src/offset_finder.cpp
)

target_link_libraries(simple_wh
    Threads::Threads
)

# Поиск смещения координат
add_executable(find_coord_offset
    src/find_coord_offset.cpp
    src/memory.cpp
)

target_link_libraries(find_coord_offset
    Threads::Threads
)

# Поиск игрока по координатам
add_executable(find_player
    src/find_player_by_coords.cpp
    src/memory.cpp
)

target_link_libraries(find_player
    Threads::Threads
)

# Автопоиск структуры
add_executable(auto_find
    src/auto_find_player.cpp
    src/memory.cpp
)

target_link_libraries(auto_find
    Threads::Threads
)

# Авто ВХ
add_executable(wh_auto
    src/wh_auto.cpp
    src/memory.cpp
)

target_link_libraries(wh_auto
    Threads::Threads
)

# Тест регионов
add_executable(test_regions
    src/test_regions.cpp
    src/memory.cpp
)

target_link_libraries(test_regions
    Threads::Threads
)

# Тест анонимных регионов
add_executable(anon_regions
    src/test_regions2.cpp
    src/memory.cpp
)

target_link_libraries(anon_regions
    Threads::Threads
)

# Тест координат
add_executable(coord_test
    src/test_coords.cpp
    src/memory.cpp
)

target_link_libraries(coord_test
    Threads::Threads
)

# Поиск реальных координат
add_executable(real_coords
    src/find_real_coords.cpp
    src/memory.cpp
)

target_link_libraries(real_coords
    Threads::Threads
)

# Финальный ВХ
add_executable(radar_final
    src/wh_final.cpp
    src/memory.cpp
)

target_link_libraries(radar_final
    Threads::Threads
)

# Поиск оффсетов (v1)
add_executable(offset_scanner
    src/final_offset_finder.cpp
    src/memory.cpp
)

target_link_libraries(offset_scanner
    Threads::Threads
)

# Ультимативный поиск оффсетов
add_executable(ultimate_offset_finder
    src/ultimate_finder.cpp
    src/memory.cpp
)

target_link_libraries(ultimate_offset_finder
    Threads::Threads
)

# ПОИСК ИМЁН (НОВОЕ ИМЯ)
add_executable(name_scanner
    src/ultimate_finder_names.cpp
    src/memory.cpp
)

target_link_libraries(name_scanner
    Threads::Threads
)

configure_file(scripts/run.sh run.sh COPYONLY)

# Ультимативный чит
add_executable(ultimate
    src/ultimate_cheat.cpp
    src/memory.cpp
)

target_link_libraries(ultimate
    Threads::Threads
)
