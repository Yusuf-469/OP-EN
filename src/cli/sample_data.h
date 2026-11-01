#pragma once

#include <cstddef>
#include <string>

namespace uew {

void writeHeatSinkStl(const std::string& path);  // ASCII STL, millimetres
void writeHeatSinkCsv(const std::string& path);  // synthetic nodal temperatures, metres / C
void writeLargeStl(const std::string& path, std::size_t triangles);
void writeLargeCsv(const std::string& path, std::size_t nodes);

}  // namespace uew
