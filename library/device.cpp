#include <stdint.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "unistd.h"
#include <fcntl.h>
#include <cassert>
#include <cstring>

#include "device.h"

namespace device {
    /**
     * Sector
     * */
    Sector::Sector(u32 sec_num, Device &device) noexcept
            : sec_num_{sec_num}, device_{device}, dirty_(false) {
        // std::memcpy(value_, value, SECTOR_SIZE);
    }

    void Sector::mark_dirty() noexcept {
        dirty_ = true;
    }

    const void *Sector::read_ptr(u32 offset) noexcept {
        assert(offset < SECTOR_SIZE);
        return &value_[offset];
    }

    const void *Sector::write_ptr(u32 offset) noexcept {
        assert(offset < SECTOR_SIZE);
        dirty_ = true;
        return &value_[offset];
    }

    void Sector::sync() noexcept {
        if (dirty_) {
            device_.writeSectorValue(sec_num_, value_);
            dirty_ = false;
        }
    }

    void Sector::modify_device(Device &device) noexcept {
        device_ = device;
    }

    Sector::~Sector() noexcept {
        sync();
    }

    /**
     * LinuxFileDriver
     * */
    LinuxFileDriver::LinuxFileDriver(std::string file_path) noexcept: file_path_{std::move(file_path)} {
        int fd = open(file_path_.c_str(), O_RDONLY);
        assert(fd > 0);

        // get device size
        struct stat stat_buf{};
        assert(fstat(fd, &stat_buf) == 0);
        if (stat_buf.st_mode & S_IFREG) {
            device_sz_ = stat_buf.st_size;
        } else if (stat_buf.st_mode & S_IFBLK) {
            assert(ioctl(fd, BLKGETSIZE64, &device_sz_) == 0);
        } else { // doesn't allow other device.
            assert(false);
        }

        close(fd);
    }

    std::optional<std::shared_ptr<Sector>> LinuxFileDriver::readSector(u32 sec_num) noexcept {
        if ((sec_num + 1) * SECTOR_SIZE > device_sz_) {
            return std::nullopt;
        }

        int fd = open(file_path_.c_str(), O_RDONLY);
        assert(fd > 0);
        off_t offset = lseek(fd, sec_num * SECTOR_SIZE, SEEK_SET);
        assert(offset == sec_num * SECTOR_SIZE);
        std::shared_ptr<Sector> sector = std::make_shared<Sector>(sec_num, *this);
        ssize_t cnt = read(fd, (char *) sector->read_ptr(0), SECTOR_SIZE);
        assert(cnt == SECTOR_SIZE);
        assert(close(fd) == 0);
        return {sector};
    }

    bool LinuxFileDriver::writeSectorValue(u32 sec_num, const u8 *buf) noexcept {
        if ((sec_num + 1) * SECTOR_SIZE > device_sz_) {
            return false;
        }

        int fd = open(file_path_.c_str(), O_WRONLY);
        assert(fd > 0);
        off_t offset = lseek(fd, sec_num * SECTOR_SIZE, SEEK_SET);
        assert(offset == sec_num * SECTOR_SIZE);
        ssize_t cnt = write(fd, buf, SECTOR_SIZE);
        assert(cnt == SECTOR_SIZE);
        assert(close(fd) == 0);

        return true;
    }

    /**
     * CacheManger
     * */
    CacheManager::CacheManager(Device &device, u32 cache_sz) noexcept
            : device_{device}, sector_cache_(cache_sz) {}

    std::optional<std::shared_ptr<Sector>> CacheManager::readSector(u32 sec_num) noexcept {}

    bool CacheManager::writeSectorValue(u32 sec_num, const u8 *buf) noexcept {}
}
