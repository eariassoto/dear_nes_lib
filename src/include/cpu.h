// Copyright (c) 2020 Emmanuel Arias
#pragma once
#include <array>
#include <cassert>
#include <cinttypes>
#include <functional>
#include <map>

namespace cpuemulator {

class Bus;

enum CpuFlag : uint8_t {
    C = (0b1 << 0),  // Carry Bit
    Z = (0b1 << 1),  // Zero
    I = (0b1 << 2),  // Disable Interrupts
    D = (0b1 << 3),  // Decimal Mode
    B = (0b1 << 4),  // Break
    U = (0b1 << 5),  // Unused
    V = (0b1 << 6),  // Overflow
    N = (0b1 << 7),  // Negative
};

class Cpu {
   public:
    Cpu();
    ~Cpu() = default;

    void SetBus(Bus *bus);

    // The RESET routine takes 8 cycles, and for our purposes
    // it will simulate this process:
    // * Zero registers A, X, Y and status flags
    // * Load the program counter register by reading the address from
    //   0xFFFC (low byte) and 0xFFFD (high byte)
    // * Leave the stack pointer register in 0xFD
    // For more info refer to: https://www.c64-wiki.com/wiki/Reset_(Process)
    void Reset();

    void Clock();

    void NonMaskableInterrupt();

    inline bool IsCurrentInstructionComplete() const { return m_Cycles == 0; }

    inline uint8_t GetFlag(CpuFlag flag) const {
        if ((m_StatusRegister & flag) == 0x00) {
            return 0;
        }
        return 1;
    }

    inline void SetFlag(CpuFlag flag, bool value) {
        if (value) {
            m_StatusRegister |= flag;
        } else {
            m_StatusRegister &= ~flag;
        }
    }

    inline uint8_t GetRegisterA() const { return m_RegisterA; }
    inline uint8_t GetRegisterX() const { return m_RegisterX; }
    inline uint8_t GetRegisterY() const { return m_RegisterY; }
    inline uint8_t GetStackPointer() const { return m_StackPointer; }
    inline uint16_t GetProgramCounter() const { return m_ProgramCounter; }

   private:
    Bus *m_Bus;

    uint8_t m_RegisterA = 0x00;
    uint8_t m_RegisterX = 0x00;
    uint8_t m_RegisterY = 0x00;

    uint8_t m_StackPointer = 0x00;

    uint8_t m_StatusRegister = 0x00;

    uint16_t m_ProgramCounter = 0x00;

    uint8_t m_CyclesToIdle = 0;
    uint64_t m_GlobalCycles = 0;

    uint16_t m_AddressAbsolute = 0x0000;
    uint16_t m_AddressRelative = 0x0000;

    uint8_t m_OpCode = 0x00;
    uint8_t m_Cycles = 0x00;

    struct Instruction {
        Instruction(const std::string &name,
                    std::function<void(Cpu *)> funcOperate,
                    std::function<void(Cpu *)> addressingMode, uint8_t cycles)
            : m_Name{name},
              m_FuncOperate{funcOperate},
              m_AddressingMode{addressingMode},
              m_Cycles{cycles} {}

        std::string m_Name;
        std::function<void(Cpu *)> m_FuncOperate;
        std::function<void(Cpu *)> m_AddressingMode;
        uint8_t m_Cycles = 0;
    };
    std::map<uint8_t, Instruction> m_InstrTable;

    bool m_AddressingModeNeedsAdditionalCycle = false;
    bool m_InstructionNeedsAdditionalCycle = false;

   private:
    uint8_t Read(uint16_t address);

    void Write(uint16_t address, uint8_t data);

    uint8_t ReadWordFromProgramCounter();

    uint16_t ReadDoubleWordFromProgramCounter();

    void ImmediateAddressing();

    void ZeroPageAddressing();

    void IndexedZeroPageAddressingX();

    void IndexedZeroPageAddressingY();

    void AbsoluteAddressing();

    void IndexedAbsoluteAddressingX();

    void IndexedAbsoluteAddressingY();

    void AbsoluteIndirectAddressing();

    void IndexedIndirectAddressingX();

    void IndirectIndexedAddressingY();

    void RelativeAddressing();

    void Instruction_ADC();

    void Instruction_AND();

    void Instruction_ASL();

    void Instruction_ASL_AcummAddr();

    void Instruction_ExecuteBranch();

    void Instruction_BCC();

    void Instruction_BCS();

    void Instruction_BEQ();

    void Instruction_BIT();

    void Instruction_BMI();

    void Instruction_BNE();

    void Instruction_BPL();

    void Instruction_BRK();

    void Instruction_BVC();

    void Instruction_BVS();

    void Instruction_CLC();

    void Instruction_CLD();

    void Instruction_CLI();

    void Instruction_CLV();

    void Instruction_CMP();

    void Instruction_CPX();

    void Instruction_CPY();

    void Instruction_DEC();

    void Instruction_DEX();

    void Instruction_DEY();

    void Instruction_EOR();

    void Instruction_INC();

    void Instruction_INX();

    void Instruction_INY();

    void Instruction_JMP();

    void Instruction_JSR();

    void Instruction_LDA();

    void Instruction_LDX();

    void Instruction_LDY();

    void Instruction_LSR();

    void Instruction_LSR_AcummAddr();

    void Instruction_NOP();

    void Instruction_ORA();

    void Instruction_PHA();

    void Instruction_PHP();

    void Instruction_PLA();

    void Instruction_PLP();

    void Instruction_ROL();

    void Instruction_ROL_AcummAddr();

    void Instruction_ROR();

    void Instruction_ROR_AcummAddr();

    void Instruction_RTI();

    void Instruction_RTS();

    void Instruction_SBC();

    void Instruction_SEC();

    void Instruction_SED();

    void Instruction_SEI();

    void Instruction_STA();

    void Instruction_STX();

    void Instruction_STY();

    void Instruction_TAX();

    void Instruction_TAY();

    void Instruction_TSX();

    void Instruction_TXA();

    void Instruction_TXS();

    void Instruction_TYA();

    void RegisterAllInstructionSet();
};

}  // namespace cpuemulator
