#define FUSE_USE_VERSION 34

#include "fuse_lowlevel.h"

static void fat32_init(void *userdata, struct fuse_conn_info *conn) {
    // TODO
}

static void fat32_destroy(void* userdata) {
    // TODO
}

static void fat32_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    // TODO
}

static void fat32_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev) {
    // TODO
}

static void fat32_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode) {
    // TODO
}

static void fat32_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name) {
    // TODO
}

static void fat32_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t parent, const char *name) {
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

static void fat32_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup) {
    // TODO
}

static void fat32_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets) {
    // TODO
}

static void fat32_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int valid, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_readlink(fuse_req_t req, fuse_ino_t ino) {
    // TODO
}

static void fat32_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_write_buf(fuse_req_t req, fuse_ino_t ino,
                         struct fuse_bufvec *in_buf, off_t off,
                         struct fuse_file_info *fi) {
    // TODO
}

static void fat32_statfs(fuse_req_t req, fuse_ino_t ino) {
    // TODO
}

static void fat32_fallocate(fuse_req_t req, fuse_ino_t ino, int mode,
                         off_t offset, off_t length, struct fuse_file_info *fi) {
    // TODO
}

static void fat32_flock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op) {
    // TODO
}

static void fat32_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size) {
    // TODO
}

static void fat32_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size) {
    // TODO
}

static void fat32_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags) {
    // TODO
}

static void fat32_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name) {
    // TODO
}

static void fat32_lseek(fuse_req_t req, fuse_ino_t ino, off_t off, int whence, struct fuse_file_info *fi) {
    // TODO
}

static const struct fuse_lowlevel_ops fat32_ll_oper = {
        .init = fat32_init,
        .destroy = fat32_destroy,
};

int main(int argc, char *argv[]) {
}
