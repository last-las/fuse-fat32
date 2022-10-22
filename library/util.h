#ifndef STUPID_FAT32_UTIL_H
#define STUPID_FAT32_UTIL_H

#include <list>
#include <optional>
#include <unordered_map>

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef char byte;
typedef u32 size;

namespace util {
    template<typename key_t, typename value_t>
    class LRUCache {
    public:
        explicit LRUCache(u32 max_size) noexcept: max_size_{max_size} {}

        void put(key_t key, value_t value) noexcept {
            if (caches_map_.find(key) != caches_map_.end()) {
                caches_map_.erase(key);
                key_queue_.remove(key);
            }
            caches_map_.insert(key, value);
            key_queue_.push_back(key);

            if (caches_map_.size() > max_size_) {
                auto removed_key = key_queue_.begin();
                caches_map_.erase(*removed_key);
                key_queue_.pop_front();
            }
        }

        std::optional<value_t> get(key_t key) noexcept {
            if (caches_map_.find(key) != caches_map_.end()) {
                return std::optional(caches_map_[key]);
            } else {
                return std::nullopt;
            }
        }

    private:
        u32 max_size_;
        std::unordered_map<key_t, value_t> caches_map_;
        std::list<key_t> key_queue_;
    };
} // namespace util

#endif //STUPID_FAT32_UTIL_H
