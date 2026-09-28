#pragma once

#include <string_view>

namespace pricer {

// Version de la bibliothèque (sert aussi à valider la chaîne de build : lib -> CLI / tests).
std::string_view version() noexcept;

}  // namespace pricer
