#ifndef STUPID_FAT32_DEVICE_H
#define STUPID_FAT32_DEVICE_H

#include <memory>
#include <string>
#include <utility>
#include "config.h"
#include "util.h"

namespace device {
    class Sector;

    class Device {
    public:
        virtual std::optional<std::shared_ptr<Sector>> readSector(u32) = 0;

        virtual bool writeSectorValue(u32, const u8 *) = 0;
    };

    // TODO: comment here.
    class Sector {
    public:
        Sector(u32 sec_num, Device &device) noexcept;

        void mark_dirty() noexcept;

        const void *read_ptr(u32 offset) noexcept;

        const void *write_ptr(u32 offset) noexcept;

        void sync() noexcept;

        void modify_device(Device &device) noexcept;

        ~Sector() noexcept;

    private:
        u32 sec_num_;
        u8 value_[SECTOR_SIZE];
        Device &device_;
        bool dirty_;
    };

    /**
     * Treat Linux file as a block device using Linux system call.
     *
     * When using the `LinuxFileDriver`, the caller must make sure the `file_path_` exist, otherwise it panics.
     * */
    class LinuxFileDriver : public Device {
    public:
        explicit LinuxFileDriver(std::string file_path) noexcept;

        std::optional<std::shared_ptr<Sector>> readSector(u32 sec_num) noexcept override;

        bool writeSectorValue(u32 sec_num, const u8 *buf) noexcept override;

    private:
        std::string file_path_;
        u64 device_sz_;
    };

    class CacheManager : public Device {
    public:
        explicit CacheManager(Device &device, u32 cache_sz = CACHED_SECTOR_NUM) noexcept;

        std::optional<std::shared_ptr<Sector>> readSector(u32 sec_num) noexcept override;

        bool writeSectorValue(u32 sec_num, const u8 *buf) noexcept override;

        bool contains(u32 sec_num) noexcept;

    private:
        Device &inner_device_;
        util::LRUCacheMap<u32, std::shared_ptr<Sector>> sector_cache_;
    };

} // namespace device

#endif //STUPID_FAT32_DEVICE_H