#ifndef STUPID_FAT32_UTIL_H
#define STUPID_FAT32_UTIL_H

#include <list>
#include <unordered_map>

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef char byte;
typedef u32 size;

namespace util {
    using std::shared_ptr;

    template<typename key_t, typename value_t>
    class LRUCache {
        typedef shared_ptr<key_t> key_ptr;
        typedef shared_ptr<value_t> value_ptr;
    public:
        explicit LRUCache(u32 max_size) noexcept: max_size_{max_size} {}

        void put(key_ptr key, value_ptr value) noexcept {}

        std::optional<value_ptr> get(key_ptr key) noexcept {}

    private:
        u32 max_size_;
        std::unordered_map<key_ptr, std::pair<u32, value_ptr>> caches_map_;
        std::list<key_ptr> fifo_queue_;
    };
} // namespace util

#endif //STUPID_FAT32_UTIL_H
