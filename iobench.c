#define _GNU_SOURCE
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

void do_io(int fd, int iotype, int random_type, off_t *offsets, size_t size, int blocksize, char *buf, int sleep, int interval)
{
    struct timeval tv, tv1, tv2;

    gettimeofday(&tv1, NULL);
    int i; 
    int sleep_counter = 0;
    for (i=0; i < size; ++i) {
        if (sleep > 0 && interval > 0) {
            if (sleep_counter == interval) {
                usleep(sleep);
                sleep_counter = 0;
            }
        }

        //long randblock = random() % count;
        //off_t offset = randblock * BLOCKSIZE;
        if (random_type) {
            off_t offset = offsets[i] * blocksize;
            long rl = lseek(fd, offset, SEEK_SET);
            if (rl < 0)// rv*BLOCKSIZE)
                perror("lseek");
        }

        ssize_t r = iocall(iotype, fd, buf, blocksize);
        //printf("%ld\n", r);
        if (r != blocksize) {
            printf("%s returned: %lu, expected: %d\n", iotype==TYPE_READ ? "read" : "write", r, blocksize);
        }
        sleep_counter++;
    }
    gettimeofday(&tv2, NULL);
    timersub(&tv2, &tv1, &tv);

    printf("%lu secs %lu usecs (%lf MB/s)\n", tv.tv_sec, tv.tv_usec, ((double)size*blocksize/((double) tv.tv_sec + (double)tv.tv_usec/1000000)/(1024*1024)));


}

int main(int argc, char **argv)
{
    int iotype;
    int random_type;
    int fd;
    size_t size;
    int blocksize;
    int sleep;
    int interval;
    off_t *offsets;
    char *buf;

    DocoptArgs args = docopt(argc, argv, 1, VERSION);

    if (args.read || args.randread) 
        iotype = TYPE_READ;
    else
        iotype = TYPE_WRITE;

    if (args.randread || args.randwrite)
        random_type = 1;

    size = atol(args.size);
    blocksize = atoi(args.blocksize);
    interval = atoi(args.interval);
    sleep = atoi(args.sleep);

    buf = (char *)malloc(size*blocksize);
    printf( "%lu %s operations (%d each) from %s.\n"
            "Sleeping %d every %d blocks. %s caches.\n", 
            size, argv[1], blocksize, args.file, sleep, interval, args.clear ? "Clear" : "Do not clear" );

    if (args.clear)
        system("sync; echo 3 > /proc/sys/vm/drop_caches");

    fd = open(args.file, O_RDWR);
    if (fd < 0) {
        perror("opening file");
        exit(1);
    }

    lseek(fd, 0, SEEK_SET);
    srandom(time(NULL));

    if (random_type)
        offsets = generate_offsets(0, size-1);

    do_io(fd, iotype, random_type, offsets, size, blocksize, buf, sleep, interval);

    if (random_type)
        free_offsets(offsets);
    free(buf);
    close(fd);
    return 0;

}
