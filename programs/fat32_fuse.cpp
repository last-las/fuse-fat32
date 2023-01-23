#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <optional>
#include <sys/types.h>
#include <sys/vfs.h>
#include "utime.h"
#include <vector>
#include <fuse_i.h>
#include "fuse_lowlevel.h"
#include "fuse_common.h"
#include "fs.h"
#include "cmdline.h"

using util::u8, util::u16, util::u32, util::i64, util::u64;

std::unique_ptr<fs::FAT32fs> filesystem;


// If ino is provided, the directory must exist on the filesystem
std::shared_ptr<fs::Directory> getExistDir(fuse_ino_t ino) {
    if (ino == 1) {
        ino = 0;
    }
    auto result = filesystem->getDirByIno(ino);
    assert(result.has_value());
    return result.value();
}

// If ino is provided, the directory must exist on the filesystem
std::shared_ptr<fs::File> getExistFile(fuse_ino_t ino) {
    if (ino == 1) {
        ino = 0;
    }
    auto result = filesystem->getFileByIno(ino);
    assert(result.has_value());
    return result.value();
}

bool isFileType(mode_t mode, mode_t flag) {
    return (mode & S_IFMT) == flag;
}

bool isIllegalFat32FileType(mode_t mode) {
    if (isFileType(mode, S_IFSOCK) | isFileType(mode, S_IFLNK) | isFileType(mode, S_IFBLK) |
        isFileType(mode, S_IFDIR) | isFileType(mode, S_IFCHR) | isFileType(mode, S_IFIFO)) {
        return true;
    } else {
        return false;
    }
}

struct stat readFileStat(std::shared_ptr<fs::File> file) {
    struct stat file_stat;
    file_stat.st_dev = 0;
    file_stat.st_ino = file->ino();
    file_stat.st_mode = file->isDir() ? 0755 | S_IFDIR : 0755 | S_IFREG;
    file_stat.st_nlink = 0;
    file_stat.st_uid = 0;
    file_stat.st_gid = 0;
    file_stat.st_rdev = 0;
    file_stat.st_size = file->file_sz();
    file_stat.st_blksize = 0; // ignored currently
    file_stat.st_blocks = 0; // ignored currently
    file_stat.st_atim = fat32::dos2UnixTs(file->accTime());
    file_stat.st_mtim = fat32::dos2UnixTs(file->wrtTime());
    file_stat.st_ctim = fat32::dos2UnixTs_2(file->crtTime());

    return file_stat;
}

struct statvfs getStatfs() {
    struct statvfs fs_stat{};
    fs_stat.f_bsize = fat32::bytesPerClus(filesystem->bpb());
    fs_stat.f_frsize = fat32::bytesPerClus(filesystem->bpb());
    fs_stat.f_blocks = filesystem->fat().totalClusCnt();
    fs_stat.f_bfree = filesystem->fat().availClusCnt();
    fs_stat.f_bavail = filesystem->fat().availClusCnt();
    fs_stat.f_files = 0;
    fs_stat.f_ffree = 0;
    fs_stat.f_favail = 0;
    fs_stat.f_fsid = 2080;
    fs_stat.f_flag = 0;
    fs_stat.f_namemax = 64;

    return fs_stat;
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
        e.attr = readFileStat(file);
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
    auto stat = readFileStat(file);
    fuse_reply_attr(req, &stat, 1);
}

// The dispatcher for chmod, chown, truncate and utimensat/futimens, some of them should be skipped.
static void fat32_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int valid, struct fuse_file_info *fi) {
    if (valid & (FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
        fuse_reply_err(req, EPERM); // chown cannot be implemented on fat32
        return;
    }
    // chmod are ignored for fat32.
    auto file = getExistFile(ino);
    if (valid & FUSE_SET_ATTR_SIZE) {
        if (!file->truncate(attr->st_size)) {
            fuse_reply_err(req, EPERM);
            return;
        }
    }
    if (valid & FUSE_SET_ATTR_CTIME) {
        fat32::FatTimeStamp2 ts2 = fat32::unix2DosTs_2(attr->st_ctim);
        file->setCrtTime(ts2);
    }
    if (valid & FUSE_SET_ATTR_ATIME) {
        fat32::FatTimeStamp ts{};
        if (valid & FUSE_SET_ATTR_ATIME_NOW) {
            ts = fat32::getCurDosTs();
        } else {
            ts = fat32::unix2DosTs(attr->st_atim);
        }
        file->setAccTime(ts);
    }
    if (valid & FUSE_SET_ATTR_MTIME) {
        fat32::FatTimeStamp ts{};
        if (valid & FUSE_SET_ATTR_MTIME_NOW) {
            ts = fat32::getCurDosTs();
        } else {
            ts = fat32::unix2DosTs(attr->st_mtim);
        }
        file->setWrtTime(ts);
    }

    auto new_stat = readFileStat(file);
    fuse_reply_attr(req, &new_stat, 0);
}

static void fat32_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev) {
    // Even though mknod can be used to create a regular file... we return EPERM in every situation.
    fuse_reply_err(req, EPERM);
}

// fat32 doesn't support mode
static void fat32_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t) {
    if (!filesystem->isValidName(name)) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    auto parent_dir = getExistDir(parent);
    auto result = parent_dir->crtDir(name);
    if (!result.has_value()) {
        fuse_reply_err(req, EFBIG);
        return;
    }
    auto child_dir = result.value();
    struct fuse_entry_param e{};
    e.attr = readFileStat(child_dir);
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
    assert(parent_dir->delFile(name));
    child->selfDestruct();
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

    auto sub_dir = std::dynamic_pointer_cast<fs::Directory>(child);
    if (!sub_dir->isEmpty()) {
        fuse_reply_err(req, ENOTEMPTY);
        return;
    }

    assert(parent_dir->delFile(name));
    sub_dir->selfDestruct();
    fuse_reply_err(req, 0);
}

static void fat32_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name) {
    fuse_reply_err(req, EPERM);
}

static void fat32_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
                         fuse_ino_t newparent, const char *newname, unsigned int flags) {
    // for simplicity, return invalid when `flags` is not empty
    if (flags) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    // lookup name and newname on parent and newparent dir separately
    auto old_parent = getExistDir(parent);
    auto new_parent = getExistDir(newparent);
    auto old_result = old_parent->lookupFile(name);
    if (!old_result.has_value()) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    auto old_file = old_result.value();
    auto new_result = new_parent->lookupFile(newname);

    if (new_result.has_value()) { // newname exists
        auto new_file = new_result.value();
        // libfuse checks the file type automatically, the asserting here makes sure that.
        assert((old_file->isDir() && new_file->isDir()) || (!old_file->isDir() && !new_file->isDir()));

        if (old_file->isDir()) {
            if (!std::dynamic_pointer_cast<fs::Directory>(new_file)->isEmpty()) {
                fuse_reply_err(req, ENOTEMPTY);
                return;
            }
        }

        new_file->exchangeMetaData(old_file);
        old_file->selfDestruct();
    } else { // newname doesn't exist
        if (!filesystem->isValidName(newname)) {
            fuse_reply_err(req, EINVAL);
            return;
        }

        std::optional<std::shared_ptr<fs::File>> result;
        if (old_file->isDir()) {
            result = new_parent->crtDir(newname);
        } else {
            result = new_parent->crtFile(newname);
        }
        if (!result.has_value()) {
            fuse_reply_err(req, EFBIG);
            return;
        }
        auto new_file = result.value();
        new_file->exchangeMetaData(old_file);
        old_file->selfDestruct();
    }

    fuse_reply_err(req, 0);
}

static void fat32_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname) {
    fuse_reply_err(req, EPERM);
}

static void fat32_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    // auto result = filesystem->openFile(ino);
    // assert(result.has_value());
    fuse_reply_open(req, fi);
}

static void fat32_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (size >= 4294967295) { // size should be less than u32::max
        fuse_reply_err(req, EINVAL);
        return;
    }
    auto file = getExistFile(ino);
    char *buf = new char[size];
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
    filesystem->closeFile(ino);
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
    // assert(filesystem->openFile(ino).has_value());
    fuse_reply_open(req, fi);
}

// todo: make it right
static void fat32_do_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t offset, struct fuse_file_info *fi, bool plus) {
    auto file = getExistFile(ino);
    if (!file->isDir()) { // do we really need this?
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    auto dir = std::dynamic_pointer_cast<fs::Directory>(file);
    char *buf = new char[size];
    char *p = buf;
    u32 rem = size;
    u64 offset_ = offset;
    while (true) {
        auto ret = dir->lookupFileByIndex(offset_);
        offset = offset_;
        if (!ret.has_value()) {
            break;
        }
        auto sub_file = ret.value();
        auto state = readFileStat(sub_file);

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
    fat32_do_readdir(req, ino, size, offset, fi, false);
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
    filesystem->closeFile(ino);
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

// ignore `ino`
static void fat32_statfs(fuse_req_t req, fuse_ino_t ino) {
    struct statvfs stat_vfs = getStatfs();
    fuse_reply_statfs(req, &stat_vfs);
}


// we will only create regular file.
static void fat32_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi) {
    if (!filesystem->isValidName(name)) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    if (isIllegalFat32FileType(mode)) {
        fuse_reply_err(req, EPERM);
        return;
    }

    auto parent_dir = getExistDir(parent);
    if (parent_dir->lookupFile(name).has_value()) {
        fuse_reply_err(req, EEXIST);
        return;
    }

    auto result = parent_dir->crtFile(name);
    if (!result.has_value()) {
        fuse_reply_err(req, EFBIG);
        return;
    }
    auto child_file = result.value();
    struct fuse_entry_param e{};
    e.attr = readFileStat(child_file);
    e.ino = child_file->ino();
    fuse_reply_create(req, &e, fi);
}

static const struct fuse_lowlevel_ops fat32_ll_oper = {
        // .init = fat32_init,
        // .destroy = fat32_destroy,
        .lookup = fat32_lookup,
        .getattr = fat32_getattr,
        .setattr = fat32_setattr,
        .mknod = fat32_mknod,
        .mkdir = fat32_mkdir,
        .unlink = fat32_unlink,
        .rmdir = fat32_rmdir,
        .symlink = fat32_symlink,
        .rename = fat32_rename,
        .link = fat32_link,
        .open = fat32_open,
        .read = fat32_read,
        .write = fat32_write,
        // .release = fat32_release,
        .fsync = fat32_fsync,
        .opendir = fat32_opendir,
        .readdir = fat32_readdir,
        // .releasedir = fat32_releasedir,
        .fsyncdir = fat32_fsyncdir,
        .statfs = fat32_statfs,
        .create = fat32_create,
        .readdirplus = fat32_readdir_plus,
};

int main(int argc, char *argv[]) {
    struct fuse_session *se;
    std::shared_ptr<device::LinuxFileDriver> real_device;
    std::shared_ptr<device::CacheManager> cache_mgr;
    int ret = -1;
    std::string mountpoint, device_path;
    cmdline::parser cmd_parser;
    bool is_foreground, is_debug;
    std::vector<const char *> arguments;
    int fake_argc = 1;
    char **fake_argv;
    struct fuse_args fake_args;

    cmd_parser.add("debug", 'd', "enable debug mode");
    cmd_parser.add("foreground", 'f', "foreground operation");
    cmd_parser.add<std::string>("device-path", 'p', "the path to the device", true);
    cmd_parser.add<std::string>("mountpoint", 'm', "the mountpoint", true);
    cmd_parser.parse_check(argc, argv);

    mountpoint = util::getFullPath(cmd_parser.get<std::string>("mountpoint"));
    device_path = util::getFullPath(cmd_parser.get<std::string>("device-path"));
    is_foreground = cmd_parser.exist("foreground");
    is_debug = cmd_parser.exist("debug");

    arguments.push_back(argv[0]);
    if (is_debug) {
        arguments.push_back("-d");
        fake_argc += 1;
    }
    fake_argv = (char **) &arguments[0];
    fake_args = {fake_argc, fake_argv, 0};

    // We only use debug option, other fuse options are ignored.
    se = fuse_session_new(&fake_args, &fat32_ll_oper,
                          sizeof(fuse_lowlevel_ops), NULL);
    if (se == NULL)
        goto err_out1;

    if (fuse_set_signal_handlers(se) != 0)
        goto err_out2;

    if (fuse_session_mount(se, mountpoint.c_str()) != 0)
        goto err_out3;

    real_device = std::make_shared<device::LinuxFileDriver>(device_path, SECTOR_SIZE);
    cache_mgr = std::make_shared<device::CacheManager>(std::move(real_device));
    filesystem = fs::FAT32fs::from(std::move(cache_mgr));
    fuse_daemonize(is_foreground);

    /* Block until ctrl+c or fusermount -u */
    ret = fuse_session_loop(se);

    filesystem->flush();
    fuse_session_unmount(se);
    err_out3:
    fuse_remove_signal_handlers(se);
    err_out2:
    fuse_session_destroy(se);
    err_out1:

    return ret ? 1 : 0;
}