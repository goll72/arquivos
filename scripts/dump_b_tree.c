#include <stdio.h>
#include <stdlib.h>
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

    fclose(f);
}
