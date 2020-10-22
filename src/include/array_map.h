// Copyright (c) 2020 Emmanuel Arias
#pragma once
#include <algorithm>
#include <array>
#include <stdexcept>


// This class was taken from Jason Turner video
// https://www.youtube.com/watch?v=INn3xa4pMfg
template <typename Key, typename Value, std::size_t Size>
struct Map {
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
