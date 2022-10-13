#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <optional>
#include <sys/types.h>
#include "utime.h"
#include <vector>
#include "fuse_lowlevel.h"
#include "fs.h"

fs::FAT32fs filesystem;

// If ino is provided, the directory must exist on the filesystem
std::shared_ptr<fs::Directory> getExistDir(fuse_ino_t ino) {
    auto result = filesystem.getDir(ino);
    assert(result.has_value());
    return result.value();
}

// If ino is provided, the directory must exist on the filesystem
std::shared_ptr<fs::File> getExistFile(fuse_ino_t ino) {
    auto result = filesystem.getFile(ino);
    assert(result.has_value());
    return result.value();
}

struct stat read_file_stat(std::shared_ptr<fs::File> file) {
    // TODO
}

std::optional<fat32::FatTimeStamp2> unix_to_dos_ts(struct timespec unix_ts) {
    // TODO
}

fat32::FatTimeStamp2 get_cur_ts() {
    // TODO
}

static void fat32_init(void *userdata, struct fuse_conn_info *conn) {
    // TODO
}

static void fat32_destroy(void *userdata) {
    // TODO
}

static void fat32_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    auto parent_dir = getExistDir(parent);
    auto result = parent_dir->lookupFile(name);
    if (result.has_value()) {
        auto file = result.value();
        struct fuse_entry_param e{};
        e.attr = read_file_stat(file);
        e.ino = file->ino();
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
    auto file = getExistFile(ino);
    auto stat = read_file_stat(file);
    fuse_reply_attr(req, &stat, 1);
}

// The dispatcher for chmod, chown, truncate and utimensat/futimens, some of them should be skipped.
static void fat32_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int valid, struct fuse_file_info *fi) {
    if (valid & (FUSE_SET_ATTR_MODE | FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
        fuse_reply_err(req, EPERM); // chmod, chown cannot be implemented on fat32
        return;
    }
    auto file = getExistFile(ino);
    if (valid & FUSE_SET_ATTR_SIZE) {
        if (!file->truncate(attr->st_size)) {
            fuse_reply_err(req, EPERM);
            return;
        }
    }
    if (valid & FUSE_SET_ATTR_CTIME) {
        auto ts_result = unix_to_dos_ts(attr->st_ctim);
        if (!ts_result.has_value()) {
            fuse_reply_err(req, EINVAL);
            return;
        }
        fat32::FatTimeStamp2 ts2 = ts_result.value();
        file->setCrtTime(ts2);
    }
    if (valid & FUSE_SET_ATTR_ATIME) {
        fat32::FatTimeStamp2 ts2{};
        if (valid & FUSE_SET_ATTR_ATIME_NOW) {
            ts2 = get_cur_ts();
        } else {
            auto ts_result = unix_to_dos_ts(attr->st_atim);
            if (!ts_result.has_value()) {
                fuse_reply_err(req, EINVAL);
                return;
            }
            ts2 = ts_result.value();
        }
        file->setAccTime(ts2.ts);
    }
    if (valid & FUSE_SET_ATTR_MTIME) {
        fat32::FatTimeStamp2 ts2{};
        if (valid & FUSE_SET_ATTR_MTIME_NOW) {
            ts2 = get_cur_ts();
        } else {
            auto ts_result = unix_to_dos_ts(attr->st_mtim);
            if (!ts_result.has_value()) {
                fuse_reply_err(req, EINVAL);
                return;
            }
            ts2 = ts_result.value();
        }
        file->setWrtTime(ts2.ts);
    }
    fuse_reply_err(req, 0);
}

// fat32 doesn't support mode
static void fat32_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t) {
    auto parent_dir = getExistDir(parent);
    auto child_dir = parent_dir->crtDir(name);
    struct fuse_entry_param e{};
    e.attr = read_file_stat(child_dir);
    e.ino = child_dir->ino();
    fuse_reply_entry(req, &e);
}

static void fat32_unlink(fuse_req_t req, fuse_ino_t parent, const char *name) {
    auto parent_dir = getExistDir(parent);
    auto child_result = parent_dir->lookupFile(name);
    if (!child_result.has_value()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    auto child = child_result.value();
    if (child->isDir()) {
        fuse_reply_err(req, EISDIR);
        return;
    }
    assert(parent_dir->delFileEntry(name));
    child->markDeleted();
    fuse_reply_err(req, 0);
}

static void fat32_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
    auto parent_dir = getExistDir(parent);
    auto child_result = parent_dir->lookupFile(name);
    if (!child_result.has_value()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    auto child = child_result.value();
    if (!child->isDir()) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    assert(parent_dir->delFileEntry(name));
    child->markDeleted();
    fuse_reply_err(req, 0);
}

// for simplicity, ignores when `flags` is not empty
static void fat32_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
                         fuse_ino_t newparent, const char *newname, unsigned int flags) {
    if (flags) {
        fuse_reply_err(req, EINVAL);
    }

    auto old_parent = getExistDir(parent);
    auto new_parent = getExistDir(newparent);
    auto old_result = old_parent->lookupFile(name);
    if (!old_result.has_value()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    auto old_file = old_result.value();
    auto new_result = new_parent->lookupFile(newname);
    if (new_result.has_value()) { // newname exists, overwrite
        auto new_file = new_result.value();
        if (old_file->isDir()) {
            if (!new_file->isDir()) {
                fuse_reply_err(req, ENOTDIR);
                return;
            } else if (!std::dynamic_pointer_cast<fs::Directory>(new_file)->isEmpty()) {
                fuse_reply_err(req, ENOTEMPTY);
                return;
            }
        }
        old_parent->delFileEntry(name);
        old_file->renameTo(new_file);
        new_file->markDeleted(); // free clusters occupied by new_file
    } else { // newname doesn't exist
        old_parent->delFileEntry(name);
        auto new_file = new_parent->crtFile(newname);
        old_file->renameTo(new_file);
        new_file->markDeleted(); // free clusters occupied by new_file
    }

    fuse_reply_err(req, 0);
}

static void fat32_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    auto result = filesystem.openFile(ino);
    assert(result.has_value());
    fuse_reply_open(req, fi);
}

static void fat32_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi) {
    // TODO: optimize here... this seems a little dumb
    auto file = getExistFile(ino);
    byte *buf = new byte[size];
    auto cnt = file->read(buf, size, offset);
    fuse_reply_buf(req, buf, cnt);
    delete[] buf;
}

static void fat32_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                        size_t size, off_t off, struct fuse_file_info *fi) {
    auto file = getExistFile(ino);
    auto cnt = file->write(buf, size, off);
    fuse_reply_write(req, cnt);
}

static void fat32_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // Release is called when FUSE is completely done with a file;
    //  at that point, you can free up any temporarily allocated data structures.
    filesystem.closeFile(ino);
    fuse_reply_err(req, 0);
}

static void fat32_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi) {
    auto file = getExistFile(ino);
    file->sync(datasync);
    fuse_reply_err(req, 0);
}

static void fat32_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    auto target = getExistFile(ino);
    if (!target->isDir()) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    assert(filesystem.openFile(ino).has_value());
    fuse_reply_open(req, fi);
}

static void fat32_do_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t offset, struct fuse_file_info *fi, bool plus) {
    auto file = getExistFile(ino);
    if (!file->isDir()) { // do we really need this?
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    auto dir = std::dynamic_pointer_cast<fs::Directory>(file);
    byte *buf = new byte[size];
    byte *p = buf;
    u32 rem = size;
    while (true) {
        auto ret = dir->lookupFileByIndex(offset);
        if (!ret.has_value()) {
            break;
        }
        auto sub_file = ret.value();
        auto state = read_file_stat(sub_file);

        offset += 1;
        u64 entsize;
        if (plus) { // might be wrong..
            struct fuse_entry_param e{};
            e.attr = state;
            e.ino = sub_file->ino();
            entsize = fuse_add_direntry_plus(req, p, rem, sub_file->name(), &e, offset);
        } else {
            entsize = fuse_add_direntry(req, p, rem, sub_file->name(), &state, offset);
        }
        if (entsize > rem) { //does this necessary?
            break;
        }
        p += entsize;
        rem -= entsize;
    }

    fuse_reply_buf(req, buf, size - rem);
    delete[] buf;
}


static void fat32_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi) {
    fat32_do_readdir(req, ino, size, offset, fi,false);
}

static void fat32_readdir_plus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi) {
    fat32_do_readdir(req, ino, size, offset, fi, true);
}

static void fat32_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    auto target = getExistFile(ino);
    if (!target->isDir()) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    filesystem.closeFile(ino);
    fuse_reply_err(req, 0);
}

static void fat32_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi) {
    auto file = getExistFile(ino);
    if (!file->isDir()) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    file->sync(datasync);
    fuse_reply_err(req, 0);
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
        .setattr = fat32_setattr,
        .mkdir = fat32_mkdir,
        .unlink = fat32_unlink,
        .rmdir = fat32_rmdir,
        .rename = fat32_rename,
        .open = fat32_open,
        .read = fat32_read,
        .write = fat32_write,
        .release = fat32_release,
        .fsync = fat32_fsync,
        .opendir = fat32_opendir,
        .readdir = fat32_readdir,
        .releasedir = fat32_releasedir,
        .fsyncdir = fat32_fsyncdir,
        .readdirplus = fat32_readdir_plus,
};

int main(int argc, char *argv[]) {
    // TODO: enable -o default_permissions
    device::LinuxFileDevice dev = device::LinuxFileDevice("/dev/sdb1", 512);
    auto fs = fs::FAT32fs::from(&dev);
    printf("hello world! --fuse\n");
}
