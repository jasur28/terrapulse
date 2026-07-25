#pragma once

#include <memory>

namespace tp::core {

template <typename T>
using Ptr = std::shared_ptr<T>;

template <typename T>
using CPtr = std::shared_ptr<const T>;

} // namespace tp::core
