/* tree.c — recursive file-tree operations for the file manager.
 *
 * Copy / move / delete of whole directory trees, split out of main.c so the
 * walk (the part with recursion, symlink handling and containment rules)
 * can be exercised on the host by test/tree_test.c without a compositor.
 * The UI never calls rename/unlink/rmdir directly: every path goes through
 * copy_tree / move_tree / remove_tree so files, folders and symlinks all
 * behave the same wherever the operation was started from (^C/^V, the
 * context menu, or a drag-and-drop).
 */
#define _GNU_SOURCE
#include "tree.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Chunked file copy. Returns 0 on success, -errno on failure. */
int copy_file(const char *from, const char *to)
{
    int sfd = open(from, O_RDONLY);
    if (sfd < 0) return -errno;
    int dfd = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dfd < 0) { int e = errno; close(sfd); return -e; }
    static char buf[16384];
    int rc = 0;
    for (;;) {
        ssize_t n = read(sfd, buf, sizeof(buf));
        if (n == 0) break;
        if (n < 0) { rc = -errno; break; }
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(dfd, buf + off, (size_t)(n - off));
            if (w <= 0) { rc = -errno; break; }
            off += w;
        }
        if (rc) break;
    }
    close(sfd);
    close(dfd);
    if (rc) unlink(to);
    return rc;
}

/* Is `child` the same path as `parent`, or inside it? Used to refuse a
 * recursive copy/move of a folder into its own subtree, which would other-
 * wise recurse until the depth cap with a half-written tree behind it. */
int path_within(const char *child, const char *parent)
{
    size_t plen = strlen(parent);
    while (plen > 1 && parent[plen - 1] == '/') plen--;
    if (strncmp(child, parent, plen) != 0) return 0;
    return child[plen] == '\0' || child[plen] == '/';
}

/* Recursion cap for the tree walkers below: deeper than any real tree here,
 * and it bounds the stack (each level holds two path buffers). */
#define TREE_MAX_DEPTH 32

/* Recursively copy `from` (file, dir or symlink) to `to`. 0 or -errno.
 * Symlinks are recreated, never followed — following one would copy
 * whatever it points at, escaping the tree being copied. */
int copy_tree(const char *from, const char *to, int depth)
{
    if (depth > TREE_MAX_DEPTH) return -ELOOP;

    struct stat st;
    if (lstat(from, &st) != 0) return -errno;

    if (S_ISLNK(st.st_mode)) {
        char target[800];
        ssize_t n = readlink(from, target, sizeof(target) - 1);
        if (n < 0) return -errno;
        target[n] = '\0';
        return symlink(target, to) != 0 ? -errno : 0;
    }
    if (!S_ISDIR(st.st_mode))
        return copy_file(from, to);

    if (mkdir(to, st.st_mode & 0777) != 0 && errno != EEXIST)
        return -errno;
    DIR *d = opendir(from);
    if (!d) return -errno;
    int rc = 0;
    struct dirent *de;
    while (rc == 0 && (de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        char sub_from[1024], sub_to[1024];
        if (snprintf(sub_from, sizeof(sub_from), "%s/%s", from, de->d_name)
                >= (int)sizeof(sub_from) ||
            snprintf(sub_to, sizeof(sub_to), "%s/%s", to, de->d_name)
                >= (int)sizeof(sub_to)) {
            rc = -ENAMETOOLONG;
            break;
        }
        rc = copy_tree(sub_from, sub_to, depth + 1);
    }
    closedir(d);
    return rc;
}

/* Recursively delete `path`. 0 or -errno. A symlink is unlinked, never
 * descended — descending one would delete files outside the tree. */
int remove_tree(const char *path, int depth)
{
    if (depth > TREE_MAX_DEPTH) return -ELOOP;

    struct stat st;
    if (lstat(path, &st) != 0) return -errno;
    if (!S_ISDIR(st.st_mode))
        return unlink(path) != 0 ? -errno : 0;

    DIR *d = opendir(path);
    if (!d) return -errno;
    int rc = 0;
    struct dirent *de;
    while (rc == 0 && (de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        char sub[1024];
        if (snprintf(sub, sizeof(sub), "%s/%s", path, de->d_name)
                >= (int)sizeof(sub)) {
            rc = -ENAMETOOLONG;
            break;
        }
        rc = remove_tree(sub, depth + 1);
    }
    closedir(d);
    if (rc != 0) return rc;
    return rmdir(path) != 0 ? -errno : 0;
}

/* Move `from` to `to`, falling back to copy+delete when rename can't cross
 * a filesystem boundary (/tmp ramfs ↔ ext2 root). 0 or -errno. */
int move_tree(const char *from, const char *to)
{
    if (rename(from, to) == 0) return 0;
    if (errno != EXDEV) return -errno;
    int rc = copy_tree(from, to, 0);
    if (rc != 0) { remove_tree(to, 0); return rc; }
    return remove_tree(from, 0);
}
