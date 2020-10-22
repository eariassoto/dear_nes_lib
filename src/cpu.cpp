// Copyright (c) 2020 Emmanuel Arias
#include "include/cpu.h"

#include <cassert>
#include "include/bus.h"

namespace cpuemulator {

Cpu::Cpu() { RegisterAllInstructionSet(); }

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

        // todo handle illegal ops
        auto instrIt = m_InstrTable.find(m_OpCode);
        if (instrIt == m_InstrTable.end()) {
            return;
        }

        Instruction &instr = instrIt->second;

        m_Cycles = instr.m_Cycles;

        if (instr.m_AddressingMode) {
            instr.m_AddressingMode(this);
        }

        instr.m_FuncOperate(this);

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

void Cpu::RegisterAllInstructionSet() {
    m_InstrTable.try_emplace(0x00, "BRK", &Cpu::Instruction_BRK, nullptr, 7);
    m_InstrTable.try_emplace(0x01, "ORA", &Cpu::Instruction_ORA,
                             &Cpu::IndexedIndirectAddressingX, 6);
    m_InstrTable.try_emplace(0x05, "ORA", &Cpu::Instruction_ORA,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0x06, "ASL", &Cpu::Instruction_ASL,
                             &Cpu::ZeroPageAddressing, 5);
    m_InstrTable.try_emplace(0x08, "PHP", &Cpu::Instruction_PHP, nullptr, 3);
    m_InstrTable.try_emplace(0x09, "ORA", &Cpu::Instruction_ORA,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0x0A, "ASL", &Cpu::Instruction_ASL_AcummAddr,
                             nullptr, 2);
    m_InstrTable.try_emplace(0x0D, "ORA", &Cpu::Instruction_ORA,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0x0E, "ASL", &Cpu::Instruction_ASL,
                             &Cpu::AbsoluteAddressing, 6);
    m_InstrTable.try_emplace(0x10, "BPL", &Cpu::Instruction_BPL,
                             &Cpu::RelativeAddressing, 2);
    m_InstrTable.try_emplace(0x11, "ORA", &Cpu::Instruction_ORA,
                             &Cpu::IndirectIndexedAddressingY, 5);
    m_InstrTable.try_emplace(0x15, "ORA", &Cpu::Instruction_ORA,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0x16, "ASL", &Cpu::Instruction_ASL,
                             &Cpu::IndexedZeroPageAddressingX, 6);
    m_InstrTable.try_emplace(0x18, "CLC", &Cpu::Instruction_CLC, nullptr, 2);
    m_InstrTable.try_emplace(0x19, "ORA", &Cpu::Instruction_ORA,
                             &Cpu::IndexedAbsoluteAddressingY, 4);
    m_InstrTable.try_emplace(0x1D, "ORA", &Cpu::Instruction_ORA,
                             &Cpu::IndexedAbsoluteAddressingX, 4);
    m_InstrTable.try_emplace(0x1E, "ASL", &Cpu::Instruction_ASL,
                             &Cpu::IndexedAbsoluteAddressingX, 7);
    m_InstrTable.try_emplace(0x20, "JSR", &Cpu::Instruction_JSR,
                             &Cpu::AbsoluteAddressing, 6);
    m_InstrTable.try_emplace(0x21, "AND", &Cpu::Instruction_AND,
                             &Cpu::IndexedIndirectAddressingX, 6);
    m_InstrTable.try_emplace(0x24, "BIT", &Cpu::Instruction_BIT,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0x25, "AND", &Cpu::Instruction_AND,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0x26, "ROL", &Cpu::Instruction_ROL,
                             &Cpu::ZeroPageAddressing, 5);
    m_InstrTable.try_emplace(0x28, "PLP", &Cpu::Instruction_PLP, nullptr, 4);
    m_InstrTable.try_emplace(0x29, "AND", &Cpu::Instruction_AND,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0x2A, "ROL", &Cpu::Instruction_ROL_AcummAddr,
                             nullptr, 2);
    m_InstrTable.try_emplace(0x2C, "BIT", &Cpu::Instruction_BIT,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0x2D, "AND", &Cpu::Instruction_AND,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0x2E, "ROL", &Cpu::Instruction_ROL,
                             &Cpu::AbsoluteAddressing, 6);
    m_InstrTable.try_emplace(0x30, "BMI", &Cpu::Instruction_BMI,
                             &Cpu::RelativeAddressing, 2);
    m_InstrTable.try_emplace(0x31, "AND", &Cpu::Instruction_AND,
                             &Cpu::IndirectIndexedAddressingY, 5);
    m_InstrTable.try_emplace(0x35, "AND", &Cpu::Instruction_AND,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0x36, "ROL", &Cpu::Instruction_ROL,
                             &Cpu::IndexedZeroPageAddressingX, 6);
    m_InstrTable.try_emplace(0x38, "SEC", &Cpu::Instruction_SEC, nullptr, 2);
    m_InstrTable.try_emplace(0x39, "AND", &Cpu::Instruction_AND,
                             &Cpu::IndexedAbsoluteAddressingY, 4);
    m_InstrTable.try_emplace(0x3D, "AND", &Cpu::Instruction_AND,
                             &Cpu::IndexedAbsoluteAddressingX, 4);
    m_InstrTable.try_emplace(0x3E, "ROL", &Cpu::Instruction_ROL,
                             &Cpu::IndexedAbsoluteAddressingX, 7);
    m_InstrTable.try_emplace(0x40, "RTI", &Cpu::Instruction_RTI, nullptr, 6);
    m_InstrTable.try_emplace(0x41, "EOR", &Cpu::Instruction_EOR,
                             &Cpu::IndexedIndirectAddressingX, 6);
    m_InstrTable.try_emplace(0x45, "EOR", &Cpu::Instruction_EOR,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0x46, "LSR", &Cpu::Instruction_LSR,
                             &Cpu::ZeroPageAddressing, 5);
    m_InstrTable.try_emplace(0x48, "PHA", &Cpu::Instruction_PHA, nullptr, 3);
    m_InstrTable.try_emplace(0x49, "EOR", &Cpu::Instruction_EOR,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0x4A, "LSR", &Cpu::Instruction_LSR_AcummAddr,
                             nullptr, 2);
    m_InstrTable.try_emplace(0x4C, "JMP", &Cpu::Instruction_JMP,
                             &Cpu::AbsoluteAddressing, 3);
    m_InstrTable.try_emplace(0x4D, "EOR", &Cpu::Instruction_EOR,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0x4E, "LSR", &Cpu::Instruction_LSR,
                             &Cpu::AbsoluteAddressing, 6);
    m_InstrTable.try_emplace(0x50, "BVC", &Cpu::Instruction_BVC,
                             &Cpu::RelativeAddressing, 2);
    m_InstrTable.try_emplace(0x51, "EOR", &Cpu::Instruction_EOR,
                             &Cpu::IndirectIndexedAddressingY, 5);
    m_InstrTable.try_emplace(0x55, "EOR", &Cpu::Instruction_EOR,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0x56, "LSR", &Cpu::Instruction_LSR,
                             &Cpu::IndexedZeroPageAddressingX, 6);
    m_InstrTable.try_emplace(0x58, "CLI", &Cpu::Instruction_CLI, nullptr, 2);
    m_InstrTable.try_emplace(0x59, "EOR", &Cpu::Instruction_EOR,
                             &Cpu::IndexedAbsoluteAddressingY, 4);
    m_InstrTable.try_emplace(0x5D, "EOR", &Cpu::Instruction_EOR,
                             &Cpu::IndexedAbsoluteAddressingX, 4);
    m_InstrTable.try_emplace(0x5E, "LSR", &Cpu::Instruction_LSR,
                             &Cpu::IndexedAbsoluteAddressingX, 7);
    m_InstrTable.try_emplace(0x60, "RTS", &Cpu::Instruction_RTS, nullptr, 6);
    m_InstrTable.try_emplace(0x61, "ADC", &Cpu::Instruction_ADC,
                             &Cpu::IndexedIndirectAddressingX, 6);
    m_InstrTable.try_emplace(0x65, "ADC", &Cpu::Instruction_ADC,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0x66, "ROR", &Cpu::Instruction_ROR,
                             &Cpu::ZeroPageAddressing, 5);
    m_InstrTable.try_emplace(0x68, "PLA", &Cpu::Instruction_PLA, nullptr, 4);
    m_InstrTable.try_emplace(0x69, "ADC", &Cpu::Instruction_ADC,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0x6A, "ROR", &Cpu::Instruction_ROR_AcummAddr,
                             nullptr, 2);
    m_InstrTable.try_emplace(0x6C, "JMP", &Cpu::Instruction_JMP,
                             &Cpu::AbsoluteIndirectAddressing, 5);
    m_InstrTable.try_emplace(0x6D, "ADC", &Cpu::Instruction_ADC,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0x6E, "ROR", &Cpu::Instruction_ROR,
                             &Cpu::AbsoluteAddressing, 6);
    m_InstrTable.try_emplace(0x70, "BVS", &Cpu::Instruction_BVS,
                             &Cpu::RelativeAddressing, 2);
    m_InstrTable.try_emplace(0x71, "ADC", &Cpu::Instruction_ADC,
                             &Cpu::IndirectIndexedAddressingY, 5);
    m_InstrTable.try_emplace(0x75, "ADC", &Cpu::Instruction_ADC,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0x76, "ROR", &Cpu::Instruction_ROR,
                             &Cpu::IndexedZeroPageAddressingX, 6);
    m_InstrTable.try_emplace(0x78, "SEI", &Cpu::Instruction_SEI, nullptr, 2);
    m_InstrTable.try_emplace(0x79, "ADC", &Cpu::Instruction_ADC,
                             &Cpu::IndexedAbsoluteAddressingY, 4);
    m_InstrTable.try_emplace(0x7D, "ADC", &Cpu::Instruction_ADC,
                             &Cpu::IndexedAbsoluteAddressingX, 4);
    m_InstrTable.try_emplace(0x7E, "ROR", &Cpu::Instruction_ROR,
                             &Cpu::IndexedAbsoluteAddressingX, 7);
    m_InstrTable.try_emplace(0x81, "STA", &Cpu::Instruction_STA,
                             &Cpu::IndexedIndirectAddressingX, 6);
    m_InstrTable.try_emplace(0x84, "STY", &Cpu::Instruction_STY,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0x85, "STA", &Cpu::Instruction_STA,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0x86, "STX", &Cpu::Instruction_STX,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0x88, "DEY", &Cpu::Instruction_DEY, nullptr, 2);
    m_InstrTable.try_emplace(0x8A, "TXA", &Cpu::Instruction_TXA, nullptr, 2);
    m_InstrTable.try_emplace(0x8C, "STY", &Cpu::Instruction_STY,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0x8D, "STA", &Cpu::Instruction_STA,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0x8E, "STX", &Cpu::Instruction_STX,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0x90, "BCC", &Cpu::Instruction_BCC,
                             &Cpu::RelativeAddressing, 2);
    m_InstrTable.try_emplace(0x91, "STA", &Cpu::Instruction_STA,
                             &Cpu::IndirectIndexedAddressingY, 6);
    m_InstrTable.try_emplace(0x94, "STY", &Cpu::Instruction_STY,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0x95, "STA", &Cpu::Instruction_STA,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0x96, "STX", &Cpu::Instruction_STX,
                             &Cpu::IndexedZeroPageAddressingY, 4);
    m_InstrTable.try_emplace(0x98, "TYA", &Cpu::Instruction_TYA, nullptr, 2);
    m_InstrTable.try_emplace(0x99, "STA", &Cpu::Instruction_STA,
                             &Cpu::IndexedAbsoluteAddressingY, 5);
    m_InstrTable.try_emplace(0x9A, "TXS", &Cpu::Instruction_TXS, nullptr, 2);
    m_InstrTable.try_emplace(0x9D, "STA", &Cpu::Instruction_STA,
                             &Cpu::IndexedAbsoluteAddressingX, 5);
    m_InstrTable.try_emplace(0xA0, "LDY", &Cpu::Instruction_LDY,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0xA1, "LDA", &Cpu::Instruction_LDA,
                             &Cpu::IndexedIndirectAddressingX, 6);
    m_InstrTable.try_emplace(0xA2, "LDX", &Cpu::Instruction_LDX,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0xA4, "LDY", &Cpu::Instruction_LDY,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0xA5, "LDA", &Cpu::Instruction_LDA,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0xA6, "LDX", &Cpu::Instruction_LDX,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0xA8, "TAY", &Cpu::Instruction_TAY, nullptr, 2);
    m_InstrTable.try_emplace(0xA9, "LDA", &Cpu::Instruction_LDA,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0xAA, "TAX", &Cpu::Instruction_TAX, nullptr, 2);
    m_InstrTable.try_emplace(0xAC, "LDY", &Cpu::Instruction_LDY,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0xAD, "LDA", &Cpu::Instruction_LDA,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0xAE, "LDX", &Cpu::Instruction_LDX,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0xB0, "BCS", &Cpu::Instruction_BCS,
                             &Cpu::RelativeAddressing, 2);
    m_InstrTable.try_emplace(0xB1, "LDA", &Cpu::Instruction_LDA,
                             &Cpu::IndirectIndexedAddressingY, 5);
    m_InstrTable.try_emplace(0xB4, "LDY", &Cpu::Instruction_LDY,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0xB5, "LDA", &Cpu::Instruction_LDA,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0xB6, "LDX", &Cpu::Instruction_LDX,
                             &Cpu::IndexedZeroPageAddressingY, 4);
    m_InstrTable.try_emplace(0xB8, "CLV", &Cpu::Instruction_CLV, nullptr, 2);
    m_InstrTable.try_emplace(0xB9, "LDA", &Cpu::Instruction_LDA,
                             &Cpu::IndexedAbsoluteAddressingY, 4);
    m_InstrTable.try_emplace(0xBA, "TSX", &Cpu::Instruction_TSX, nullptr, 2);
    m_InstrTable.try_emplace(0xBC, "LDY", &Cpu::Instruction_LDY,
                             &Cpu::IndexedAbsoluteAddressingX, 4);
    m_InstrTable.try_emplace(0xBD, "LDA", &Cpu::Instruction_LDA,
                             &Cpu::IndexedAbsoluteAddressingX, 4);
    m_InstrTable.try_emplace(0xBE, "LDX", &Cpu::Instruction_LDX,
                             &Cpu::IndexedAbsoluteAddressingY, 4);
    m_InstrTable.try_emplace(0xC0, "CPY", &Cpu::Instruction_CPY,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0xC1, "CMP", &Cpu::Instruction_CMP,
                             &Cpu::IndexedIndirectAddressingX, 6);
    m_InstrTable.try_emplace(0xC4, "CPY", &Cpu::Instruction_CPY,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0xC5, "CMP", &Cpu::Instruction_CMP,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0xC6, "DEC", &Cpu::Instruction_DEC,
                             &Cpu::ZeroPageAddressing, 5);
    m_InstrTable.try_emplace(0xC8, "INY", &Cpu::Instruction_INY, nullptr, 2);
    m_InstrTable.try_emplace(0xC9, "CMP", &Cpu::Instruction_CMP,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0xCA, "DEX", &Cpu::Instruction_DEX, nullptr, 2);
    m_InstrTable.try_emplace(0xCC, "CPY", &Cpu::Instruction_CPY,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0xCD, "CMP", &Cpu::Instruction_CMP,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0xCE, "DEC", &Cpu::Instruction_DEC,
                             &Cpu::AbsoluteAddressing, 6);
    m_InstrTable.try_emplace(0xD0, "BNE", &Cpu::Instruction_BNE,
                             &Cpu::RelativeAddressing, 2);
    m_InstrTable.try_emplace(0xD1, "CMP", &Cpu::Instruction_CMP,
                             &Cpu::IndirectIndexedAddressingY, 5);
    m_InstrTable.try_emplace(0xD5, "CMP", &Cpu::Instruction_CMP,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0xD6, "DEC", &Cpu::Instruction_DEC,
                             &Cpu::IndexedZeroPageAddressingX, 6);
    m_InstrTable.try_emplace(0xD8, "CLD", &Cpu::Instruction_CLD, nullptr, 2);
    m_InstrTable.try_emplace(0xD9, "CMP", &Cpu::Instruction_CMP,
                             &Cpu::IndexedAbsoluteAddressingY, 4);
    m_InstrTable.try_emplace(0xDD, "CMP", &Cpu::Instruction_CMP,
                             &Cpu::IndexedAbsoluteAddressingX, 4);
    m_InstrTable.try_emplace(0xDE, "DEC", &Cpu::Instruction_DEC,
                             &Cpu::IndexedAbsoluteAddressingX, 7);
    m_InstrTable.try_emplace(0xE0, "CPX", &Cpu::Instruction_CPX,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0xE1, "SBC", &Cpu::Instruction_SBC,
                             &Cpu::IndexedIndirectAddressingX, 6);
    m_InstrTable.try_emplace(0xE4, "CPX", &Cpu::Instruction_CPX,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0xE5, "SBC", &Cpu::Instruction_SBC,
                             &Cpu::ZeroPageAddressing, 3);
    m_InstrTable.try_emplace(0xE6, "INC", &Cpu::Instruction_INC,
                             &Cpu::ZeroPageAddressing, 5);
    m_InstrTable.try_emplace(0xE8, "INX", &Cpu::Instruction_INX, nullptr, 2);
    m_InstrTable.try_emplace(0xE9, "SBC", &Cpu::Instruction_SBC,
                             &Cpu::ImmediateAddressing, 2);
    m_InstrTable.try_emplace(0xEA, "NOP", &Cpu::Instruction_NOP, nullptr, 2);
    m_InstrTable.try_emplace(0xEC, "CPX", &Cpu::Instruction_CPX,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0xED, "SBC", &Cpu::Instruction_SBC,
                             &Cpu::AbsoluteAddressing, 4);
    m_InstrTable.try_emplace(0xEE, "INC", &Cpu::Instruction_INC,
                             &Cpu::AbsoluteAddressing, 6);
    m_InstrTable.try_emplace(0xF0, "BEQ", &Cpu::Instruction_BEQ,
                             &Cpu::RelativeAddressing, 2);
    m_InstrTable.try_emplace(0xF1, "SBC", &Cpu::Instruction_SBC,
                             &Cpu::IndirectIndexedAddressingY, 5);
    m_InstrTable.try_emplace(0xF5, "SBC", &Cpu::Instruction_SBC,
                             &Cpu::IndexedZeroPageAddressingX, 4);
    m_InstrTable.try_emplace(0xF6, "INC", &Cpu::Instruction_INC,
                             &Cpu::IndexedZeroPageAddressingX, 6);
    m_InstrTable.try_emplace(0xF8, "SED", &Cpu::Instruction_SED, nullptr, 2);
    m_InstrTable.try_emplace(0xF9, "SBC", &Cpu::Instruction_SBC,
                             &Cpu::IndexedAbsoluteAddressingY, 4);
    m_InstrTable.try_emplace(0xFD, "SBC", &Cpu::Instruction_SBC,
                             &Cpu::IndexedAbsoluteAddressingX, 4);
    m_InstrTable.try_emplace(0xFE, "INC", &Cpu::Instruction_INC,
                             &Cpu::IndexedAbsoluteAddressingX, 7);
}

}  // namespace cpuemulator