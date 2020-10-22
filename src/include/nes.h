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

    uint8_t GetControllerState(size_t controllerIdx) const;
    void ClearControllerState(size_t controllerIdx);
    void WriteControllerState(size_t controllerIdx, uint8_t data);

    // TODO: provide api and make it private
    inline Ppu* GetPpu() { return &m_Ppu; }
    inline Cpu* GetCpu() { return &m_Cpu; }

   private:
    Bus m_Bus;
    Dma m_Dma;
    Ppu m_Ppu;
    Cpu m_Cpu;

    Cartridge* m_Cartridge = nullptr;

    bool m_IsCartridgeLoaded = false;

    uint32_t m_SystemClockCounter = 0;
};
}  // namespace cpuemulator