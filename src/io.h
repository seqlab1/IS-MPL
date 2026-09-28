#pragma once

#include <string>
#include <vector>

namespace is_mpl::detail {

std::vector<std::string> read_nonempty_lines(const std::string& path);

}  // namespace is_mpl::detail
