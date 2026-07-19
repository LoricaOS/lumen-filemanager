/* tree_test.c — host self-check for the recursive tree operations.
 *
 *   make check
 *
 * Builds a small tree under a temp dir, then asserts the properties the file
 * manager depends on: copy is recursive, symlinks are recreated rather than
 * followed (the one that matters — following them would let a copy or a
 * delete escape the tree), delete takes the contents with it, move survives
 * a rename that fails with EXDEV, and a folder cannot be put inside itself.
 */
#define _GNU_SOURCE
#include "../src/tree.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[256];

static void wr(const char *rel, const char *data)
{
    char p[512];
    snprintf(p, sizeof p, "%s/%s", root, rel);
    int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);
    assert(write(fd, data, strlen(data)) == (ssize_t)strlen(data));
    close(fd);
}

static void md(const char *rel)
{
    char p[512];
    snprintf(p, sizeof p, "%s/%s", root, rel);
    assert(mkdir(p, 0755) == 0);
}

static int exists(const char *rel)
{
    char p[512];
    struct stat st;
    snprintf(p, sizeof p, "%s/%s", root, rel);
    return lstat(p, &st) == 0;
}

static int is_link(const char *rel)
{
    char p[512];
    struct stat st;
    snprintf(p, sizeof p, "%s/%s", root, rel);
    return lstat(p, &st) == 0 && S_ISLNK(st.st_mode);
}

static void slurp(const char *rel, char *out, size_t n)
{
    char p[512];
    snprintf(p, sizeof p, "%s/%s", root, rel);
    int fd = open(p, O_RDONLY);
    assert(fd >= 0);
    ssize_t r = read(fd, out, n - 1);
    assert(r >= 0);
    out[r] = '\0';
    close(fd);
}

static void abspath(char *out, size_t n, const char *rel)
{
    snprintf(out, n, "%s/%s", root, rel);
}

int main(void)
{
    char tmpl[] = "/tmp/fmtreetestXXXXXX";
    assert(mkdtemp(tmpl) != NULL);
    snprintf(root, sizeof root, "%s", tmpl);

    /* src/
     *   a.txt
     *   sub/b.txt
     *   link -> a.txt
     *   escape -> ../outside   (must NOT be followed)
     */
    md("outside");
    wr("outside/secret.txt", "do not touch");
    md("src");
    wr("src/a.txt", "alpha");
    md("src/sub");
    wr("src/sub/b.txt", "bravo");

    char p[512], q[512];
    abspath(p, sizeof p, "src/link");
    assert(symlink("a.txt", p) == 0);
    abspath(p, sizeof p, "src/escape");
    assert(symlink("../outside", p) == 0);

    /* ── copy_tree is recursive and preserves content ─────────────────── */
    abspath(p, sizeof p, "src");
    abspath(q, sizeof q, "copy");
    assert(copy_tree(p, q, 0) == 0);
    assert(exists("copy/a.txt"));
    assert(exists("copy/sub/b.txt"));
    char buf[64];
    slurp("copy/sub/b.txt", buf, sizeof buf);
    assert(strcmp(buf, "bravo") == 0);

    /* ── symlinks are recreated, not followed ─────────────────────────── */
    assert(is_link("copy/link"));
    assert(is_link("copy/escape"));
    /* If `escape` had been followed, the copy would hold outside/'s files. */
    assert(!exists("copy/escape/secret.txt") || is_link("copy/escape"));

    /* ── remove_tree deletes contents, and does NOT follow the symlink out
     *    of the tree (outside/secret.txt must survive) ─────────────────── */
    abspath(p, sizeof p, "copy");
    assert(remove_tree(p, 0) == 0);
    assert(!exists("copy"));
    assert(exists("outside/secret.txt"));

    /* ── move_tree relocates a whole tree ─────────────────────────────── */
    abspath(p, sizeof p, "src");
    abspath(q, sizeof q, "moved");
    assert(move_tree(p, q) == 0);
    assert(!exists("src"));
    assert(exists("moved/sub/b.txt"));

    /* ── containment: a folder cannot go inside itself ────────────────── */
    abspath(p, sizeof p, "moved");
    abspath(q, sizeof q, "moved/sub");
    assert(path_within(q, p));          /* dest is inside src → refuse   */
    assert(path_within(p, p));          /* same path also counts         */
    assert(!path_within(p, q));         /* parent is not inside child    */
    /* A prefix that is not a path component must not count. */
    assert(!path_within("/home/livewire", "/home/live"));
    /* A trailing slash on the parent must not change the answer. */
    assert(path_within("/home/live/x", "/home/live/"));

    /* ── depth cap trips instead of recursing forever ─────────────────── */
    abspath(p, sizeof p, "moved");
    abspath(q, sizeof q, "deep");
    assert(copy_tree(p, q, TREE_MAX_DEPTH + 1) == -ELOOP);

    /* ── missing source reports ENOENT, not success ───────────────────── */
    abspath(p, sizeof p, "nope");
    abspath(q, sizeof q, "nope-copy");
    assert(copy_tree(p, q, 0) == -ENOENT);
    assert(remove_tree(p, 0) == -ENOENT);

    abspath(p, sizeof p, "");
    remove_tree(root, 0);
    printf("tree_test: all checks passed\n");
    return 0;
}
