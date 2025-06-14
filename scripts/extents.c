#include <err.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fs.h>
#include <linux/fiemap.h>

static const char *flag_map[32] = {
    [__builtin_clz(FIEMAP_EXTENT_LAST)] = "last",
    [__builtin_clz(FIEMAP_EXTENT_UNKNOWN)] = "unknown",
    [__builtin_clz(FIEMAP_EXTENT_DELALLOC)] = "delayed_alloc",
    [__builtin_clz(FIEMAP_EXTENT_ENCODED)] = "encoded",
    [__builtin_clz(FIEMAP_EXTENT_DATA_ENCRYPTED)] = "encrypted",
    [__builtin_clz(FIEMAP_EXTENT_NOT_ALIGNED)] = "not_aligned",
    [__builtin_clz(FIEMAP_EXTENT_DATA_INLINE)] = "inline",
    [__builtin_clz(FIEMAP_EXTENT_DATA_TAIL)] = "tail",
    [__builtin_clz(FIEMAP_EXTENT_UNWRITTEN)] = "unwritten",
    [__builtin_clz(FIEMAP_EXTENT_MERGED)] = "merged",
    [__builtin_clz(FIEMAP_EXTENT_SHARED)] = "shared"
};

int main(int argc, char **argv)
{
    if (argc != 2)
        errx(1, "error: argument FILE not provided\n"
                "  Print extents mappings for FILE");

    const char *path = argv[1];

    int fd = open(path, O_RDONLY);

    if (fd == -1)
        err(1, "Couldn't open `%s'", path);

    static struct fiemap f = {
        .fm_length = UINT64_MAX
    };
    
    if (ioctl(fd, FS_IOC_FIEMAP, &f) < 0)
        err(1, "Couldn't read file extent mappings");

    size_t exts_size = sizeof f.fm_extents[0] * f.fm_mapped_extents;
    struct fiemap *fs = malloc(sizeof *fs + exts_size);

    memset(fs->fm_extents, 0, exts_size);

    fs->fm_length = f.fm_length;

    fs->fm_extent_count = f.fm_mapped_extents;
    fs->fm_mapped_extents = 0;

    if (ioctl(fd, FS_IOC_FIEMAP, fs) < 0)
        err(1, "Couldn't read file extent mappings");


    for (int i = 0; i < fs->fm_mapped_extents; i++) {
        struct fiemap_extent *ext = &fs->fm_extents[i];

        printf("ext % 2d:   offset 0x%08llx(0x%08llx)   length % 12lld",
                   i, ext->fe_logical, ext->fe_physical, ext->fe_length);

        uint32_t flags = ext->fe_flags;

        if (flags != 0)
            printf("      ");

        while (flags != 0) {
             uint32_t clz = __builtin_clz(flags);
             uint32_t flag = 1U << (31 - clz);

             if (!flag_map[clz])
                 errx(1, "Unrecognized flag: %x", flag);

             printf(" %s", flag_map[clz]);
             flags &= ~flag;
        }

        putc('\n', stdout);
    }

    free(fs);
}
