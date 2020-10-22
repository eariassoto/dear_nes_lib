// Copyright (c) 2020 Emmanuel Arias
#pragma once

#include <cstdint>

#include "include/bus.h"
#include "include/cpu.h"
#include "include/dma.h"
#include "include/ppu.h"

namespace cpuemulator {

class Cartridge;
class Ppu;

class Nes {

   public:
    Nes();
    ~Nes();

    uint64_t GetSystemClockCounter() const;

    void CpuWrite(uint16_t address, uint8_t data);
    uint8_t CpuRead(uint16_t address, bool isReadOnly = false);

    void InsertCatridge(Cartridge* cartridge);
    void Reset();
    void Clock();

    void DoFrame();

    bool IsCartridgeLoaded() const;

    // TODO: provide api and make it private
    Bus m_Bus;
    Dma m_Dma;
    Ppu m_Ppu;
    Cpu m_Cpu;

   private:

    Cartridge* m_Cartridge = nullptr;

    bool m_IsCartridgeLoaded = false;

    uint32_t m_SystemClockCounter = 0;

    uint8_t m_ControllerState[2] = {0};

    
};
}  // namespace cpuemulator