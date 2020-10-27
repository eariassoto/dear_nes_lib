// Copyright (c) 2020 Emmanuel Arias
#pragma once
#include <cinttypes>
#include <string_view>

#include "virtual-nes/enums.h"

namespace cpuemulator {

// Forward declaration
class Bus;

/// <summary>
/// Virtual implementation of the 6502 CPU version for the NES. The instruction
/// set for this implementation is based on https://www.masswerk.at/6502/6502_instruction_set.html
/// The CPU can be reset using the Reset() function, and this will simulate the real
/// life version of the reset routine.
/// In this current implementation illegal instruction are not handled. There are some instruction
/// that do not crash the system, but they do originally undocumented operations. Some games
/// take advantage of this and will need to support said instructions.
/// </summary>
class Cpu {
   public:

    /// <summary>
    /// Set the reference to the memory bus
    /// </summary>
    /// <param name="bus"></param>
    void SetBus(Bus *bus);

    /// <summary>
    /// The RESET routine takes 8 cycles, and for our purposes
    /// it will simulate this process:
    /// * Zero registers A, X, Y and status flags
    /// * Load the program counter register by reading the address from
    ///   0xFFFC (low byte) and 0xFFFD (high byte)
    /// * Leave the stack pointer register in 0xFD
    /// For more info refer to: https://www.c64-wiki.com/wiki/Reset_(Process)
    /// </summary>
    void Reset();

    /// <summary>
    /// This function will read the opcode from the memory location pointed by the
    /// program counter register and execute the function. It will add the proper amount
    /// of cycles to fully simulate the cycle count.
    /// </summary>
    void Clock();

    /// <summary>
    /// Simulate the NMI (non maskable interruption) process. The IRL process is
    /// described in this wiki entry:
    /// https://wiki.nesdev.com/w/index.php/CPU_interrupts#IRQ_and_NMI_tick-by-tick_execution
    /// </summary>
    void NonMaskableInterrupt();

    /// <summary>
    /// Returns true if the current instruction has finished to wait for the cycles
    /// it takes to finish in the real hardware version
    /// </summary>
    /// <returns></returns>
    inline bool IsCurrentInstructionComplete() const { return m_Cycles == 0; }

    /// <summary>
    /// Return 0x01 or 0x01 for a given register flag
    /// </summary>
    /// <param name="flag"></param>
    /// <returns></returns>
    inline uint8_t GetFlag(CpuFlag flag) const {
        if ((m_StatusRegister & flag) == 0x00) {
            return 0;
        }
        return 1;
    }

    /// <summary>
    /// Set a register flag
    /// </summary>
    /// <param name="flag"></param>
    /// <param name="value"></param>
    inline void SetFlag(CpuFlag flag, bool value) {
        if (value) {
            m_StatusRegister |= flag;
        } else {
            m_StatusRegister &= ~flag;
        }
    }

    /// <summary>
    /// Get the value for register A
    /// </summary>
    /// <returns></returns>
    inline uint8_t GetRegisterA() const { return m_RegisterA; }
    
    /// <summary>
    /// Get the value for register X
    /// </summary>
    /// <returns></returns>
    inline uint8_t GetRegisterX() const { return m_RegisterX; }
    
    /// <summary>
    /// Get the value for register Y
    /// </summary>
    /// <returns></returns>
    inline uint8_t GetRegisterY() const { return m_RegisterY; }
    
    /// <summary>
    /// Get the value for the stack pointer register
    /// </summary>
    /// <returns></returns>
    inline uint8_t GetStackPointer() const { return m_StackPointer; }
    
    /// <summary>
    /// Get the value for the program counter register
    /// </summary>
    /// <returns></returns>
    inline uint16_t GetProgramCounter() const { return m_ProgramCounter; }

    /// <summary>
    /// Typedef for a void() function pointer. Used for the instruction table callbacks.
    /// </summary>
    using FuncPtr = void (Cpu::*)();

    /// <summary>
    /// Wrapper object for a item in the look-up table. It is meant to be used in
    /// a constexpr container.
    /// </summary>
    struct Instruction {
        /// <summary>
        /// Construct a constexpr item for the instruction set lookup table.
        /// The table will reference the operation code to the proper callbacks.
        /// </summary>
        /// <param name="name"></param>
        /// <param name="executeInstruction"></param>
        /// <param name="execureAddressingMode"></param>
        /// <param name="cycles"></param>
        constexpr Instruction(const std::string_view name,
                              const FuncPtr executeInstruction,
                              const FuncPtr execureAddressingMode,
                               const uint8_t cycles);
        /// <summary>
        /// Instruction's name for debugging purposes. At the moment it is not being used.
        /// </summary>
        std::string_view m_Name;
        
        /// <summary>
        /// Instruction callback. This is the virtual implementation of the instruction.
        /// </summary>
        const FuncPtr m_ExecuteInstruction;

        /// <summary>
        /// Virtual implementation for the memory addressing associated to the instruction.
        /// A instruction can have more than one op code, each associated to a specific
        /// addressing mode.
        /// </summary>
        const FuncPtr m_ExecureAddressingMode;

        /// <summary>
        /// Base cycle duration for the instruction.
        /// </summary>
        uint8_t m_Cycles;
    };

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

    bool m_AddressingModeNeedsAdditionalCycle = false;
    bool m_InstructionNeedsAdditionalCycle = false;

   private:
    uint8_t Read(uint16_t address);

    void Write(uint16_t address, uint8_t data);

    uint8_t ReadWordFromProgramCounter();

    uint16_t ReadDoubleWordFromProgramCounter();

    Instruction FindInstruction(const uint8_t opCode);

   private:
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
};

}  // namespace cpuemulator
