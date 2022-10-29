#ifndef STUPID_FAT32_COMMON_H
#define STUPID_FAT32_COMMON_H

#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <dirent.h>
#include <syscall.h>

#include "gtest/gtest.h"

#include "../library/util.h"

#define SEC_SIZE (0x200 * 0x8)
#define BUF_SIZE (SEC_SIZE * 4)

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


bool is_dot_path(const char *path) {
    if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0) {
        return true;
    } else {
        return false;
    }
}

// a simple version of "rm -rf dirname"
void rmDirRecur(const char *dirname) {
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
                rmDirRecur(d->d_name);
            } else {
                rmFile(d->d_name);
            }
        }
        bpos += d->d_reclen;
    }
    close(dir_fd);
    delete[]buffer;

    // chdir to parent dir and remove dirname
    chdir("..");
    rmDir(dirname);
}


void add_regular_file(const char *regular_file, u64 regular_file_sz) {
    int fd = open(regular_file, O_CREAT, 0644);
    ASSERT_GT(fd, 0);
    ASSERT_EQ(truncate64(regular_file, regular_file_sz), 0);
    ASSERT_EQ(close(fd), 0);
}

// came from "man 4 loop"
bool add_loop_file(char *loop_name, const char *regular_file) {
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
    sprintf(loop_name, "/dev/loop%d", dev_nr);
    int loop_fd = open(loop_name, O_RDWR);
    if (loop_fd == -1) {
        perror("open: loop_name");
        return false;
    }

    // open `regular_file` as backend file
    int bck_fd = open(regular_file, O_RDWR);
    if (bck_fd < 0) {
        perror("open: regular_file");
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

void rm_loop_file(const char *loop_name) {
    int loop_fd = open(loop_name, O_RDONLY);
    if (loop_fd == -1) {
        perror("open: loop_name");
        return;
    }

    if (ioctl(loop_fd, LOOP_CLR_FD) == -1) {
        perror("ioctl-LOOP_CLR_FD");
        return;
    }
}

#endif //STUPID_FAT32_COMMON_H
