// Copyright (c) 2020 Emmanuel Arias
#include "virtual-nes/nes.h"

#include <cassert>
#include <iostream>

#include "virtual-nes/cartridge.h"
#include "virtual-nes/enums.h"

namespace cpuemulator {

Nes::Nes() {
    m_Bus.SetPpu(&m_Ppu);
    m_Bus.SetDma(&m_Dma);

    m_Dma.SetBus(&m_Bus);

    m_Cpu.SetBus(&m_Bus);
}

Nes::~Nes() {
    if (m_Cartridge != nullptr) {
        delete m_Cartridge;
    }
}

uint64_t Nes::GetSystemClockCounter() const { return m_SystemClockCounter; }

void Nes::InsertCatridge(Cartridge* cartridge) {
    // TODO: Loading more than one cartridge will leam memory.
    // Consider having a cartridge manager that will handle/swap
    // the cartridge data

    // Logger::Get().Log("BUS", "Inserting cartridge");
    m_Cartridge = cartridge;
    m_Bus.SetCartridge(cartridge);
    m_Ppu.ConnectCatridge(cartridge);
    m_IsCartridgeLoaded = true;
}

void Nes::Reset() {
    m_Cpu.Reset();
    m_SystemClockCounter = 0;
}

void Nes::Clock() {
    m_Ppu.Clock();
    if (m_SystemClockCounter % 3 == 0) {
        if (m_Dma.IsTranferInProgress()) {
            if (m_Dma.IsInWaitState()) {
                if (m_SystemClockCounter % 2 == 1) {
                    m_Dma.StopWaiting();
                }
            } else {
                if (m_SystemClockCounter % 2 == 0) {
                    m_Dma.ReadData();
                } else {
                    // todo offer api function
                    auto [addr, data] = m_Dma.GetLastReadData();
                    m_Ppu.m_OAMPtr[addr] = data;
                }
            }
        } else {
            m_Cpu.Clock();
        }
    }

    if (m_Ppu.NeedsToDoNMI()) {
        m_Cpu.NonMaskableInterrupt();
    }

    ++m_SystemClockCounter;
}

void Nes::DoFrame() {
    do {
        Clock();
    } while (!m_Ppu.IsFrameCompleted());

    do {
        m_Cpu.Clock();
    } while (m_Cpu.IsCurrentInstructionComplete());

    m_Ppu.StartNewFrame();
}

bool Nes::IsCartridgeLoaded() const { return m_IsCartridgeLoaded; }

uint8_t Nes::GetControllerState(size_t controllerIdx) const {
    assert(controllerIdx < NUM_CONTROLLERS);
    return m_Bus.GetControllerState(controllerIdx);
}

void Nes::ClearControllerState(size_t controllerIdx) {
    assert(controllerIdx < NUM_CONTROLLERS);
    m_Bus.ClearControllerState(controllerIdx);
}

void Nes::WriteControllerState(size_t controllerIdx, uint8_t data) {
    assert(controllerIdx < NUM_CONTROLLERS);
    m_Bus.WriteControllerState(controllerIdx, data);
}

}  // namespace cpuemulator
