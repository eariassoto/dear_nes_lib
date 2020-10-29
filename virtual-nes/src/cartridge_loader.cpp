// Copyright (c) 2020 Emmanuel Arias
#include "virtual-nes/cartridge_loader.h"

#include <fstream>
#include <vector>

#include "virtual-nes/cartridge.h"
#include "virtual-nes/cartridge_header.h"
#include "virtual-nes/mapper_000.h"

namespace virtualnes {

// Forward declaration
class Cartridge;

std::variant<CartridgeLoaderError, Cartridge*>
CartridgeLoader::LoadNewCartridge(const std::string& fileName) {
    std::variant<CartridgeLoaderError, Cartridge*> result;

    std::ifstream ifs;
    ifs.open(fileName, std::ifstream::binary);
    if (!ifs.is_open()) {
        result = CartridgeLoaderError::FILE_NOT_FOUND;
        return result;
    }

    // This consumes the header from the ifstream
    CartridgeHeader header{ifs};

    // Check for the mapper type
    if (!IsMapperSupported(header.GetMapperId())) {
        result = CartridgeLoaderError::MAPPER_NOT_SUPPORTED;
        return result;
    }

    IMapper* mapperPtr = CreateMapper(header);

    if (header.HasTrainerData()) {
        ifs.seekg(512, std::ios_base::cur);
    }

    std::vector<uint8_t> programMemory(header.GetProgramMemorySize());
    ifs.read(reinterpret_cast<char*>(programMemory.data()),
             programMemory.size());

    std::vector<uint8_t> characterMemory(header.GetCharacterMemorySize());
    ifs.read(reinterpret_cast<char*>(characterMemory.data()),
             characterMemory.size());

    result = new Cartridge{std::move(header), mapperPtr, std::move(programMemory),
                       std::move(characterMemory)};

    return result;
}

bool CartridgeLoader::IsMapperSupported(uint8_t mapperId) {
    if (mapperId == 0x00) {
        return true;
    }
    return false;
}

IMapper* CartridgeLoader::CreateMapper(const CartridgeHeader& header) {
    if (header.GetMapperId() == 0x00) {
        return new Mapper_000(header.GetProgramMemoryBanks(),
                              header.GetCharacterMemoryBanks());
    }
    // This should not be reachable
    return nullptr;
}

}  // namespace virtualnes