#include <fcntl.h>

#include <sys/vfs.h>
#include <cstdlib>

#include "gtest/gtest.h"

#include "fat32.h"
#include "device.h"
#include "common.h"

std::shared_ptr<device::LinuxFileDriver> device_;
fat32::BPB bpb;
static std::vector<u8> fat_snapshot;
static u32 start_sec_no_;
static u32 fat_sec_no_;
static u32 fat_num_;

const u32 KSNameCnt = 12;
std::string short_names[KSNameCnt] = {
        "SHORT",
        "SHORT.EXT",
        "DAMNLONG",
        "DAMNLONG.EXT",
        "1.23",
        "1.234",
        "SPACE.TXT",
        "DOT.TXT",
        "SPACE_AN.TXT",
        "A.TXT",
        "\xc4\xe3\xba\xc3\xca\xc0\xbd\xe7.TXT",
        "\xc4\xe3\xba\xc3\x31\xca\xc0\xbd.TXT",
};
const u32 KLNameCnt = 14;
std::string long_names[KLNameCnt] = {
        "short",
        "short.ext",
        "damnLongName",
        "damnLongName.ext",
        "1.23",
        "1.2345",
        " space.txt ",
        ".dot.txt.",
        " ..space_and_dot.txt.. ",
        "a.b.c.txt",
        "你好世界.txt",
        "你好1世界.txt",
        "[123].[a]",
        "+,;=[]1.txt",
};
fat32::BasisName basis_names[KLNameCnt] = {
        {"SHORT   ",                         "   "},
        {"SHORT   ",                         "EXT"},
        {"DAMNLONG",                         "   "},
        {"DAMNLONG",                         "EXT"},
        {"1       ",                         "23 "},
        {"1       ",                         "234"},
        {"SPACE   ",                         "TXT"},
        {"DOT     ",                         "TXT"},
        {"SPACE_AN",                         "TXT"},
        {"A       ",                         "TXT"}, // Note that on win10, the "a.b.c.txt" is converted to "ABC.TXT".
        {"\xc4\xe3\xba\xc3\xca\xc0\xbd\xe7", "TXT"},
        {"\xc4\xe3\xba\xc3\x31\xca\xc0\xbd", "TXT"},
        {"_123_   ",                         "_A_"},
        {"______1 ",                         "TXT"},
};
const u8 fst_entry_data[32] = {
        0x01, 0x64, 0x00, 0x61, 0x00, 0x6D, 0x00, 0x6E, 0x00, 0x6C, 0x00, 0x0F, 0x00, 0x66, 0x6F, 0x00,
        0x6E, 0x00, 0x67, 0x00, 0x6E, 0x00, 0x61, 0x00, 0x6D, 0x00, 0x00, 0x00, 0x65, 0x00, 0x2E, 0x00
};
const u8 lst_entry_data[32] = {
        0x42, 0x74, 0x00, 0x78, 0x00, 0x74, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x0F, 0x00, 0x66, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF
};

/**
 * The test case requires mkfs.fat to be installed.
 * */
class TestFat32Env : public testing::Environment, Fat32Filesystem {
public:
    TestFat32Env() {
        device_ = std::make_shared<device::LinuxFileDriver>(regular_file, SECTOR_SIZE);
        bpb = *(fat32::BPB *) device_->readSector(0).value()->read_ptr(0);
        start_sec_no_ = fat32::getFirstFATSector(bpb, 0);
        fat_sec_no_ = bpb.BPB_FATsz32;
        fat_num_ = bpb.BPB_num_fats;

        // take snapshot of FAT
        fat_snapshot = std::vector<u8>(fat_sec_no_ * fat_num_ * SECTOR_SIZE);
        u8 *snapshot_ptr = &fat_snapshot[0];
        for (u32 sec_no = start_sec_no_; sec_no < start_sec_no_ + fat_sec_no_ * fat_num_; sec_no++) {
            auto sector = device_->readSector(sec_no).value();
            memcpy(snapshot_ptr, sector->read_ptr(0), SECTOR_SIZE);
            snapshot_ptr += SECTOR_SIZE;
        }
    }
};

static bool isOriginFAT() {
    device::LinuxFileDriver device(regular_file, SECTOR_SIZE);
    u8 const *snapshot_ptr = &fat_snapshot[0];
    for (u32 sec_no = start_sec_no_; sec_no < start_sec_no_ + fat_sec_no_ * fat_num_; sec_no++) {
        auto sector = device.readSector(sec_no).value();
        if (memcmp(snapshot_ptr, sector->read_ptr(0), SECTOR_SIZE) != 0) {
            return false;
        }
        snapshot_ptr += SECTOR_SIZE;
    }
    return true;
}


TEST(FAT32Test, StructSz) {
    ASSERT_EQ(sizeof(fat32::BPB), 90);
    ASSERT_EQ(sizeof(fat32::FSInfo), 512);
    ASSERT_EQ(sizeof(fat32::ShortDirEntry), 32);
    ASSERT_EQ(sizeof(fat32::LongDirEntry), 32);
}

TEST(FAT32Test, IllegalChr) {
    ASSERT_TRUE(fat32::isInvalidShortEntryChr('\\'));
    ASSERT_TRUE(fat32::isInvalidShortEntryChr('/'));
    ASSERT_TRUE(fat32::isInvalidShortEntryChr(':'));
    ASSERT_TRUE(fat32::isInvalidShortEntryChr('*'));
    ASSERT_TRUE(fat32::isInvalidShortEntryChr('?'));
    ASSERT_TRUE(fat32::isInvalidShortEntryChr('\"'));
    ASSERT_TRUE(fat32::isInvalidShortEntryChr('<'));
    ASSERT_TRUE(fat32::isInvalidShortEntryChr('>'));
    ASSERT_TRUE(fat32::isInvalidShortEntryChr('|'));
    ASSERT_TRUE(fat32::isInvalidShortEntryChr('.'));
    ASSERT_FALSE(fat32::isInvalidShortEntryChr('\x05'));
}

TEST(FAT32Test, EntryClusNo) {
    fat32::ShortDirEntry short_dir_entry{};
    u32 clus_no = 0xdeadbeef;
    setFstClusNo(short_dir_entry, clus_no);
    ASSERT_EQ(short_dir_entry.fst_clus_high, 0xdead);
    ASSERT_EQ(short_dir_entry.fst_clus_low, 0xbeef);
    ASSERT_EQ(readEntryClusNo(short_dir_entry), clus_no);
}

TEST(FAT32Test, readShortDirEntryName) {
    u8 data[32] = {
            0x53, 0x48, 0x4F, 0x52, 0x54, 0x20, 0x20, 0x20, 0x4E, 0x4F, 0x20, 0x20, 0x18, 0xB9, 0x30, 0x8B,
            0x81, 0x55, 0x81, 0x55, 0x00, 0x00, 0x31, 0x8B, 0x81, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    auto *short_dir_entry = (fat32::ShortDirEntry *) &data;
    std::string read_name = fat32::readShortEntryName(*short_dir_entry);
    ASSERT_STREQ("SHORT.NO", read_name.c_str());
}

TEST(FAT32Test, readLongDirEntryName) {
    util::string_utf8 name = "damnlongname.txt";
    auto *fst_dir_entry = (fat32::LongDirEntry *) &fst_entry_data;
    auto *lst_dir_entry = (fat32::LongDirEntry *) &lst_entry_data;

    util::string_utf16 read_utf16_name;
    read_utf16_name += fat32::readLongEntryName(*fst_dir_entry);
    read_utf16_name += fat32::readLongEntryName(*lst_dir_entry);
    util::string_utf8 read_utf8_name = util::utf16ToUtf8(read_utf16_name).value();

    ASSERT_STREQ(name.c_str(), read_utf8_name.c_str());
}

TEST(FAT32Test, unixDosCvt) {
    timespec unix_ts;
    ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &unix_ts), 0);
    fat32::FatTimeStamp dos_ts = fat32::unix2DosTs(unix_ts);
    timespec parsed_unix_ts = fat32::dos2UnixTs(dos_ts);
    timespec parsed_unix_ts2 = fat32::dos2UnixTs(fat32::unix2DosTs(parsed_unix_ts));

    time_t gap = unix_ts.tv_sec - parsed_unix_ts.tv_sec;
    ASSERT_TRUE(gap == 0 || gap == 1);
    ASSERT_EQ(parsed_unix_ts.tv_sec, parsed_unix_ts2.tv_sec);
}

TEST(FAT32Test, unixDosCvt2) {
    timespec unix_ts;
    ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &unix_ts), 0);
    fat32::FatTimeStamp2 dos_ts = fat32::unix2DosTs_2(unix_ts);
    timespec parsed_unix_ts = fat32::dos2UnixTs_2(dos_ts);
    timespec parsed_unix_ts2 = fat32::dos2UnixTs_2(fat32::unix2DosTs_2(parsed_unix_ts));

    time_t sec_gap = unix_ts.tv_sec - parsed_unix_ts.tv_sec;
    ASSERT_TRUE(sec_gap == 0 || sec_gap == 1);
    ASSERT_EQ(unix_ts.tv_nsec / 10000000, parsed_unix_ts.tv_nsec / 10000000);
    ASSERT_EQ(parsed_unix_ts.tv_sec, parsed_unix_ts2.tv_sec);
}

void assert_basis_name_eq(fat32::BasisName &a, fat32::BasisName &b) {
    ASSERT_STREQ(a.primary, b.primary);
    ASSERT_STREQ(a.extension, b.extension);
}

TEST(FAT32Test, chkSum) {
    fat32::BasisName basis_name = {"DAMNLO~1", "TXT"};
    u8 expected_chk_sum = 0x66;
    u8 calc_chk_sum = fat32::chkSum(basis_name);
    ASSERT_EQ(calc_chk_sum, expected_chk_sum);
}

TEST(FAT32Test, mkLongDirEntry) {
    util::string_utf8 utf8_name = "damnlongname.txt";
    util::string_utf16 utf16_name = util::utf8ToUtf16(utf8_name).value();
    u8 chk_sum = 0x66;
    fat32::LongDirEntry fst_dir_entry = fat32::mkLongDirEntry(false, 1, chk_sum, utf16_name, 0);
    fat32::LongDirEntry lst_dir_entry = fat32::mkLongDirEntry(true, 2, chk_sum, utf16_name, 26);
    ASSERT_EQ(strncmp((const char *) &fst_entry_data[0], (const char *) &fst_dir_entry, 32), 0);
    ASSERT_EQ(strncmp((const char *) &lst_entry_data[0], (const char *) &lst_dir_entry, 32), 0);
}

TEST(FAT32Test, genBasisNameFromShort) {
    fat32::BasisName gen_basis_name;
    for (u32 i = 0; i < KSNameCnt; ++i) {
        gen_basis_name = fat32::genBasisNameFromShort(short_names[i]);
        assert_basis_name_eq(gen_basis_name, basis_names[i]);
    }
}

TEST(FAT32Test, genBasisNameFromLong) {
    fat32::BasisName gen_basis_name;
    for (u32 i = 0; i < KLNameCnt; ++i) {
        gen_basis_name = fat32::genBasisNameFromLong(long_names[i]);
        assert_basis_name_eq(gen_basis_name, basis_names[i]);
    }
}

TEST(FAT32Test, AvailClusCnt) {
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device_);
    struct statfs fs_stat{};
    ASSERT_EQ(statfs(mnt_point, &fs_stat), 0);

    // check
    u64 clus_cnt = fat.availClusCnt() + 2; // linux fat driver seems to treat the two reserved clusters as available
    ASSERT_EQ(clus_cnt * bpb.BPB_sec_per_clus * SECTOR_SIZE, fs_stat.f_bsize * fs_stat.f_bfree);

    // alloc, free ant check again
    auto clus_chain = fat.allocClus(100).value();
    fat.freeClus(clus_chain[0]);
    clus_cnt = fat.availClusCnt() + 2;
    ASSERT_EQ(clus_cnt * bpb.BPB_sec_per_clus * SECTOR_SIZE, fs_stat.f_bsize * fs_stat.f_bfree);
}

TEST(FAT32Test, AllocFree) {
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device_);
    std::vector<u32> clus_chain;
    u32 fst_clus;
    u64 avail_clus_cnt = fat.availClusCnt();

    // small alloc
    clus_chain = fat.allocClus(10).value();
    fst_clus = clus_chain[0];
    fat.freeClus(fst_clus);
    ASSERT_TRUE(isOriginFAT());

    // large alloc
    clus_chain = fat.allocClus(avail_clus_cnt).value();
    fst_clus = clus_chain[0];
    fat.freeClus(fst_clus);
    ASSERT_TRUE(isOriginFAT());

    // random alloc
    ASSERT_EQ(fat.allocClus(100).value()[0], 3);
    ASSERT_EQ(fat.allocClus(100).value()[0], 103);
    ASSERT_EQ(fat.allocClus(100).value()[0], 203);
    fat.freeClus(103);
    ASSERT_EQ(fat.allocClus(50).value()[0], 103);

    fat.freeClus(3);
    fat.freeClus(103);
    fat.freeClus(203);
    ASSERT_TRUE(isOriginFAT());

    // failed alloc
    ASSERT_FALSE(fat.allocClus(avail_clus_cnt + 1).has_value());

    // free empty cluster
    fat.freeClus(0);
    fat.freeClus(1);
    ASSERT_TRUE(isOriginFAT());

    // check available cnt
    ASSERT_EQ(avail_clus_cnt, fat.availClusCnt());
}

TEST(FAT32Test, SetFreeCount) {
    u32 free_clus_no = 5;
    fat32::FAT fat = fat32::FAT(bpb, free_clus_no, device_);

    auto clus_chain = fat.allocClus(5).value();
    ASSERT_EQ(clus_chain[0], 5);
    ASSERT_EQ(clus_chain[1], 3);
    ASSERT_EQ(clus_chain[2], 4);
    ASSERT_EQ(clus_chain[3], 6);
    ASSERT_EQ(clus_chain[4], 7);

    // clear
    fat.freeClus(clus_chain[0]);
    ASSERT_TRUE(isOriginFAT());
}

TEST(FAT32Test, ReadClusChain) {
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device_);
    std::vector<u32> clus_chain, read_clus_chain;

    // read empty clus chains
    clus_chain = fat.readClusChains(0);
    ASSERT_TRUE(clus_chain.empty());

    // read valid clus chains
    clus_chain = fat.allocClus(5).value();
    read_clus_chain = fat.readClusChains(clus_chain[0]);
    ASSERT_EQ(clus_chain, read_clus_chain);

    // clear
    fat.freeClus(clus_chain[0]);
    ASSERT_TRUE(isOriginFAT());
}

TEST(FAT32Test, Resize) {
    fat32::FAT fat = fat32::FAT(bpb, 0xffffffff, device_);
    std::vector<u32> expect_result;

    u32 fst_clus = fat.allocClus(5).value()[0];
    // shrink
    ASSERT_TRUE(fat.resize(fst_clus, 2, false));
    expect_result = {3, 4};
    ASSERT_EQ(fat.readClusChains(fst_clus), expect_result);
    // grow
    ASSERT_TRUE(fat.resize(fst_clus, 7, false));
    expect_result = {3, 4, 5, 6, 7, 8, 9};
    ASSERT_EQ(fat.readClusChains(fst_clus), expect_result);

    // clear
    fat.freeClus(fst_clus);
    ASSERT_TRUE(isOriginFAT());

    // special case when fst_clus eq 0
    fst_clus = 0;
    {
        // resize 0
        ASSERT_TRUE(fat.resize(fst_clus, 0, false));
        ASSERT_EQ(fst_clus, 0);
        ASSERT_TRUE(isOriginFAT());

        // resize non-zero
        ASSERT_TRUE(fat.resize(fst_clus, 5, false));
        ASSERT_EQ(fst_clus, 3);

        // resize 0 again
        ASSERT_TRUE(fat.resize(fst_clus, 0, false));
        ASSERT_EQ(fst_clus, 0);
        ASSERT_TRUE(isOriginFAT());
    }
}


int main(int argc, char **argv) {
    testing::AddGlobalTestEnvironment(new TestFat32Env);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
