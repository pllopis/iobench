#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "docopt.c"

#define VERSION "0.1"

#define TYPE_READ   0
#define TYPE_WRITE  1

size_t iocall(int iotype, int fd, char *buf, size_t size)
{
    if (iotype == TYPE_READ)
        return read(fd, buf, size);
    else
        return write(fd, buf, size);
}

off_t *generate_offsets(off_t min, off_t max)
{
    int count = max - min + 1;
    off_t *offsets = (off_t*) malloc(sizeof(off_t) * count);
    if (offsets == NULL)
        return NULL;

    int i;
    for(i=0; i<count; i++) {
        offsets[i] = i + min;
    }
    for(i=0; i<count; i++) {
        int s = random() % count;
        int d = random() % count;
        off_t swap = offsets[s];
        offsets[s] = offsets[d];
        offsets[d] = swap;
    }

    return offsets;
}

inline void free_offsets(off_t *offsets)
{
    free(offsets);
}

size_t parse_size(char *size_str)
{
    size_t size;
    int len = strlen(size_str);
    int factor = 1;
    
    switch(size_str[len-1]) {
    case 'k':
    case 'K':
        factor=1024;
        break;
    case 'm':
    case 'M':
        factor=1024*1024;
        break;
    case 'g':
    case 'G':
        factor=1024*1024*1024;
        break;
    default:
        factor=1;
    }

    if (factor != 1)
        size_str[len-1] = '\0';

    return atoll(size_str) * factor;
}

void do_io(int fd, int iotype, int random_type, off_t *offsets, size_t blocks, int blocksize, char *buf, int sleep, int interval)
{
    struct timeval tv, tv1, tv2;

    gettimeofday(&tv1, NULL);
    size_t i; 
    int sleep_counter = 0;
    for (i=0; i < blocks; ++i) {
        if (sleep > 0 && interval > 0) {
            if (sleep_counter == interval) {
                usleep(sleep);
                sleep_counter = 0;
            }
        }

        if (random_type) {
            off_t offset = offsets[i] * blocksize;
            long rl = lseek(fd, offset, SEEK_SET);
            if (rl < 0)
                perror("lseek");
        }

        ssize_t r = iocall(iotype, fd, buf, blocksize);
        if (r != blocksize) {
            printf("%s returned: %lu, expected: %d\n", iotype==TYPE_READ ? "read" : "write", r, blocksize);
        }
        sleep_counter++;
    }

    if (iotype == TYPE_WRITE) {
        fsync(fd);
    }

    gettimeofday(&tv2, NULL);
    timersub(&tv2, &tv1, &tv);

    printf("%lu secs %lu usecs (%lf MB/s)\n", tv.tv_sec, tv.tv_usec, ((double)blocks*blocksize/((double) tv.tv_sec + (double)tv.tv_usec/1000000)/(1024*1024)));
}

int main(int argc, char **argv)
{
    int i;
    int iotype;
    int random_type;
    int fd;
    size_t blocks;
    int blocksize;
    int sleep;
    int interval;
    int repeat;
    off_t *offsets;
    char *buf;

    DocoptArgs args = docopt(argc, argv, 1, VERSION);
    if (args.verbose) {
        printf("Commands\n");
        printf("read: %d\n", args.read);
        printf("write: %d\n", args.write);
        printf("randread: %d\n", args.randread);
        printf("randwrite: %d\n", args.randwrite);
        printf("Arguments\n");
        printf("file: %s\n", args.file);
        printf("size: %s\n", args.size);
        printf("blocks: %s\n", args.blocks);
        printf("blocksize: %s\n", args.blocksize);
        printf("interval: %s\n", args.interval);
        printf("clear: %d\n", args.clear);
        printf("verbose: %d\n", args.verbose);
        printf("repeat: %s\n", args.repeat);
        printf("sleep: %s\n", args.sleep);
    }

    if (args.read || args.randread) 
        iotype = TYPE_READ;
    else
        iotype = TYPE_WRITE;

    if (args.randread || args.randwrite)
        random_type = 1;
    else
        random_type = 0;

    blocksize = parse_size(args.blocksize);
    interval = atoi(args.interval);
    sleep = atoi(args.sleep);
    repeat = atoi(args.repeat);

    if (args.size) {
        blocks = (int)ceil((double)parse_size(args.size) / blocksize); 
    } else {
        blocks = parse_size(args.blocks);
    }

    buf = (char *)malloc(blocks*blocksize);

    printf( "%lu %s operations (%d each) from %s.\n"
            "Sleeping %d every %d blocks. %s caches. %s I/O. Repeat: %d\n", 
            blocks, argv[1], blocksize, args.file, sleep, interval, 
            args.clear ? "Clear" : "Do not clear", random_type ? "Random" : "Sequential",
            repeat);

    fd = open(args.file, O_RDWR);
    if (fd < 0) {
        perror("opening file");
        exit(1);
    }

    lseek(fd, 0, SEEK_SET);

    for (i=0; i<repeat; i++) {
        if (random_type) {
            srandom(time(NULL));
            offsets = generate_offsets(0, blocks-1);
        }

        if (args.clear) {
            fdatasync(fd);
            posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
        }

        do_io(fd, iotype, random_type, offsets, blocks, blocksize, buf, sleep, interval);

        if (random_type)
            free_offsets(offsets);
    }

    free(buf);
    close(fd);
    return 0;
}
