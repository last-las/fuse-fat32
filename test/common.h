#ifndef STUPID_FAT32_COMMON_H
#define STUPID_FAT32_COMMON_H

#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <dirent.h>
#include <syscall.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <unistd.h>

#include "gtest/gtest.h"

#include "../library/util.h"

#define SEC_SIZE (0x200 * 0x8)
#define BUF_SIZE (SEC_SIZE * 4)

u64 block_sz = 1024 * 1024 * 512; // 512MB
char regular_file[] = "regular_file";
char loop_name[512];
char mnt_point[] = "fat32_mnt";

struct linux_dirent {
    long d_ino;
    off_t d_off;
    unsigned short d_reclen;
    char d_name[];
};

void crtFileOrExist(const char *filename) {
    int fd = open(filename, O_CREAT, 0644);
    if (fd == -1 && errno != 0) {
        printf("Fail to create file %s: %s\n", filename, strerror(errno));
        exit(1);
    }
    close(fd);
}

void crtDirOrExist(const char *dirname) {
    int ret = mkdir(dirname, 0755);
    if (ret == -1 && errno != 0) {
        printf("Fail to create dir %s: %s\n", dirname, strerror(errno));
        exit(1);
    }
}

// Remove as more files as possible, so do not exit when unlink fails.
void rmFile(const char *filename) {
    int ret = unlink(filename);
    if (ret == -1 && errno != 0) {
        printf("Fail to remove file %s: %s\n", filename, strerror(errno));
    }
}

// Remove as more directories as possible, so do not exit when unlink fails
void rmDir(const char *dirname) {
    int ret = rmdir(dirname);
    if (ret == -1 && errno != 0) {
        printf("Fail to remove dir %s: %s\n", dirname, strerror(errno));
    }
}

ssize_t readFile(const char *filename, char *buf, u32 size, u32 offset) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printf("Fail to open file %s: %s\n", filename, strerror(errno));
        return -1;
    }
    if (offset > 0 && lseek(fd, offset, SEEK_SET) == -1) {
        printf("Fail to lseek file %s: %s\n", filename, strerror(errno));
        close(fd);
        return -1;
    }
    ssize_t cnt = read(fd, buf, size);
    if (cnt == -1) {
        printf("Fail to read file %s: %s\n", filename, strerror(errno));
    }
    close(fd);
    sync();
    return cnt;
}

ssize_t writeFile(const char *filename, const char *buf, u32 size, u32 offset) {
    int fd = open(filename, O_WRONLY);
    if (fd == -1) {
        printf("Fail to open file %s: %s\n", filename, strerror(errno));
        return -1;
    }
    if (offset > 0 && lseek(fd, offset, SEEK_SET) == -1) {
        printf("Fail to lseek file %s: %s\n", filename, strerror(errno));
        close(fd);
        return -1;
    }
    ssize_t cnt = write(fd, buf, size);
    if (cnt == -1) {
        printf("Fail to write file %s: %s\n", filename, strerror(errno));
    }
    close(fd);
    sync();
    return cnt;
}

std::optional<struct stat> readFileStat(const char *filename) {
    struct stat stat_buf{};
    int ret = stat(filename, &stat_buf);
    if (ret == -1) {
        printf("Fail to read stat of file %s: %s\n", filename, strerror(errno));
        return std::nullopt;
    }

    return stat_buf;
}

// return 1, 0, -1 to indicate x is greater, equal or less than y.
int unixTsCmp(const struct timespec &x, const struct timespec &y) {
    if (x.tv_sec == y.tv_sec) {
        if (x.tv_nsec == y.tv_nsec) {
            return 0;
        } else {
            return x.tv_nsec > y.tv_nsec ? 1 : -1;
        }
    } else {
        return x.tv_sec > y.tv_sec ? 1 : -1;
    }
}


bool is_dot_path(const char *path) {
    if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0) {
        return true;
    } else {
        return false;
    }
}

// a simple version of "rm -rf dirname"
void rmDirRecur(const char *dirname, bool rm_root) {
    // open dirname and chdir to it
    int dir_fd = open(dirname, O_DIRECTORY);
    if (dir_fd == -1) {
        return;
    }
    chdir(dirname);

    // remove the sub files recursively
    struct linux_dirent *d;
    char *buffer = new char[BUF_SIZE];
    int nread = syscall(SYS_getdents, dir_fd, buffer, BUF_SIZE);
    for (int bpos = 0; bpos < nread;) {
        d = (struct linux_dirent *) (buffer + bpos);
        if (!is_dot_path(d->d_name)) {
            struct stat fstat_{};
            stat(d->d_name, &fstat_);
            if (fstat_.st_mode & S_IFDIR) {
                rmDirRecur(d->d_name, true);
            } else {
                rmFile(d->d_name);
            }
        }
        bpos += d->d_reclen;
    }
    close(dir_fd);
    delete[]buffer;

    // chdir to parent dir
    chdir("..");
    if (rm_root) {
        rmDir(dirname);
    }
}


void add_regular_file(const char *reg_file, u64 reg_file_sz) {
    int fd = open(reg_file, O_CREAT, 0644);
    ASSERT_GT(fd, 0);
    ASSERT_EQ(truncate64(reg_file, reg_file_sz), 0);
    ASSERT_EQ(close(fd), 0);
}

// came from "man 4 loop"
bool add_loop_file(char *loop_name_, const char *regular_file_) {
    // find a free loop device
    int loop_ctl_fd = open("/dev/loop-control", O_RDWR);
    if (loop_ctl_fd == -1) {
        perror("open: /dev/loop-control");
        return false;
    }
    int dev_nr = ioctl(loop_ctl_fd, LOOP_CTL_GET_FREE);
    if (dev_nr == -1) {
        perror("ioctl-LOOP_CTL_GET_FREE");
        return false;
    }
    sprintf(loop_name_, "/dev/loop%d", dev_nr);
    int loop_fd = open(loop_name_, O_RDWR);
    if (loop_fd == -1) {
        perror("open: loop_name_");
        return false;
    }

    // open `regular_file_` as backend file
    int bck_fd = open(regular_file_, O_RDWR);
    if (bck_fd < 0) {
        perror("open: regular_file_");
        return false;
    }

    // loop
    if (ioctl(loop_fd, LOOP_SET_FD, bck_fd) == -1) {
        perror("ioctl-LOOP_SET_FD");
    }

    close(loop_fd);
    close(bck_fd);
    return true;
}

void rm_loop_file(const char *loop_name_) {
    int loop_fd = open(loop_name_, O_RDONLY);
    if (loop_fd == -1) {
        perror("open: loop_name_");
        return;
    }

    if (ioctl(loop_fd, LOOP_CLR_FD) == -1) {
        perror("ioctl-LOOP_CLR_FD");
        return;
    }
}

int wrappedUmount(const char *file_path) {
    errno = 0;
    int times = 3;
    sync();
    while (umount(file_path) != 0 && times > 0) {
        if (errno == 16) { // device busy
            usleep(300000);
            times--;
        } else {
            return -1;
        }
    }

    if (times > 0) {
        return 0;
    } else {
        return -1;
    }
}

void assert_file_exist(const char *file) {
    int fd = open(file, O_RDONLY);
    ASSERT_GT(fd, 0);
    ASSERT_EQ(close(fd), 0);
}

void assert_dir_exist(const char *dir) {
    int dir_fd = open(dir, O_DIRECTORY);
    ASSERT_GT(dir_fd, 0);
    ASSERT_EQ(close(dir_fd), 0);
}

void assert_file_content(const char *file, const char *buf, u32 buf_size) {
    char tmp_buf[200];
    int s_fd = open(file, O_RDONLY);
    ASSERT_GT(s_fd, 0);
    ASSERT_EQ(read(s_fd, tmp_buf, 200), buf_size);
    ASSERT_EQ(strncmp(buf, tmp_buf, buf_size), 0);
    close(s_fd);
}

void assert_file_non_exist(const char *file) {
    errno = 0;
    int fd = open(file, O_RDONLY);
    ASSERT_EQ(fd, -1);
    ASSERT_EQ(errno, ENOENT);
}

void assert_dir_non_exist(const char *dir) {
    errno = 0;
    int fd = open(dir, O_DIRECTORY);
    ASSERT_EQ(fd, -1);
    ASSERT_EQ(errno, ENOENT);
}

int create_file_ret_fd(const char *file) {
    int fd = open(file, O_CREAT, 0644);
    EXPECT_GT(fd, 0);
    return fd;
}

void create_file(const char *file) {
    int fd = create_file_ret_fd(file);
    ASSERT_EQ(close(fd), 0);
}

void create_dir(const char *dir) {
    ASSERT_EQ(mkdir(dir, 0755), 0) << strerror(errno);
}

void rm_file(const char *file) {
    ASSERT_EQ(unlink(file), 0) << strerror(errno);

}

void rm_dir(const char *dir) {
    ASSERT_EQ(rmdir(dir), 0) << strerror(errno);
}

class Fat32Filesystem {
public:
    Fat32Filesystem() {
        if (block_sz < 512 * 1024 * 1024) {
            printf("mk.fat will create fat16 when fat sz is less than 512MB!");
            exit(1);
        }
        // create and mount a fat32 filesystem
        add_regular_file(regular_file, block_sz);
        if (!add_loop_file(loop_name, regular_file)) {
            printf("Add loop file failed, skip the following tests\n");
            exit(1);
        }
        crtDirOrExist(mnt_point);
        mkfs_fat_on(loop_name);
        if (mount(loop_name, mnt_point, "vfat", 0, nullptr) != 0) {
            printf("Mount failed, skip the following tests\n");
            exit(1);
        }
    }

    virtual ~Fat32Filesystem() {
        rmDirRecur(mnt_point, false);
        if (wrappedUmount(mnt_point) != 0) {
            printf("umount errno:%d %s\n", errno, strerror(errno));
        }
        rm_loop_file(loop_name);
        rmDir(mnt_point);
        // todo: rmFile(regular_file);
    }

private:
    static void mkfs_fat_on(const char *device) {
        char command[64];
        sprintf(command, "mkfs.fat %s", device);
        int ret = system(command);
        ASSERT_EQ(ret, 0);
    }
};

#endif //STUPID_FAT32_COMMON_H
