#pragma once

#include <string>
#include <string_view>

namespace ViewerPackUltis {

std::string sanitizeFileName(std::string_view input, char replacement = '_');

} // namespace ViewerPackUltis
