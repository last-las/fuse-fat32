#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include "fuse_lowlevel.h"
#include "fs.h"

fs::FAT32fs filesystem;

struct stat read_file_stat(fs::File& file) {
    // TODO
}

static void fat32_init(void *userdata, struct fuse_conn_info *conn) {
    // TODO
}

static void fat32_destroy(void *userdata) {
    // TODO
}

static void fat32_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    auto parent_result = filesystem.getDir(parent);
    assert(parent_result.has_value()); // TODO: impl panic macro and use fs.getDir().value_or() instead.
    auto parent_dir = parent_result.value();
    auto result = parent_dir.lookupFile(name);
    if (result.has_value()) {
        auto file = result.value();
        struct fuse_entry_param e{};
        e.attr = read_file_stat(file);
        e.ino = file.ino();
        // might need to do something about e.attr_timeout, e.entry_timeout
        fuse_reply_entry(req, &e);
    } else {
        fuse_reply_err(req, ENOENT);
    }
}

static void fat32_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup) {
    // TODO: `fat32_forget` is invoked when the file should be wiped out from the disk
}

static void fat32_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    auto result = filesystem.getFile(ino);
    assert(result.has_value());
    auto file = result.value();
    auto stat = read_file_stat(file);
    fuse_reply_attr(req, &stat, 1);
}

static void fat32_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int valid, struct fuse_file_info *fi) {
    // TODO: The dispatcher for chmod, chown, truncate and utimensat/futimens, some of them should be skipped.
    if (valid & (FUSE_SET_ATTR_MODE | FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
        fuse_reply_err(req, EPERM); // chmod, chown cannot be implemented on fat32
    }
    auto result = filesystem.getFile(ino);
    assert(result.has_value());
    auto file = result.value();
    if (valid & FUSE_SET_ATTR_SIZE) {
        if (!file.truncate(attr->st_size)) {
            fuse_reply_err(req, EPERM);
        }
    }
    if (valid & (FUSE_SET_ATTR_ATIME | FUSE_SET_ATTR_MTIME)) {
    }
}

static void fat32_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode) {
    // TODO
}

static void fat32_unlink(fuse_req_t req, fuse_ino_t parent, const char *name) {
    // TODO
}

static void fat32_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
    // TODO
}

static void fat32_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
                         fuse_ino_t newparent, const char *newname, unsigned int flags) {
    // TODO
}

static void fat32_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                        size_t size, off_t off, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO: Release is called when FUSE is completely done with a file;
    //  at that point, you can free up any temporarily allocated data structures.
}

static void fat32_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi) {
    // TODO: write the file contents back to disk
}

static void fat32_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO: The same as `fat32_release`
}

static void fat32_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi) {
    // TODO: write the directory contents back to disk
}

static void fat32_statfs(fuse_req_t req, fuse_ino_t ino) {
    // TODO
}

static void fat32_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets) {
    // TODO
}

static const struct fuse_lowlevel_ops fat32_ll_oper = {
        .init = fat32_init,
        .destroy = fat32_destroy,
        .lookup = fat32_lookup,
        .getattr = fat32_getattr,
};

int main(int argc, char *argv[]) {
    device::LinuxFileDevice dev = device::LinuxFileDevice("/dev/sdb1", 512);
    filesystem = fs::FAT32fs::from(&dev);
    printf("hello world! --fuse\n");
}
