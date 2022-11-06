#include <string>
#include <cstring>
#include <iconv.h>

#include "util.h"

namespace util {
    std::optional<string_gbk> utf8ToGbk(string_utf8 &utf8_str) noexcept {
        iconv_t cd = iconv_open("gbk", "utf8");
        size_t src_len = utf8_str.length();
        size_t dst_len = src_len * 2; // max possible gbk str size
        string_gbk gbk_str(dst_len, 0);
        byte *src_ptr = (byte *) utf8_str.c_str();
        byte *dst_ptr = (byte *) gbk_str.c_str();
        size_t remained_dst_len = dst_len;

        std::optional<string_gbk> result = std::nullopt;
        if (iconv(cd, &src_ptr, &src_len, &dst_ptr, &remained_dst_len) == -1) {
            goto end;
        }

        gbk_str.resize(dst_len - remained_dst_len);
        result = {std::move(gbk_str)};
        end:
        iconv_close(cd);
        return result;
    }

    std::optional<string_utf8> gbkToUtf8(string_gbk &gbk_str) noexcept {
        iconv_t cd = iconv_open("utf8", "gbk");
        size_t src_len = gbk_str.length();
        size_t dst_len = src_len * 4; // max possible utf str size
        string_utf8 utf8_str(dst_len, 0);
        byte *src_ptr = (byte *) gbk_str.c_str();
        byte *dst_ptr = (byte *) utf8_str.c_str();
        size_t remained_dst_len = dst_len;

        std::optional<string_utf8> result = std::nullopt;
        if (iconv(cd, &src_ptr, &src_len, &dst_ptr, &remained_dst_len) == -1) {
            goto end;
        }

        utf8_str.resize(dst_len - remained_dst_len);
        result = {std::move(utf8_str)};
        end:
        iconv_close(cd);
        return result;
    }
}