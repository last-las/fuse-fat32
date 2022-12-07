#ifndef STUPID_FAT32_UTIL_H
#define STUPID_FAT32_UTIL_H

#include <list>
#include <optional>
#include <unordered_map>

// todo: move to namespace util
typedef unsigned long long u64;
typedef long long i64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef char byte; // todo: byte should not be char but unsigned char!
typedef u32 size;

namespace util {
    typedef std::string string_utf8;
    typedef std::string string_gbk;
    typedef std::string string_utf16;

    template<typename key_t, typename value_t>
    class LRUCacheMap {
    public:
        typedef std::pair<key_t, value_t> key_value_pair_t;
        typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;

        explicit LRUCacheMap(u32 max_size) noexcept: max_size_{max_size} {}

        void put(key_t key, value_t value) noexcept {
            auto it = caches_map_.find(key);
            if (it != caches_map_.end()) {
                caches_map_.erase(key);
                key_value_list_.erase(it->second);
            }
            key_value_list_.push_front(std::pair(key, value));
            caches_map_[key] = key_value_list_.begin();

            if (caches_map_.size() > max_size_) {
                auto removed_item = key_value_list_.end();
                removed_item--;
                caches_map_.erase(removed_item->first);
                key_value_list_.erase(removed_item);
            }
        }

        std::optional<value_t> get(key_t key) noexcept {
            auto it = caches_map_.find(key);
            if (it != caches_map_.end()) {
                // move the item in the front of `key_value_list_`
                auto begin = key_value_list_.begin();
                key_value_list_.splice(begin, key_value_list_, it->second);

                return std::optional(it->second->second);
            } else {
                return std::nullopt;
            }
        }

        std::optional<value_t> remove(key_t key) noexcept {
            auto it = caches_map_.find(key);
            if (it != caches_map_.end()) {
                auto value = it->second->second;
                caches_map_.erase(it);
                key_value_list_.erase(it->second);
                return {value};
            } else {
                return std::nullopt;
            }
        }

        u64 size() noexcept {
            return key_value_list_.size();
        }

        typename std::list<key_value_pair_t>::const_iterator begin() {
            return key_value_list_.begin();
        }

        typename std::list<key_value_pair_t>::const_iterator end() {
            return key_value_list_.end();
        }

    private:
        u32 max_size_;
        std::unordered_map<key_t, list_iterator_t> caches_map_;
        std::list<key_value_pair_t> key_value_list_;
    };

    std::optional<string_gbk> utf8ToGbk(string_utf8 &utf8_str) noexcept;

    std::optional<string_utf8> gbkToUtf8(string_gbk &gbk_str) noexcept;

    std::optional<string_utf16> utf8ToUtf16(string_utf8 &utf8_str) noexcept;

    std::optional<string_utf8> utf16ToUtf8(string_utf16 &utf16_str) noexcept;

    void toUpper(string_utf8 &utf8_str) noexcept;

    void lstrip(std::string &s, char a) noexcept;

    void rstrip(std::string &s, char a) noexcept;

    void strip(std::string &s, char a) noexcept;

    std::string getFullPath(std::string file_path) noexcept;
} // namespace util

#endif //STUPID_FAT32_UTIL_H
