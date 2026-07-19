/* tree.h — recursive file-tree operations (see tree.c).
 *
 * All return 0 on success or -errno. `depth` is the recursion counter: pass
 * 0 from a caller. Symlinks are never followed — copy recreates them, delete
 * unlinks them — so an operation can never escape the tree it was given.
 */
#ifndef FM_TREE_H
#define FM_TREE_H

/* Recursion cap: deeper than any real tree here, and it bounds the stack
 * (each level holds up to two path buffers). Exceeding it returns -ELOOP. */
#define TREE_MAX_DEPTH 32

int copy_file(const char *from, const char *to);
int copy_tree(const char *from, const char *to, int depth);
int remove_tree(const char *path, int depth);
int move_tree(const char *from, const char *to);

/* Non-zero if `child` is `parent` or lives inside it. Callers use this to
 * refuse copying/moving a folder into its own subtree. */
int path_within(const char *child, const char *parent);

#endif /* FM_TREE_H */
