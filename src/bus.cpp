// Copyright (c) 2020 Emmanuel Arias
#include "include/bus.h"

#include <cstring>
#include <cassert>

#include "include/cartridge.h"
#include "include/dma.h"
#include "include/ppu.h"

namespace cpuemulator {

Bus::Bus() { memset(m_CpuRam, 0, 0x800); }

Bus::~Bus() { delete[] m_CpuRam; }

void Bus::SetCartridge(Cartridge* cartridge) {
    assert(m_Cartridge != nullptr);
    m_Cartridge = cartridge;
}

void Bus::SetDma(Dma* dma) {
    assert(m_Dma != nullptr);
    m_Dma = dma;
}

void Bus::SetPpu(Ppu* ppu) {
    assert(m_Ppu != nullptr);
    m_Ppu = ppu;
}

void Bus::CpuWrite(uint16_t address, uint8_t data) {
    if (m_Cartridge->CpuWrite(address, data)) {
    } else if (address >= 0x0000 && address <= 0x1FFF) {
        m_CpuRam[GetRealRamAddress(address)] = data;
    } else if (address >= 0x2000 && address <= 0x3FFF) {
        m_Ppu->CpuWrite(GetRealPpuAddress(address), data);
    } else if (address == 0x4014) {
        m_Dma->StartTransfer(data);
    } else if (address >= 0x4016 && address <= 0x4017) {
        m_ControllerState[address & 0x0001] = m_Controllers[address & 0x0001];
    }
}

uint8_t Bus::CpuRead(uint16_t address, bool isReadOnly) {
    uint8_t data = 0x00;
    if (m_Cartridge->CpuRead(address, data)) {
    } else if (address >= 0x0000 && address <= 0x1FFF) {
        data = m_CpuRam[GetRealRamAddress(address)];
    } else if (address >= 0x2000 && address <= 0x3FFF) {
        data = m_Ppu->CpuRead(GetRealPpuAddress(address), isReadOnly);
    } else if (address >= 0x4016 && address <= 0x4017) {
        data = (m_ControllerState[address & 0x0001] & 0x80) > 0;
        m_ControllerState[address & 0x0001] <<= 1;
    }

    return data;
}

}  // namespace cpuemulator