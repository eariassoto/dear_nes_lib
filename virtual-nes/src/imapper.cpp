// Copyright (c) 2020 Emmanuel Arias
#include "virtual-nes/imapper.h"

namespace virtualnes {
IMapper::IMapper(uint8_t prgBanks, uint8_t chrBanks)
    : m_PrgBanks{prgBanks}, m_ChrBanks{chrBanks} {}
CARTRIDGE_MIRRORING_MODE IMapper::GetMirroringMode() const {
    return CARTRIDGE_MIRRORING_MODE::HARDWARE_DEFINED;
}
}  // namespace virtualnes