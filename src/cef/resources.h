#pragma once
#include <filesystem>
#include "include/cef_scheme.h"
namespace aurora {
void register_resources(const std::filesystem::path& directory);
}
