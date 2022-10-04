#ifndef STUPID_FAT32_DEVICE_H
#define STUPID_FAT32_DEVICE_H

#include "util.h"
#include <string>

namespace device {
    class Device {
    public:
        virtual bool readSector(u32, u8 *) = 0;

        virtual bool writeSector(u32, const u8 *) = 0;
    };

    class LinuxFileDevice : public Device {
    public:
        LinuxFileDevice(std::string file_path, u32 byts_per_sec) noexcept {}

        bool readSector(u32 sec_num, u8 *buf) noexcept override {}

        bool writeSector(u32 sec_num, const u8 *buf) noexcept override {}

    private:
        std::string file_path_;
        u16 byts_per_sec_;
    };

    class CacheManager : Device {
    public:
        CacheManager(Device &device) noexcept: device_{device} {}

        bool readSector(u32 sec_num, u8 *buf) noexcept override {}

        bool writeSector(u32 sec_num, const u8 *buf) noexcept override {}

    private:
        Device &device_;
    };

} // namespace device

#endif //STUPID_FAT32_DEVICE_H