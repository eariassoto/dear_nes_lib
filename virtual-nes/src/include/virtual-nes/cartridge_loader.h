// Copyright (c) 2020 Emmanuel Arias
#pragma once
#include <string>
#include <variant>

#include "virtual-nes/enums.h"

namespace virtualnes {

// Forward declaration
class Cartridge;
class CartridgeHeader;
class IMapper;

class CartridgeLoader {
   public:
    std::variant<CartridgeLoaderError, Cartridge*> LoadNewCartridge(
        const std::string& fileName);

   private:
    bool IsMapperSupported(uint8_t mapperId);

    IMapper* CreateMapper(const CartridgeHeader& header);
};

}  // namespace virtualnes