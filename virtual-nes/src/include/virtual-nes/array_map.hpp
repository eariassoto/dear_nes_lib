// Copyright (c) 2020 Emmanuel Arias
#pragma once
#include <algorithm>
#include <array>
#include <stdexcept>

// This class was taken from Jason Turner video
// https://www.youtube.com/watch?v=INn3xa4pMfg

/// <summary>
/// This class implements a fixed sized hash map. The map
/// uses a std::array to store the items. Searching in this
/// hash map has linear complexity. However, given that the
/// NES instruction set is quite small (151 instructions without
/// illegal/unused ops) we might even have better search times than
/// with the regular std::map.
/// </summary>
/// <typeparam name="Key">Hash map key type</typeparam>
/// <typeparam name="Value">Value type we want to store in the map</typeparam>
/// <typeparam name="Size">Fixed size for the map</typeparam>
template <typename Key, typename Value, std::size_t Size>
struct Map {
    /// <summary>
    /// Internal data container
    /// </summary>
    std::array<std::pair<Key, Value>, Size> data;

    [[nodiscard]] constexpr Value at(const Key &key) const {
        const auto itr =
            std::find_if(begin(data), end(data),
                         [&key](const auto &v) { return v.first == key; });
        if (itr != end(data)) {
            return itr->second;
        } else {
            throw std::range_error("Not Found");
        }
    }
};
