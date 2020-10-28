// Copyright (c) 2020 Emmanuel Arias
#include "virtual-nes/mapper.h"

namespace virtualnes {
IMapper::IMapper(uint8_t prgBanks, uint8_t chrBanks)
    : m_PrgBanks{prgBanks}, m_ChrBanks{chrBanks} {}
}  // namespace virtualnes