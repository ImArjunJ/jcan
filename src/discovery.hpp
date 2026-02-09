#pragma once

#include <vector>

#include "types.hpp"

namespace jcan {

[[nodiscard]] std::vector<device_descriptor> discover_adapters();

}
