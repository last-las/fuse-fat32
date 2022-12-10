#include <stdint.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "unistd.h"
#include <fcntl.h>
#include <cassert>
#include <cstring>
#include <vector>

#include "device.h"

namespace device {
    /**
     * Sector
     * */
    Sector::Sector(u32 sec_num, u32 sec_sz, Device &device) noexcept
            : sec_num_{sec_num}, sec_sz_{sec_sz}, device_{device}, dirty_(false), value_(std::vector<u8>(sec_sz)) {}

    void Sector::mark_dirty() noexcept {
        dirty_ = true;
    }

    const void *Sector::read_ptr(u32 offset) noexcept {
        assert(offset < sec_sz_); // todo: remove the assert and wrap the result with std::optional
        return &value_[offset];
    }

    void *Sector::write_ptr(u32 offset) noexcept {
        assert(offset < sec_sz_);
        dirty_ = true;
        return &value_[offset];
    }

    void Sector::sync() noexcept {
        if (dirty_) {
            device_.writeSectorValue(sec_num_, &value_[0]);
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
    LinuxFileDriver::LinuxFileDriver(std::string file_path, u32 sec_sz) noexcept
            : file_path_{std::move(file_path)}, sec_sz_{sec_sz} {
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
        if ((sec_num + 1) * sec_sz_ > device_sz_) {
            return std::nullopt;
        }

        int fd = open(file_path_.c_str(), O_RDONLY);
        assert(fd > 0);
        off_t offset = lseek(fd, sec_num * sec_sz_, SEEK_SET);
        assert(offset == sec_num * sec_sz_);
        std::shared_ptr<Sector> sector = std::make_shared<Sector>(sec_num, sec_sz_, *this);
        ssize_t cnt = read(fd, (char *) sector->read_ptr(0), sec_sz_);
        assert(cnt == sec_sz_);
        assert(close(fd) == 0);
        return {sector};
    }

    bool LinuxFileDriver::writeSectorValue(u32 sec_num, const u8 *buf) noexcept {
        if ((sec_num + 1) * sec_sz_ > device_sz_) {
            return false;
        }

        int fd = open(file_path_.c_str(), O_WRONLY);
        assert(fd > 0);
        off_t offset = lseek(fd, sec_num * sec_sz_, SEEK_SET);
        assert(offset == sec_num * sec_sz_);
        ssize_t cnt = write(fd, buf, sec_sz_);
        assert(cnt == sec_sz_);
        assert(close(fd) == 0);

        return true;
    }

    /**
     * CacheManger
     * */
    CacheManager::CacheManager(std::shared_ptr<Device> device, u32 cache_sz) noexcept
            : inner_device_{std::move(device)}, sector_cache_(cache_sz) {}

    std::optional<std::shared_ptr<Sector>> CacheManager::readSector(u32 sec_num) noexcept {
        auto result = sector_cache_.get(sec_num);
        if (result.has_value()) {
            return {result.value()};
        }

        result = inner_device_->readSector(sec_num);
        if (result.has_value()) {
            auto sector = result.value();
            sector->modify_device(*this);
            sector_cache_.put(sec_num, sector);
            return sector;
        } else {
            return std::nullopt;
        }
    }

    bool CacheManager::writeSectorValue(u32 sec_num, const u8 *buf) noexcept {
        return inner_device_->writeSectorValue(sec_num, buf);
    }

    void CacheManager::clear() noexcept {
        sector_cache_.clear();
        inner_device_->clear();
    }

    bool CacheManager::contains(u32 sec_num) noexcept {
        return sector_cache_.get(sec_num).has_value();
    }
}
