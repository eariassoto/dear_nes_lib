// Copyright (c) 2020 Emmanuel Arias
#include "virtual-nes/mapper.h"

namespace cpuemulator {
Mapper::Mapper(uint8_t prgBanks, uint8_t chrBanks)
    : m_PrgBanks{prgBanks}, m_ChrBanks{chrBanks} {}
}  // namespace cpuemulator