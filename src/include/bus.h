// Copyright (c) 2020 Emmanuel Arias
#pragma once
#include <cinttypes>
#include "include/enums.h"

namespace cpuemulator {

class Cartridge;
class Dma;
class Ppu;

class Bus {
   public:
    Bus();
    ~Bus();

    void CpuWrite(uint16_t address, uint8_t data);
    uint8_t CpuRead(uint16_t address, bool isReadOnly = false);

    void SetCartridge(Cartridge* cartridge);
    void SetDma(Dma* dma);
    void SetPpu(Ppu* ppu);

    // TODO: Provide better controller API
    uint8_t GetControllerState(size_t controllerIdx) const;
    void ClearControllerState(size_t controllerIdx);
    void WriteControllerState(size_t controllerIdx, uint8_t data);

   private:
    Cartridge* m_Cartridge = nullptr;
    Dma* m_Dma = nullptr;
    Ppu* m_Ppu = nullptr;

    uint8_t m_Controllers[NUM_CONTROLLERS] = {0};
    uint8_t m_ControllerState[NUM_CONTROLLERS] = {0};

    uint8_t* m_CpuRam = new uint8_t[SIZE_CPU_RAM];

    inline uint16_t GetRealRamAddress(uint16_t address) const {
        return address & 0x07FF;
    }
    inline uint16_t GetRealPpuAddress(uint16_t address) const {
        return address & 0x0007;
    }
};

}  // namespace cpuemulator
