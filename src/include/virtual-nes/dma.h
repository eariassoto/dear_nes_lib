// Copyright (c) 2020 Emmanuel Arias
#pragma once
#include <cinttypes>
#include <utility>

namespace cpuemulator {

class Bus;

class Dma {
   public:
    void SetBus(Bus *bus);

    void StartTransfer(uint8_t data);
    void StopWaiting();

    void ReadData();
    std::pair<uint8_t, uint8_t> GetLastReadData();

    void FinishTransfer();
    void Reset();

    inline bool IsTranferInProgress() const { return m_DmaTransfer; }
    inline bool IsInWaitState() const { return m_DmaWait; }

   private:
    Bus *m_Bus = nullptr;
    uint8_t m_DmaPage = 0x00;
    uint8_t m_DmaAddress = 0x00;
    uint8_t m_DmaData = 0x00;

    bool m_DmaTransfer = false;
    bool m_DmaWait = true;
};

}  // namespace cpuemulator
