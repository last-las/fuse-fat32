#include "device.h"

namespace device {
    /**
     * Sector
     * */
    Sector::Sector(u32 sec_num, Device &device) noexcept: sec_num_{sec_num}, device_{device} {}

    void Sector::mark_dirty() noexcept {
        dirty_ = true;
    }

    const void *Sector::read_ptr(u32 offset) noexcept {
        return &value_[offset];
    }

    const void *Sector::write_ptr(u32 offset) noexcept {
        dirty_ = true;
        return &value_[offset];
    }

    void Sector::sync() noexcept {
        if (dirty_) {
            device_.writeSectorValue(sec_num_, value_);
            dirty_ = false;
        }
    }

    Sector::~Sector() noexcept {
        sync();
    }

    /**
     * LinuxFileDevice
     * */
    LinuxFileDevice::LinuxFileDevice(std::string file_path) noexcept: file_path_{std::move(file_path)} {}

    std::optional<std::shared_ptr<Sector>> LinuxFileDevice::readSector(u32 sec_num) noexcept {}

    bool LinuxFileDevice::writeSectorValue(u32 sec_num, const u8 *buf) noexcept {}

    /**
     * CacheManger
     * */
    CacheManager::CacheManager(Device &device, u32 cache_sz) noexcept
            : device_{device}, sector_cache_(cache_sz) {}

    std::optional<std::shared_ptr<Sector>> CacheManager::readSector(u32 sec_num) noexcept {}

    bool CacheManager::writeSectorValue(u32 sec_num, const u8 *buf) noexcept {}
}
