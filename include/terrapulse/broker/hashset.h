#pragma once

#include <unordered_map>
#include <unordered_set>

namespace tp::broker {

template <typename T>
using HashSet = std::unordered_set<T>;

template <typename K, typename V>
using HashMap = std::unordered_map<K, V>;

} // namespace tp::broker
