// Copyright (c) 2020 Emmanuel Arias
#include "include/cpu.h"

#include <array>
#include <cassert>

#include "include/array_map.h"
#include "include/bus.h"

namespace cpuemulator {

void Cpu::SetBus(Bus* bus) {
    assert(bus != nullptr);
    m_Bus = bus;
}

void Cpu::Reset() {
    m_RegisterA = 0;
    m_RegisterX = 0;
    m_RegisterY = 0;
    m_StatusRegister = 0x00 | CpuFlag::U;

    static constexpr uint16_t addressToReadPC = 0xFFFC;
    uint16_t lo = m_Bus->CpuRead(addressToReadPC);
    uint16_t hi = m_Bus->CpuRead(addressToReadPC + 1);
    m_ProgramCounter = (hi << 8) | lo;

    m_StackPointer = 0xFD;

    m_Cycles = 8;
}

void Cpu::Clock() {
    if (m_Cycles == 0) {
        m_OpCode = ReadWordFromProgramCounter();

        SetFlag(CpuFlag::U, 1);

        auto instr2 = FindInstruction(m_OpCode);

        m_Cycles = instr2.m_Cycles;

        if (instr2.m_ExecureAddressingMode) {
            (this->*instr2.m_ExecureAddressingMode)();
        }

        (this->*instr2.m_ExecuteInstruction)();

        if (m_AddressingModeNeedsAdditionalCycle &&
            m_InstructionNeedsAdditionalCycle) {
            ++m_Cycles;
        }

        SetFlag(U, true);
    }

    m_Cycles--;
}

void Cpu::NonMaskableInterrupt() {
    Write(0x0100 + m_StackPointer, (m_ProgramCounter >> 8) & 0x00FF);
    m_StackPointer--;
    Write(0x0100 + m_StackPointer, m_ProgramCounter & 0x00FF);
    m_StackPointer--;

    SetFlag(B, 0);
    SetFlag(U, 1);
    SetFlag(I, 1);
    Write(0x0100 + m_StackPointer, m_StatusRegister);
    m_StackPointer--;

    uint16_t addresToReadPC = 0xFFFA;
    uint16_t lo = Read(addresToReadPC + 0);
    uint16_t hi = Read(addresToReadPC + 1);
    m_ProgramCounter = (hi << 8) | lo;

    m_Cycles = 8;
}

uint8_t Cpu::Read(uint16_t address) { return m_Bus->CpuRead(address); }

void Cpu::Write(uint16_t address, uint8_t data) {
    m_Bus->CpuWrite(address, data);
}

uint8_t Cpu::ReadWordFromProgramCounter() {
    return m_Bus->CpuRead(m_ProgramCounter++);
}

uint16_t Cpu::ReadDoubleWordFromProgramCounter() {
    uint16_t lowNibble = ReadWordFromProgramCounter();
    uint16_t highNibble = ReadWordFromProgramCounter();

    return (highNibble << 8) | lowNibble;
}

void Cpu::ImmediateAddressing() { m_AddressAbsolute = m_ProgramCounter++; }

void Cpu::ZeroPageAddressing() {
    m_AddressAbsolute = ReadWordFromProgramCounter();
    m_AddressAbsolute &= 0x00FF;
}

void Cpu::IndexedZeroPageAddressingX() {
    m_AddressAbsolute = ReadWordFromProgramCounter();
    m_AddressAbsolute += m_RegisterX;
    m_AddressAbsolute &= 0x00FF;
}

void Cpu::IndexedZeroPageAddressingY() {
    m_AddressAbsolute = ReadWordFromProgramCounter();
    m_AddressAbsolute += m_RegisterY;
    m_AddressAbsolute &= 0x00FF;
}

void Cpu::AbsoluteAddressing() {
    m_AddressAbsolute = ReadDoubleWordFromProgramCounter();
}

void Cpu::IndexedAbsoluteAddressingX() {
    m_AddressAbsolute = ReadDoubleWordFromProgramCounter();
    uint16_t highNibble = m_AddressAbsolute & 0xFF00;

    m_AddressAbsolute += m_RegisterX;

    m_AddressingModeNeedsAdditionalCycle =
        (m_AddressAbsolute & 0xFF00) != highNibble;
}

void Cpu::IndexedAbsoluteAddressingY() {
    m_AddressAbsolute = ReadDoubleWordFromProgramCounter();
    uint16_t highNibble = m_AddressAbsolute & 0xFF00;

    m_AddressAbsolute += m_RegisterY;

    m_AddressingModeNeedsAdditionalCycle =
        (m_AddressAbsolute & 0xFF00) != highNibble;
}

void Cpu::AbsoluteIndirectAddressing() {
    uint16_t pointer = ReadDoubleWordFromProgramCounter();

    // Simulate page boundary hardware bug
    if ((pointer & 0x00FF) == 0x00FF) {
        m_AddressAbsolute = (Read(pointer & 0xFF00) << 8) | Read(pointer);
    } else {
        m_AddressAbsolute = (Read(pointer + 1) << 8) | Read(pointer);
    }
}

void Cpu::IndexedIndirectAddressingX() {
    uint16_t t = ReadWordFromProgramCounter();

    uint16_t registerXValue = static_cast<uint16_t>(m_RegisterX);

    uint16_t lowNibbleAddress = (t + registerXValue) & 0x00FF;
    uint16_t lowNibble = Read(lowNibbleAddress);

    uint16_t highNibbleAddress = (t + registerXValue + 1) & 0x00FF;
    uint16_t highNibble = Read(highNibbleAddress);

    m_AddressAbsolute = (highNibble << 8) | lowNibble;
}

void Cpu::IndirectIndexedAddressingY() {
    uint16_t t = ReadWordFromProgramCounter();

    uint16_t lowNibble = Read(t & 0x00FF);
    uint16_t highNibble = Read((t + 1) & 0x00FF);

    m_AddressAbsolute = (highNibble << 8) | lowNibble;
    m_AddressAbsolute += m_RegisterY;

    m_AddressingModeNeedsAdditionalCycle =
        (m_AddressAbsolute & 0xFF00) != (highNibble << 8);
}

void Cpu::RelativeAddressing() {
    m_AddressRelative = ReadWordFromProgramCounter();

    if (m_AddressRelative & 0x80)  // if unsigned
    {
        m_AddressRelative |= 0xFF00;
    }
}

void Cpu::Instruction_ADC() {
    uint8_t valueFetched = Read(m_AddressAbsolute);

    uint16_t castedFetched = static_cast<uint16_t>(valueFetched);
    uint16_t castedCarry = static_cast<uint16_t>(GetFlag(CpuFlag::C));
    uint16_t castedAccum = static_cast<uint16_t>(m_RegisterA);

    uint16_t temp = castedAccum + castedFetched + castedCarry;
    SetFlag(CpuFlag::C, temp > 255);
    SetFlag(CpuFlag::Z, (temp & 0x00FF) == 0);
    SetFlag(CpuFlag::N, temp & 0x80);
    SetFlag(V,
            (~(castedAccum ^ castedFetched) & (castedAccum ^ temp)) & 0x0080);

    m_RegisterA = temp & 0x00FF;

    m_InstructionNeedsAdditionalCycle = true;
}

void Cpu::Instruction_AND() {
    uint8_t valueFetched = Read(m_AddressAbsolute);

    m_RegisterA &= valueFetched;
    SetFlag(CpuFlag::Z, m_RegisterA == 0x00);
    SetFlag(CpuFlag::N, m_RegisterA & 0x80);

    m_InstructionNeedsAdditionalCycle = true;
}

void Cpu::Instruction_ASL() {
    uint8_t valueFetched = Read(m_AddressAbsolute);

    uint16_t temp = static_cast<uint16_t>(valueFetched) << 1;
    SetFlag(CpuFlag::C, (temp & 0xFF00) > 0);
    SetFlag(CpuFlag::Z, (temp & 0x00FF) == 0);
    SetFlag(CpuFlag::N, temp & 0x80);

    Write(m_AddressAbsolute, temp & 0x00FF);
}

void Cpu::Instruction_ASL_AcummAddr() {
    uint16_t temp = static_cast<uint16_t>(m_RegisterA) << 1;
    SetFlag(CpuFlag::C, (temp & 0xFF00) > 0);
    SetFlag(CpuFlag::Z, (temp & 0x00FF) == 0);
    SetFlag(CpuFlag::N, temp & 0x80);

    m_RegisterA = temp & 0x00FF;
}

void Cpu::Instruction_ExecuteBranch() {
    m_Cycles++;
    m_AddressAbsolute = m_ProgramCounter + m_AddressRelative;

    if ((m_AddressAbsolute & 0xFF00) != (m_ProgramCounter & 0xFF00)) {
        m_Cycles++;
    }
    m_ProgramCounter = m_AddressAbsolute;
}

void Cpu::Instruction_BCC() {
    if (GetFlag(CpuFlag::C) == 0) {
        Instruction_ExecuteBranch();
    }
}

void Cpu::Instruction_BCS() {
    if (GetFlag(CpuFlag::C) == 1) {
        Instruction_ExecuteBranch();
    }
}

void Cpu::Instruction_BEQ() {
    if (GetFlag(CpuFlag::Z) == 1) {
        Instruction_ExecuteBranch();
    }
}

void Cpu::Instruction_BIT() {
    uint8_t valueFetched = Read(m_AddressAbsolute);

    uint16_t temp = m_RegisterA & valueFetched;
    SetFlag(Z, (temp & 0x00FF) == 0x00);
    SetFlag(N, valueFetched & (1 << 7));
    SetFlag(V, valueFetched & (1 << 6));
}

void Cpu::Instruction_BMI() {
    if (GetFlag(CpuFlag::N) == 1) {
        Instruction_ExecuteBranch();
    }
}

void Cpu::Instruction_BNE() {
    if (GetFlag(CpuFlag::Z) == 0) {
        Instruction_ExecuteBranch();
    }
}

void Cpu::Instruction_BPL() {
    if (GetFlag(CpuFlag::N) == 0) {
        Instruction_ExecuteBranch();
    }
}

void Cpu::Instruction_BRK() {
    m_ProgramCounter++;

    SetFlag(I, 1);
    Write(0x0100 + m_StackPointer, (m_ProgramCounter >> 8) & 0x00FF);
    m_StackPointer--;
    Write(0x0100 + m_StackPointer, m_ProgramCounter & 0x00FF);
    m_StackPointer--;

    SetFlag(CpuFlag::B, 1);
    Write(0x0100 + m_StackPointer, m_StatusRegister);
    m_StackPointer--;
    SetFlag(B, 0);

    m_ProgramCounter = static_cast<uint16_t>(Read(0xFFFE)) |
                       (static_cast<uint16_t>(Read(0xFFFF)) << 8);
}

void Cpu::Instruction_BVC() {
    if (GetFlag(CpuFlag::V) == 0) {
        Instruction_ExecuteBranch();
    }
}

void Cpu::Instruction_BVS() {
    if (GetFlag(CpuFlag::V) == 1) {
        Instruction_ExecuteBranch();
    }
}

void Cpu::Instruction_CLC() { SetFlag(CpuFlag::C, false); }

void Cpu::Instruction_CLD() { SetFlag(CpuFlag::D, false); }

void Cpu::Instruction_CLI() { SetFlag(CpuFlag::I, false); }

void Cpu::Instruction_CLV() { SetFlag(CpuFlag::V, false); }

void Cpu::Instruction_CMP() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    uint16_t temp = static_cast<uint16_t>(m_RegisterA) -
                    static_cast<uint16_t>(valueFetched);
    SetFlag(C, m_RegisterA >= valueFetched);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    m_InstructionNeedsAdditionalCycle = true;
}

void Cpu::Instruction_CPX() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    uint16_t temp = static_cast<uint16_t>(m_RegisterX) -
                    static_cast<uint16_t>(valueFetched);
    SetFlag(C, m_RegisterX >= valueFetched);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
}

void Cpu::Instruction_CPY() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    uint16_t temp = static_cast<uint16_t>(m_RegisterY) -
                    static_cast<uint16_t>(valueFetched);
    SetFlag(C, m_RegisterY >= valueFetched);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
}

void Cpu::Instruction_DEC() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    uint16_t temp = valueFetched - 1;
    Write(m_AddressAbsolute, temp & 0x00FF);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
}

void Cpu::Instruction_DEX() {
    m_RegisterX--;
    SetFlag(Z, m_RegisterX == 0x00);
    SetFlag(N, m_RegisterX & 0x80);
}

void Cpu::Instruction_DEY() {
    m_RegisterY--;
    SetFlag(Z, m_RegisterY == 0x00);
    SetFlag(N, m_RegisterY & 0x80);
}

void Cpu::Instruction_EOR() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    m_RegisterA = m_RegisterA ^ valueFetched;
    SetFlag(Z, m_RegisterA == 0x00);
    SetFlag(N, m_RegisterA & 0x80);
    m_InstructionNeedsAdditionalCycle = true;
}

void Cpu::Instruction_INC() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    uint16_t temp = valueFetched + 1;
    Write(m_AddressAbsolute, temp & 0x00FF);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
}

void Cpu::Instruction_INX() {
    m_RegisterX++;
    SetFlag(Z, m_RegisterX == 0x00);
    SetFlag(N, m_RegisterX & 0x80);
}

void Cpu::Instruction_INY() {
    m_RegisterY++;
    SetFlag(Z, m_RegisterY == 0x00);
    SetFlag(N, m_RegisterY & 0x80);
}

void Cpu::Instruction_JMP() { m_ProgramCounter = m_AddressAbsolute; }

void Cpu::Instruction_JSR() {
    m_ProgramCounter--;

    Write(0x0100 + m_StackPointer, (m_ProgramCounter >> 8) & 0x00FF);
    m_StackPointer--;
    Write(0x0100 + m_StackPointer, m_ProgramCounter & 0x00FF);
    m_StackPointer--;

    m_ProgramCounter = m_AddressAbsolute;
}

void Cpu::Instruction_LDA() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    m_RegisterA = valueFetched;
    SetFlag(Z, m_RegisterA == 0x00);
    SetFlag(N, m_RegisterA & 0x80);
    m_InstructionNeedsAdditionalCycle = true;
}

void Cpu::Instruction_LDX() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    m_RegisterX = valueFetched;
    SetFlag(Z, m_RegisterX == 0x00);
    SetFlag(N, m_RegisterX & 0x80);
    m_InstructionNeedsAdditionalCycle = true;
}

void Cpu::Instruction_LDY() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    m_RegisterY = valueFetched;
    SetFlag(Z, m_RegisterY == 0x00);
    SetFlag(N, m_RegisterY & 0x80);
    m_InstructionNeedsAdditionalCycle = true;
}

void Cpu::Instruction_LSR() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    SetFlag(C, valueFetched & 0x0001);
    uint16_t temp = valueFetched >> 1;
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);

    Write(m_AddressAbsolute, temp & 0x00FF);
}

void Cpu::Instruction_LSR_AcummAddr() {
    SetFlag(C, m_RegisterA & 0x0001);
    uint16_t temp = m_RegisterA >> 1;
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);

    m_RegisterA = temp & 0x00FF;
}

void Cpu::Instruction_NOP() {}

void Cpu::Instruction_ORA() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    m_RegisterA = m_RegisterA | valueFetched;
    SetFlag(Z, m_RegisterA == 0x00);
    SetFlag(N, m_RegisterA & 0x80);
    m_InstructionNeedsAdditionalCycle = true;
}

void Cpu::Instruction_PHA() {
    Write(0x0100 + m_StackPointer, m_RegisterA);
    m_StackPointer--;
}

void Cpu::Instruction_PHP() {
    Write(0x0100 + m_StackPointer, m_StatusRegister | B | U);
    SetFlag(B, 0);
    SetFlag(U, 0);
    m_StackPointer--;
}

void Cpu::Instruction_PLA() {
    m_StackPointer++;
    m_RegisterA = Read(0x0100 + m_StackPointer);
    SetFlag(Z, m_RegisterA == 0x00);
    SetFlag(N, m_RegisterA & 0x80);
}

void Cpu::Instruction_PLP() {
    m_StackPointer++;
    m_StatusRegister = Read(0x0100 + m_StackPointer);
    SetFlag(U, 1);
}

void Cpu::Instruction_ROL() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    uint16_t temp = static_cast<uint16_t>(valueFetched << 1) | GetFlag(C);
    SetFlag(C, temp & 0xFF00);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);

    Write(m_AddressAbsolute, temp & 0x00FF);
}

void Cpu::Instruction_ROL_AcummAddr() {
    uint16_t temp = static_cast<uint16_t>(m_RegisterA << 1) | GetFlag(C);
    SetFlag(C, temp & 0xFF00);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);

    m_RegisterA = temp & 0x00FF;
}

void Cpu::Instruction_ROR() {
    uint8_t valueFetched = Read(m_AddressAbsolute);
    uint16_t temp =
        static_cast<uint16_t>(GetFlag(C) << 7) | (valueFetched >> 1);
    SetFlag(C, valueFetched & 0x01);
    SetFlag(Z, (temp & 0x00FF) == 0x00);
    SetFlag(N, temp & 0x0080);

    Write(m_AddressAbsolute, temp & 0x00FF);
}
void Cpu::Instruction_ROR_AcummAddr() {
    uint16_t temp = static_cast<uint16_t>(GetFlag(C) << 7) | (m_RegisterA >> 1);
    SetFlag(C, m_RegisterA & 0x01);
    SetFlag(Z, (temp & 0x00FF) == 0x00);
    SetFlag(N, temp & 0x0080);

    m_RegisterA = temp & 0x00FF;
}

void Cpu::Instruction_RTI() {
    m_StackPointer++;
    m_StatusRegister = Read(0x0100 + m_StackPointer);
    m_StatusRegister &= ~B;
    m_StatusRegister &= ~U;

    m_StackPointer++;
    m_ProgramCounter = static_cast<uint16_t>(Read(0x0100 + m_StackPointer));
    m_StackPointer++;
    m_ProgramCounter |= static_cast<uint16_t>(Read(0x0100 + m_StackPointer))
                        << 8;
}

void Cpu::Instruction_RTS() {
    m_StackPointer++;
    m_ProgramCounter = static_cast<uint16_t>(Read(0x0100 + m_StackPointer));
    m_StackPointer++;
    m_ProgramCounter |= static_cast<uint16_t>(Read(0x0100 + m_StackPointer))
                        << 8;

    m_ProgramCounter++;
}

void Cpu::Instruction_SBC() {
    uint8_t valueFetched = Read(m_AddressAbsolute);

    // Operating in 16-bit domain to capture carry out

    // We can invert the bottom 8 bits with bitwise xor
    uint16_t value = static_cast<uint16_t>(valueFetched) ^ 0x00FF;

    // Notice this is exactly the same as addition from here!
    uint16_t temp = static_cast<uint16_t>(m_RegisterA) + value +
                    static_cast<uint16_t>(GetFlag(C));
    SetFlag(C, temp & 0xFF00);
    SetFlag(Z, ((temp & 0x00FF) == 0));
    SetFlag(V, (temp ^ static_cast<uint16_t>(m_RegisterA)) & (temp ^ value) &
                   0x0080);
    SetFlag(N, temp & 0x0080);
    m_RegisterA = temp & 0x00FF;
    m_InstructionNeedsAdditionalCycle = true;
}

void Cpu::Instruction_SEC() { SetFlag(C, true); }

void Cpu::Instruction_SED() { SetFlag(D, true); }

void Cpu::Instruction_SEI() { SetFlag(I, true); }

void Cpu::Instruction_STA() { Write(m_AddressAbsolute, m_RegisterA); }

void Cpu::Instruction_STX() { Write(m_AddressAbsolute, m_RegisterX); }

void Cpu::Instruction_STY() { Write(m_AddressAbsolute, m_RegisterY); }

void Cpu::Instruction_TAX() {
    m_RegisterX = m_RegisterA;
    SetFlag(Z, m_RegisterX == 0x00);
    SetFlag(N, m_RegisterX & 0x80);
}

void Cpu::Instruction_TAY() {
    m_RegisterY = m_RegisterA;
    SetFlag(Z, m_RegisterY == 0x00);
    SetFlag(N, m_RegisterY & 0x80);
}

void Cpu::Instruction_TSX() {
    m_RegisterX = m_StackPointer;
    SetFlag(Z, m_RegisterX == 0x00);
    SetFlag(N, m_RegisterX & 0x80);
}

void Cpu::Instruction_TXA() {
    m_RegisterA = m_RegisterX;
    SetFlag(Z, m_RegisterA == 0x00);
    SetFlag(N, m_RegisterA & 0x80);
}

void Cpu::Instruction_TXS() { m_StackPointer = m_RegisterX; }

void Cpu::Instruction_TYA() {
    m_RegisterA = m_RegisterY;
    SetFlag(Z, m_RegisterA == 0x00);
    SetFlag(N, m_RegisterA & 0x80);
}

constexpr Cpu::Instruction::Instruction(const std::string_view name,
                                        const FuncPtr executeInstruction,
                                const FuncPtr execureAddressingMode,
                                const uint8_t cycles)
    : m_Name{name},
      m_ExecuteInstruction{executeInstruction},
      m_ExecureAddressingMode{execureAddressingMode},
      m_Cycles{cycles} {}

using namespace std::literals::string_view_literals;

Cpu::Instruction Cpu::FindInstruction(const uint8_t opCode) {
    static constexpr std::array<std::pair<uint8_t, Cpu::Instruction>, 151>
        lookupTable{
            {{0x00,
              Cpu::Instruction("BRK"sv, &Cpu::Instruction_BRK, nullptr, 7)},
             {0x01, Cpu::Instruction("ORA"sv, &Cpu::Instruction_ORA,
                                      &Cpu::IndexedIndirectAddressingX, 6)},
             {0x05, Cpu::Instruction("ORA"sv, &Cpu::Instruction_ORA,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0x06, Cpu::Instruction("ASL"sv, &Cpu::Instruction_ASL,
                                      &Cpu::ZeroPageAddressing, 5)},
             {0x08,
              Cpu::Instruction("PHP"sv, &Cpu::Instruction_PHP, nullptr, 3)},
             {0x09, Cpu::Instruction("ORA"sv, &Cpu::Instruction_ORA,
                                      &Cpu::ImmediateAddressing, 2)},
             {0x0A, Cpu::Instruction("ASL"sv, &Cpu::Instruction_ASL_AcummAddr,
                                      nullptr, 2)},
             {0x0D, Cpu::Instruction("ORA"sv, &Cpu::Instruction_ORA,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0x0E, Cpu::Instruction("ASL"sv, &Cpu::Instruction_ASL,
                                      &Cpu::AbsoluteAddressing, 6)},
             {0x10, Cpu::Instruction("BPL"sv, &Cpu::Instruction_BPL,
                                      &Cpu::RelativeAddressing, 2)},
             {0x11, Cpu::Instruction("ORA"sv, &Cpu::Instruction_ORA,
                                      &Cpu::IndirectIndexedAddressingY, 5)},
             {0x15, Cpu::Instruction("ORA"sv, &Cpu::Instruction_ORA,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0x16, Cpu::Instruction("ASL"sv, &Cpu::Instruction_ASL,
                                      &Cpu::IndexedZeroPageAddressingX, 6)},
             {0x18,
              Cpu::Instruction("CLC"sv, &Cpu::Instruction_CLC, nullptr, 2)},
             {0x19, Cpu::Instruction("ORA"sv, &Cpu::Instruction_ORA,
                                      &Cpu::IndexedAbsoluteAddressingY, 4)},
             {0x1D, Cpu::Instruction("ORA"sv, &Cpu::Instruction_ORA,
                                      &Cpu::IndexedAbsoluteAddressingX, 4)},
             {0x1E, Cpu::Instruction("ASL"sv, &Cpu::Instruction_ASL,
                                      &Cpu::IndexedAbsoluteAddressingX, 7)},
             {0x20, Cpu::Instruction("JSR"sv, &Cpu::Instruction_JSR,
                                      &Cpu::AbsoluteAddressing, 6)},
             {0x21, Cpu::Instruction("AND"sv, &Cpu::Instruction_AND,
                                      &Cpu::IndexedIndirectAddressingX, 6)},
             {0x24, Cpu::Instruction("BIT"sv, &Cpu::Instruction_BIT,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0x25, Cpu::Instruction("AND"sv, &Cpu::Instruction_AND,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0x26, Cpu::Instruction("ROL"sv, &Cpu::Instruction_ROL,
                                      &Cpu::ZeroPageAddressing, 5)},
             {0x28,
              Cpu::Instruction("PLP"sv, &Cpu::Instruction_PLP, nullptr, 4)},
             {0x29, Cpu::Instruction("AND"sv, &Cpu::Instruction_AND,
                                      &Cpu::ImmediateAddressing, 2)},
             {0x2A, Cpu::Instruction("ROL"sv, &Cpu::Instruction_ROL_AcummAddr,
                                      nullptr, 2)},
             {0x2C, Cpu::Instruction("BIT"sv, &Cpu::Instruction_BIT,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0x2D, Cpu::Instruction("AND"sv, &Cpu::Instruction_AND,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0x2E, Cpu::Instruction("ROL"sv, &Cpu::Instruction_ROL,
                                      &Cpu::AbsoluteAddressing, 6)},
             {0x30, Cpu::Instruction("BMI"sv, &Cpu::Instruction_BMI,
                                      &Cpu::RelativeAddressing, 2)},
             {0x31, Cpu::Instruction("AND"sv, &Cpu::Instruction_AND,
                                      &Cpu::IndirectIndexedAddressingY, 5)},
             {0x35, Cpu::Instruction("AND"sv, &Cpu::Instruction_AND,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0x36, Cpu::Instruction("ROL"sv, &Cpu::Instruction_ROL,
                                      &Cpu::IndexedZeroPageAddressingX, 6)},
             {0x38,
              Cpu::Instruction("SEC"sv, &Cpu::Instruction_SEC, nullptr, 2)},
             {0x39, Cpu::Instruction("AND"sv, &Cpu::Instruction_AND,
                                      &Cpu::IndexedAbsoluteAddressingY, 4)},
             {0x3D, Cpu::Instruction("AND"sv, &Cpu::Instruction_AND,
                                      &Cpu::IndexedAbsoluteAddressingX, 4)},
             {0x3E, Cpu::Instruction("ROL"sv, &Cpu::Instruction_ROL,
                                      &Cpu::IndexedAbsoluteAddressingX, 7)},
             {0x40,
              Cpu::Instruction("RTI"sv, &Cpu::Instruction_RTI, nullptr, 6)},
             {0x41, Cpu::Instruction("EOR"sv, &Cpu::Instruction_EOR,
                                      &Cpu::IndexedIndirectAddressingX, 6)},
             {0x45, Cpu::Instruction("EOR"sv, &Cpu::Instruction_EOR,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0x46, Cpu::Instruction("LSR"sv, &Cpu::Instruction_LSR,
                                      &Cpu::ZeroPageAddressing, 5)},
             {0x48,
              Cpu::Instruction("PHA"sv, &Cpu::Instruction_PHA, nullptr, 3)},
             {0x49, Cpu::Instruction("EOR"sv, &Cpu::Instruction_EOR,
                                      &Cpu::ImmediateAddressing, 2)},
             {0x4A, Cpu::Instruction("LSR"sv, &Cpu::Instruction_LSR_AcummAddr,
                                      nullptr, 2)},
             {0x4C, Cpu::Instruction("JMP"sv, &Cpu::Instruction_JMP,
                                      &Cpu::AbsoluteAddressing, 3)},
             {0x4D, Cpu::Instruction("EOR"sv, &Cpu::Instruction_EOR,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0x4E, Cpu::Instruction("LSR"sv, &Cpu::Instruction_LSR,
                                      &Cpu::AbsoluteAddressing, 6)},
             {0x50, Cpu::Instruction("BVC"sv, &Cpu::Instruction_BVC,
                                      &Cpu::RelativeAddressing, 2)},
             {0x51, Cpu::Instruction("EOR"sv, &Cpu::Instruction_EOR,
                                      &Cpu::IndirectIndexedAddressingY, 5)},
             {0x55, Cpu::Instruction("EOR"sv, &Cpu::Instruction_EOR,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0x56, Cpu::Instruction("LSR"sv, &Cpu::Instruction_LSR,
                                      &Cpu::IndexedZeroPageAddressingX, 6)},
             {0x58,
              Cpu::Instruction("CLI"sv, &Cpu::Instruction_CLI, nullptr, 2)},
             {0x59, Cpu::Instruction("EOR"sv, &Cpu::Instruction_EOR,
                                      &Cpu::IndexedAbsoluteAddressingY, 4)},
             {0x5D, Cpu::Instruction("EOR"sv, &Cpu::Instruction_EOR,
                                      &Cpu::IndexedAbsoluteAddressingX, 4)},
             {0x5E, Cpu::Instruction("LSR"sv, &Cpu::Instruction_LSR,
                                      &Cpu::IndexedAbsoluteAddressingX, 7)},
             {0x60,
              Cpu::Instruction("RTS"sv, &Cpu::Instruction_RTS, nullptr, 6)},
             {0x61, Cpu::Instruction("ADC"sv, &Cpu::Instruction_ADC,
                                      &Cpu::IndexedIndirectAddressingX, 6)},
             {0x65, Cpu::Instruction("ADC"sv, &Cpu::Instruction_ADC,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0x66, Cpu::Instruction("ROR"sv, &Cpu::Instruction_ROR,
                                      &Cpu::ZeroPageAddressing, 5)},
             {0x68,
              Cpu::Instruction("PLA"sv, &Cpu::Instruction_PLA, nullptr, 4)},
             {0x69, Cpu::Instruction("ADC"sv, &Cpu::Instruction_ADC,
                                      &Cpu::ImmediateAddressing, 2)},
             {0x6A, Cpu::Instruction("ROR"sv, &Cpu::Instruction_ROR_AcummAddr,
                                      nullptr, 2)},
             {0x6C, Cpu::Instruction("JMP"sv, &Cpu::Instruction_JMP,
                                      &Cpu::AbsoluteIndirectAddressing, 5)},
             {0x6D, Cpu::Instruction("ADC"sv, &Cpu::Instruction_ADC,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0x6E, Cpu::Instruction("ROR"sv, &Cpu::Instruction_ROR,
                                      &Cpu::AbsoluteAddressing, 6)},
             {0x70, Cpu::Instruction("BVS"sv, &Cpu::Instruction_BVS,
                                      &Cpu::RelativeAddressing, 2)},
             {0x71, Cpu::Instruction("ADC"sv, &Cpu::Instruction_ADC,
                                      &Cpu::IndirectIndexedAddressingY, 5)},
             {0x75, Cpu::Instruction("ADC"sv, &Cpu::Instruction_ADC,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0x76, Cpu::Instruction("ROR"sv, &Cpu::Instruction_ROR,
                                      &Cpu::IndexedZeroPageAddressingX, 6)},
             {0x78,
              Cpu::Instruction("SEI"sv, &Cpu::Instruction_SEI, nullptr, 2)},
             {0x79, Cpu::Instruction("ADC"sv, &Cpu::Instruction_ADC,
                                      &Cpu::IndexedAbsoluteAddressingY, 4)},
             {0x7D, Cpu::Instruction("ADC"sv, &Cpu::Instruction_ADC,
                                      &Cpu::IndexedAbsoluteAddressingX, 4)},
             {0x7E, Cpu::Instruction("ROR"sv, &Cpu::Instruction_ROR,
                                      &Cpu::IndexedAbsoluteAddressingX, 7)},
             {0x81, Cpu::Instruction("STA"sv, &Cpu::Instruction_STA,
                                      &Cpu::IndexedIndirectAddressingX, 6)},
             {0x84, Cpu::Instruction("STY"sv, &Cpu::Instruction_STY,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0x85, Cpu::Instruction("STA"sv, &Cpu::Instruction_STA,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0x86, Cpu::Instruction("STX"sv, &Cpu::Instruction_STX,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0x88,
              Cpu::Instruction("DEY"sv, &Cpu::Instruction_DEY, nullptr, 2)},
             {0x8A,
              Cpu::Instruction("TXA"sv, &Cpu::Instruction_TXA, nullptr, 2)},
             {0x8C, Cpu::Instruction("STY"sv, &Cpu::Instruction_STY,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0x8D, Cpu::Instruction("STA"sv, &Cpu::Instruction_STA,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0x8E, Cpu::Instruction("STX"sv, &Cpu::Instruction_STX,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0x90, Cpu::Instruction("BCC"sv, &Cpu::Instruction_BCC,
                                      &Cpu::RelativeAddressing, 2)},
             {0x91, Cpu::Instruction("STA"sv, &Cpu::Instruction_STA,
                                      &Cpu::IndirectIndexedAddressingY, 6)},
             {0x94, Cpu::Instruction("STY"sv, &Cpu::Instruction_STY,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0x95, Cpu::Instruction("STA"sv, &Cpu::Instruction_STA,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0x96, Cpu::Instruction("STX"sv, &Cpu::Instruction_STX,
                                      &Cpu::IndexedZeroPageAddressingY, 4)},
             {0x98,
              Cpu::Instruction("TYA"sv, &Cpu::Instruction_TYA, nullptr, 2)},
             {0x99, Cpu::Instruction("STA"sv, &Cpu::Instruction_STA,
                                      &Cpu::IndexedAbsoluteAddressingY, 5)},
             {0x9A,
              Cpu::Instruction("TXS"sv, &Cpu::Instruction_TXS, nullptr, 2)},
             {0x9D, Cpu::Instruction("STA"sv, &Cpu::Instruction_STA,
                                      &Cpu::IndexedAbsoluteAddressingX, 5)},
             {0xA0, Cpu::Instruction("LDY"sv, &Cpu::Instruction_LDY,
                                      &Cpu::ImmediateAddressing, 2)},
             {0xA1, Cpu::Instruction("LDA"sv, &Cpu::Instruction_LDA,
                                      &Cpu::IndexedIndirectAddressingX, 6)},
             {0xA2, Cpu::Instruction("LDX"sv, &Cpu::Instruction_LDX,
                                      &Cpu::ImmediateAddressing, 2)},
             {0xA4, Cpu::Instruction("LDY"sv, &Cpu::Instruction_LDY,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0xA5, Cpu::Instruction("LDA"sv, &Cpu::Instruction_LDA,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0xA6, Cpu::Instruction("LDX"sv, &Cpu::Instruction_LDX,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0xA8,
              Cpu::Instruction("TAY"sv, &Cpu::Instruction_TAY, nullptr, 2)},
             {0xA9, Cpu::Instruction("LDA"sv, &Cpu::Instruction_LDA,
                                      &Cpu::ImmediateAddressing, 2)},
             {0xAA,
              Cpu::Instruction("TAX"sv, &Cpu::Instruction_TAX, nullptr, 2)},
             {0xAC, Cpu::Instruction("LDY"sv, &Cpu::Instruction_LDY,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0xAD, Cpu::Instruction("LDA"sv, &Cpu::Instruction_LDA,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0xAE, Cpu::Instruction("LDX"sv, &Cpu::Instruction_LDX,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0xB0, Cpu::Instruction("BCS"sv, &Cpu::Instruction_BCS,
                                      &Cpu::RelativeAddressing, 2)},
             {0xB1, Cpu::Instruction("LDA"sv, &Cpu::Instruction_LDA,
                                      &Cpu::IndirectIndexedAddressingY, 5)},
             {0xB4, Cpu::Instruction("LDY"sv, &Cpu::Instruction_LDY,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0xB5, Cpu::Instruction("LDA"sv, &Cpu::Instruction_LDA,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0xB6, Cpu::Instruction("LDX"sv, &Cpu::Instruction_LDX,
                                      &Cpu::IndexedZeroPageAddressingY, 4)},
             {0xB8,
              Cpu::Instruction("CLV"sv, &Cpu::Instruction_CLV, nullptr, 2)},
             {0xB9, Cpu::Instruction("LDA"sv, &Cpu::Instruction_LDA,
                                      &Cpu::IndexedAbsoluteAddressingY, 4)},
             {0xBA,
              Cpu::Instruction("TSX"sv, &Cpu::Instruction_TSX, nullptr, 2)},
             {0xBC, Cpu::Instruction("LDY"sv, &Cpu::Instruction_LDY,
                                      &Cpu::IndexedAbsoluteAddressingX, 4)},
             {0xBD, Cpu::Instruction("LDA"sv, &Cpu::Instruction_LDA,
                                      &Cpu::IndexedAbsoluteAddressingX, 4)},
             {0xBE, Cpu::Instruction("LDX"sv, &Cpu::Instruction_LDX,
                                      &Cpu::IndexedAbsoluteAddressingY, 4)},
             {0xC0, Cpu::Instruction("CPY"sv, &Cpu::Instruction_CPY,
                                      &Cpu::ImmediateAddressing, 2)},
             {0xC1, Cpu::Instruction("CMP"sv, &Cpu::Instruction_CMP,
                                      &Cpu::IndexedIndirectAddressingX, 6)},
             {0xC4, Cpu::Instruction("CPY"sv, &Cpu::Instruction_CPY,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0xC5, Cpu::Instruction("CMP"sv, &Cpu::Instruction_CMP,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0xC6, Cpu::Instruction("DEC"sv, &Cpu::Instruction_DEC,
                                      &Cpu::ZeroPageAddressing, 5)},
             {0xC8,
              Cpu::Instruction("INY"sv, &Cpu::Instruction_INY, nullptr, 2)},
             {0xC9, Cpu::Instruction("CMP"sv, &Cpu::Instruction_CMP,
                                      &Cpu::ImmediateAddressing, 2)},
             {0xCA,
              Cpu::Instruction("DEX"sv, &Cpu::Instruction_DEX, nullptr, 2)},
             {0xCC, Cpu::Instruction("CPY"sv, &Cpu::Instruction_CPY,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0xCD, Cpu::Instruction("CMP"sv, &Cpu::Instruction_CMP,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0xCE, Cpu::Instruction("DEC"sv, &Cpu::Instruction_DEC,
                                      &Cpu::AbsoluteAddressing, 6)},
             {0xD0, Cpu::Instruction("BNE"sv, &Cpu::Instruction_BNE,
                                      &Cpu::RelativeAddressing, 2)},
             {0xD1, Cpu::Instruction("CMP"sv, &Cpu::Instruction_CMP,
                                      &Cpu::IndirectIndexedAddressingY, 5)},
             {0xD5, Cpu::Instruction("CMP"sv, &Cpu::Instruction_CMP,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0xD6, Cpu::Instruction("DEC"sv, &Cpu::Instruction_DEC,
                                      &Cpu::IndexedZeroPageAddressingX, 6)},
             {0xD8,
              Cpu::Instruction("CLD"sv, &Cpu::Instruction_CLD, nullptr, 2)},
             {0xD9, Cpu::Instruction("CMP"sv, &Cpu::Instruction_CMP,
                                      &Cpu::IndexedAbsoluteAddressingY, 4)},
             {0xDD, Cpu::Instruction("CMP"sv, &Cpu::Instruction_CMP,
                                      &Cpu::IndexedAbsoluteAddressingX, 4)},
             {0xDE, Cpu::Instruction("DEC"sv, &Cpu::Instruction_DEC,
                                      &Cpu::IndexedAbsoluteAddressingX, 7)},
             {0xE0, Cpu::Instruction("CPX"sv, &Cpu::Instruction_CPX,
                                      &Cpu::ImmediateAddressing, 2)},
             {0xE1, Cpu::Instruction("SBC"sv, &Cpu::Instruction_SBC,
                                      &Cpu::IndexedIndirectAddressingX, 6)},
             {0xE4, Cpu::Instruction("CPX"sv, &Cpu::Instruction_CPX,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0xE5, Cpu::Instruction("SBC"sv, &Cpu::Instruction_SBC,
                                      &Cpu::ZeroPageAddressing, 3)},
             {0xE6, Cpu::Instruction("INC"sv, &Cpu::Instruction_INC,
                                      &Cpu::ZeroPageAddressing, 5)},
             {0xE8,
              Cpu::Instruction("INX"sv, &Cpu::Instruction_INX, nullptr, 2)},
             {0xE9, Cpu::Instruction("SBC"sv, &Cpu::Instruction_SBC,
                                      &Cpu::ImmediateAddressing, 2)},
             {0xEA,
              Cpu::Instruction("NOP"sv, &Cpu::Instruction_NOP, nullptr, 2)},
             {0xEC, Cpu::Instruction("CPX"sv, &Cpu::Instruction_CPX,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0xED, Cpu::Instruction("SBC"sv, &Cpu::Instruction_SBC,
                                      &Cpu::AbsoluteAddressing, 4)},
             {0xEE, Cpu::Instruction("INC"sv, &Cpu::Instruction_INC,
                                      &Cpu::AbsoluteAddressing, 6)},
             {0xF0, Cpu::Instruction("BEQ"sv, &Cpu::Instruction_BEQ,
                                      &Cpu::RelativeAddressing, 2)},
             {0xF1, Cpu::Instruction("SBC"sv, &Cpu::Instruction_SBC,
                                      &Cpu::IndirectIndexedAddressingY, 5)},
             {0xF5, Cpu::Instruction("SBC"sv, &Cpu::Instruction_SBC,
                                      &Cpu::IndexedZeroPageAddressingX, 4)},
             {0xF6, Cpu::Instruction("INC"sv, &Cpu::Instruction_INC,
                                      &Cpu::IndexedZeroPageAddressingX, 6)},
             {0xF8,
              Cpu::Instruction("SED"sv, &Cpu::Instruction_SED, nullptr, 2)},
             {0xF9, Cpu::Instruction("SBC"sv, &Cpu::Instruction_SBC,
                                      &Cpu::IndexedAbsoluteAddressingY, 4)},
             {0xFD, Cpu::Instruction("SBC"sv, &Cpu::Instruction_SBC,
                                      &Cpu::IndexedAbsoluteAddressingX, 4)},
             {0xFE, Cpu::Instruction("INC"sv, &Cpu::Instruction_INC,
                                      &Cpu::IndexedAbsoluteAddressingX, 7)}}};

    static constexpr auto map =
        Map<uint8_t, Cpu::Instruction, lookupTable.size()>{{lookupTable}};

    return map.at(opCode);
}

}  // namespace cpuemulator