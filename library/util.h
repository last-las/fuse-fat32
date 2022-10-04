#ifndef STUPID_FAT32_UTIL_H
#define STUPID_FAT32_UTIL_H

#include <list>
#include <unordered_map>

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

namespace util {
    template<typename key_t, typename value_t>
    class LruCache {
    public:
        void put(key_t &key, value_t &value) noexcept {}

        value_t &get(key_t &key) noexcept {}

        virtual void onDelete(key_t &key, value_t &value) noexcept {}

    private:
        std::unordered_map<key_t, value_t> caches_map_;
    };
} // namespace util

#endif //STUPID_FAT32_UTIL_H
