#include "virtual-nes/cartridge_header.h"

namespace virtualnes {

CartridgeHeader::CartridgeHeader(std::ifstream& inputStream) {
    inputStream.read(reinterpret_cast<char*>(&m_iNesHeader),
                     sizeof(m_iNesHeader));

    m_MirroringMode = (m_iNesHeader.m_Mapper1 & 0x01)
                          ? CARTRIDGE_MIRRORING_MODE::VERTICAL
                          : CARTRIDGE_MIRRORING_MODE::HORIZONTAL;
}

bool CartridgeHeader::HasTrainerData() const {
    return m_iNesHeader.m_Mapper1 & 0x04;
}

CARTRIDGE_MIRRORING_MODE CartridgeHeader::GetMirroringMode() const {
    return m_MirroringMode;
}

}  // namespace virtualnes
