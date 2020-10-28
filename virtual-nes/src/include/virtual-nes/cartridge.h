// Copyright (c) 2020 Emmanuel Arias
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace virtualnes {

// Forward declarations
class IMapper;

/// <summary>
/// Holds the information and data from a valid NES game cartridge. This
/// class only supports cartridges that comply to the iNES file format.
/// It also supports only cartrigdes with mappers that the emulator has implemented.
/// For more information about this format, refer to https://wiki.nesdev.com/w/index.php/INES.
/// </summary>
class Cartridge {
   public:

    /// <summary>
    /// Constructor to load a cartridge from a file path. The constructor will try to
    /// load the file and set m_IsLoaded to true if the cartridge is valid.
    /// </summary>
    /// <param name="fileName">File path for the catridge data</param>
    explicit Cartridge(const std::string& fileName);

    /// <summary>
    /// Windows version for wstring paths.
    /// Constructor to load a cartridge from a file path. The constructor will
    /// try to load the file and set m_IsLoaded to true if the cartridge is
    /// valid.
    /// </summary>
    /// <param name="fileName">File path for the catridge data</param>
    explicit Cartridge(const std::wstring& fileName);

    /// <summary>
    /// Mirroring mode for the game. Refer to https://wiki.nesdev.com/w/index.php/Mirroring
    /// </summary>
    enum class MIRROR { HORIZONTAL, VERTICAL, ONESCREEN_LO, ONESCREEN_HI };

    /// <summary>
    /// Return true if the cartridge was supported and properly loaded.
    /// In the future this will be replaced for a Cartridge factory pattern
    /// so that we can check if a cartrisge type is supported, and not fail
    /// for that reason.
    /// </summary>
    /// <returns></returns>
    bool IsLoaded() const { return m_IsLoaded; }

    /// <summary>
    /// Attempt to read from the CPU to the cartridge memory. If the mapper
    /// determines that the address is not in its domain, returns false and do nothing.
    /// Otherwise, read the data into the reference data param.
    /// </summary>
    /// <param name="address"></param>
    /// <param name="data"></param>
    /// <returns></returns>
    bool CpuRead(uint16_t address, uint8_t& data);
    
    /// <summary>
    /// Attempt to write from the CPU to the cartridge memory. If the mapper
    /// determines that the address is not in its domain, returns false and do
    /// nothing. Otherwise, write the data in memory.
    /// </summary>
    /// <param name="address"></param>
    /// <param name="data"></param>
    /// <returns></returns>
    bool CpuWrite(uint16_t address, uint8_t data);

    /// <summary>
    /// Attempt to read from the PPU to the cartridge memory. If the mapper
    /// determines that the address is not in its domain, returns false and do
    /// nothing. Otherwise, read the data into the reference data param.
    /// </summary>
    /// <param name="address"></param>
    /// <param name="data"></param>
    /// <returns></returns>
    bool PpuRead(uint16_t address, uint8_t& data);
    
    /// <summary>
    /// Attempt to write from the PPU to the cartridge memory. If the mapper
    /// determines that the address is not in its domain, returns false and do
    /// nothing. Otherwise, write the data in memory.
    /// </summary>
    /// <param name="address"></param>
    /// <param name="data"></param>
    /// <returns></returns>
    bool PpuWrite(uint16_t address, uint8_t data);

    /// <summary>
    /// Returns the mirroring mode for the cartridge
    /// </summary>
    /// <returns></returns>
    MIRROR GetMirroringMode() const;

   private:
    bool m_IsLoaded = false;
    uint8_t m_MapperId = 0x00;
    uint8_t m_NumPrgBanks = 0x00;
    uint8_t m_NumChrBanks = 0x00;
    std::vector<uint8_t> m_ProgramMemory;
    std::vector<uint8_t> m_CharacterMemory;

    MIRROR m_MirroringMode = MIRROR::HORIZONTAL;

    std::shared_ptr<IMapper> m_Mapper;

    void ConstructFromFile(std::ifstream& ifs);
};
}  // namespace virtualnes
