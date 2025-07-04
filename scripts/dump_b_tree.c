#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#define CSI "\x1b["

enum color {
    COLOR_SKIP,
    COLOR_NONE,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_CYAN,
    COLOR_GRAY
};

#define COUNTOF(arr) (sizeof(arr) / sizeof(*(arr)))

#define F_LOOP_BACK_TO(x) (x + 1)
#define F_UNDEF_SIZE      (1 << 8)

struct field {
    uint16_t flags;
    uint8_t size, color;
    char *name;
};

static const char colors[][6] = {
    [COLOR_SKIP] = "",
    [COLOR_NONE] = CSI "39m",
    [COLOR_RED] = CSI "31m",
    [COLOR_GREEN] = CSI "32m",
    [COLOR_YELLOW] = CSI "33m",
    [COLOR_BLUE] = CSI "34m",
    [COLOR_CYAN] = CSI "36m",
    [COLOR_GRAY] = CSI "37m"
};

static const struct field header[] = {
    { .size =  1, COLOR_BLUE,   "s" },
    { .size =  4, COLOR_GREEN,  "root_rrn" },
    { .size =  4, COLOR_YELLOW, "next_rrn" },
    { .size =  4, COLOR_CYAN,   "n_pages" },
    
    { F_UNDEF_SIZE, .size = -1, COLOR_GRAY, "" }
};

static const struct field data[] = {
    { .size = 4, COLOR_GRAY, "type" },
    { .size = 4, COLOR_GREEN, "n_keys" },
    { .size = 4, COLOR_BLUE, "child" },
    
    [3] = { .size = 4, COLOR_YELLOW, "key" },

    { .size = 8, COLOR_RED, "offset" },
    
    { F_LOOP_BACK_TO(3), .size = 4, COLOR_BLUE, "child" }
};

unsigned int uint_arg(char *const *argv, char opt, char *optarg)
{
    char *end;
    unsigned int val = strtoul(optarg, &end, 0);

    if (*end != '\0') {
        fprintf(stderr, "%s: -%c: invalid argument\n", argv[0], opt);
        exit(EXIT_FAILURE);
    }

    return val;
}

void print_heading(const struct field *fields, size_t n, size_t max_size)
{
    size_t n_read = 0;
    
    for (ssize_t i = 0; i != n; i++) {
        if (n_read + fields[i].size > max_size)
            break;
        
        fputs(fields[i].name, stdout);

        if (fields[i].flags & F_UNDEF_SIZE)
            continue;

        for (int j = strlen(fields[i].name); j < 3 * fields[i].size; j++)
            fputc(' ', stdout);

        n_read += fields[i].size;

        int8_t loop_back = fields[i].flags & 0xff;

        if (loop_back)
            i = loop_back - 2;
    }

    fputc('\n', stdout);
}

bool print_data(FILE *f, const struct field *fields, size_t n, size_t max_size, bool use_color)
{
    bool eof = false;
    size_t n_read = 0;
        
    for (ssize_t i = 0; i != n; i++) {
        if (use_color)
            fputs(colors[fields[i].color], stdout);

        for (int j = 0; j < fields[i].size || fields[i].flags & F_UNDEF_SIZE; j++) {
            int val = fgetc(f);

            if (val == EOF) {
                eof = true;
                goto out;
            }

            fprintf(stdout, " %02x", val);

            n_read++;

            if (n_read == max_size)
                goto out;
        }

        uint8_t loop_back = fields[i].flags & 0xff;

        if (loop_back)
            i = loop_back - 2;
    }

out:
    fputc('\n', stdout);

    return !eof;
}

typedef bool check_t(FILE *, size_t size, size_t page_size, bool use_color);

// Assumes all keys are < 65536 (= 2^16)
bool check_key_duplicity(FILE *f, size_t size, size_t page_size, bool use_color)
{
    uint64_t present[1024] = {};
    uint32_t n_pages = size / page_size;

    for (int i = 1; i < n_pages; i++) {
        fseek(f, i * page_size, SEEK_SET);

        uint32_t n_keys;

        fseek(f, 4, SEEK_CUR);
        fread(&n_keys, sizeof n_keys, 1, f);

        for (int i = 0; i < n_keys; i++) {
            fseek(f, 4, SEEK_CUR);

            uint32_t key;
            fread(&key, sizeof key, 1, f);

            if (key >> 16) {
                fprintf(stderr, "%s!%s key %x at page rrn=%x is greater than ffff\n",
                        use_color ? colors[COLOR_YELLOW] : "", use_color ? colors[COLOR_NONE] : "", key, i);
            } else {
                if (present[key & 0x3ff] & (1u << (key >> 10))) {
                    fprintf(stderr, "%sx%s key %x duplicated in tree\n",
                            use_color ? colors[COLOR_RED] : "", use_color ? colors[COLOR_NONE] : "", key);
                    return false;
                }

                present[key & 0x3ff] |= 1u << (key >> 10);
            }

            fseek(f, 8, SEEK_CUR);
        }
    }

    return true;
}

bool check_child_references(FILE *f, size_t size, size_t page_size, bool use_color)
{
    uint32_t n_pages = (size - page_size) / page_size;
    uint8_t *refs = malloc(n_pages);

#define PAGE_REF_MASK  0x7f
#define PAGE_ZERO_SIZE 0x80

    memset(refs, 0, n_pages);

    for (int i = 0; i < n_pages; i++) {
        fseek(f, (i + 1) * page_size, SEEK_SET);

        uint32_t type;
        uint32_t n_keys;
        uint32_t child_rrn;

        fread(&type, sizeof type, 1, f);
        fread(&n_keys, sizeof n_keys, 1, f);

        // Pretend root is empty
        if (type == 0 || n_keys == 0)
            refs[i] |= PAGE_ZERO_SIZE;

        for (int i = 0; i < n_keys + 1; i++) {
            fread(&child_rrn, sizeof child_rrn, 1, f);

            uint32_t key;
            fread(&key, sizeof key, 1, f);

            if (child_rrn >= 0 && child_rrn < n_pages) {
                refs[child_rrn]++;
            } else if (child_rrn != -1) {
                fprintf(stderr, "%sx%s invalid child rrn=%x\n",
                        use_color ? colors[COLOR_RED] : "", use_color ? colors[COLOR_NONE] : "", child_rrn);
                return false;
            }
                

            if (i == n_keys)
                break;

            fseek(f, 8, SEEK_CUR);
        }
    }

    for (int i = 0; i < n_pages; i++) {
        if (((refs[i] & PAGE_ZERO_SIZE) && refs[i] != PAGE_ZERO_SIZE)) {
            fprintf(stderr, "%sx%s empty/root page rrn=%x has %d references\n",
                    use_color ? colors[COLOR_RED] : "", use_color ? colors[COLOR_NONE] : "", i, refs[i] ^ PAGE_ZERO_SIZE);

            free(refs);
            
            return false;
        }

        if (!refs[i]) {
            fprintf(stderr, "%sx%s non-empty page rrn=%x has 0 references\n",
                    use_color ? colors[COLOR_RED] : "", use_color ? colors[COLOR_NONE] : "", i);

            free(refs);
            
            return false;
        }
    }

#undef PAGE_REF_MASK
#undef PAGE_ZERO_SIZE

    free(refs);

    return true;
}

static bool check_ordering_impl(FILE *f, int rrn, size_t page_size, int64_t *largest, bool use_color, int level)
{
    if (level >= 20) {
        fprintf(stderr, "%s!%s stack likely blown, this is the %dth recursive call\n",
                use_color ? colors[COLOR_YELLOW] : "", use_color ? colors[COLOR_NONE] : "", level);
        return false;
    }

    fseek(f, (rrn + 1) * page_size, SEEK_SET);

    uint32_t key;
    uint32_t n_keys;

    fseek(f, 4, SEEK_CUR);
    fread(&n_keys, sizeof n_keys, 1, f);

    for (int i = 0; i < n_keys; i++) {
        uint32_t left_child;
        fread(&left_child, sizeof left_child, 1, f);

        long off = ftell(f);

        if (left_child != -1 && !check_ordering_impl(f, left_child, page_size, largest, use_color, level + 1))
            return false;

        fseek(f, off, SEEK_SET);

        fread(&key, sizeof key, 1, f);

        if (key <= *largest) {
            fprintf(stderr, "%sx%s ordering property violated at key %x in page rrn=%x\n",
                    use_color ? colors[COLOR_RED] : "", use_color ? colors[COLOR_NONE] : "", key, rrn);
            return false;
        }
        
        fseek(f, 8, SEEK_CUR);

        if (i == n_keys - 1 && left_child == -1)
            *largest = key;
    }

    uint32_t right_child;
    fread(&right_child, sizeof right_child, 1, f);

    if (right_child != -1 && !check_ordering_impl(f, right_child, page_size, largest, use_color, level + 1))
        return false;

    if (right_child != -1 && key >= *largest) {
        fprintf(stderr, "%sx%s ordering property violated at key %x in page rrn=%x\n",
                use_color ? colors[COLOR_RED] : "", use_color ? colors[COLOR_NONE] : "", key, rrn);
        return false;
    }

    return true;
}

bool check_ordering(FILE *f, size_t size, size_t page_size, bool use_color)
{
    fseek(f, 1, SEEK_SET);

    uint32_t root_rrn;
    fread(&root_rrn, sizeof root_rrn, 1, f);

    int64_t largest = INT64_MIN;
    
    bool status = check_ordering_impl(f, root_rrn, page_size, &largest, use_color, 0);

    if (!status)
        fprintf(stderr, "%sx%s tree does not satisfy ordering property\n",
                use_color ? colors[COLOR_RED] : "", use_color ? colors[COLOR_NONE] : "");

    return status;
}

check_t *checks[] = {
    check_key_duplicity,
    check_child_references,
    check_ordering
};

int main(int argc, char **argv)
{
    int opt;

    int rrn_digits = 0;
    int page_size = 44;

    bool use_color = isatty(STDOUT_FILENO);

    while ((opt = getopt(argc, argv, "r:p:Ll")) != -1) {
        switch (opt) {
            case 'r':
                rrn_digits = uint_arg(argv, opt, optarg);
                break;
            case 'p':
                page_size = uint_arg(argv, opt, optarg);
                break;
            case 'L':
                use_color = true;
                break;
            case 'l':
                use_color = false;
                break;
            case '?':
                fprintf(stderr, "Usage: %s [-r N] [-p N] [-L | -l] -- <FILE>\n"
                                "    Dump the B-tree contained in FILE\n", argv[0]);

                return opt != 'h';
        }
    }

    const char *path = argv[optind];

    if (!path) {
        fprintf(stderr, "%s: missing argument FILE\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *f = fopen(path, "rb");

    if (!f) {
        fprintf(stderr, "%s: couldn't open file for reading: %s\n", argv[0], strerror(errno));
        return EXIT_FAILURE;
    }

    fseek(f, 0L, SEEK_END);
    long size = ftell(f);

    if (size < page_size) {
        fprintf(stderr, "%s: invalid file\n", argv[0]);
        return 1;
    }

    fseek(f, 0L, SEEK_SET);

    if (rrn_digits == 0 && size > page_size) {
        int tmp = (size - page_size) / page_size;

        do {
            tmp >>= 4;
            rrn_digits++;
        } while (tmp);
    }

    for (int i = 0; i < rrn_digits + 2; i++)
        fputc(' ', stdout);

    print_heading(header, COUNTOF(header), page_size);

    for (int i = 0; i < rrn_digits + 1; i++)
        fputc(' ', stdout);

    print_data(f, header, COUNTOF(header), page_size, use_color);
    fputc('\n', stdout);

    for (int i = 0; i < rrn_digits + 2; i++)
        fputc(' ', stdout);

    print_heading(data, COUNTOF(data), page_size);

    int cur_rrn = 0;

    do {
        if (use_color)
            fputs(colors[COLOR_NONE], stdout);

        fprintf(stdout, "%.*x ", rrn_digits, cur_rrn++);
    } while (print_data(f, data, COUNTOF(data), page_size, use_color));

    fputc('\n', stdout);

    bool status = true;

    for (int i = 0; i < COUNTOF(checks); i++) {
        fseek(f, 0L, SEEK_SET);
        
        if (!checks[i](f, size, page_size, use_color))
            status = false;
    }

    fclose(f);

    return !status;
}
