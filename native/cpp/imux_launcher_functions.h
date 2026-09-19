#pragma once

#include <filesystem>

std::filesystem::path imux_launcher_app_data_root();
std::filesystem::path imux_launcher_find_game_executable();
int imux_launcher_try_launch_game();
