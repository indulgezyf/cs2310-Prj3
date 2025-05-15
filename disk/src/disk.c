#include "disk.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "log.h"
#define BLOCKSIZE 512

// global variables
int _ncyl, _nsec, ttd, _fd;
char *_data;

int init_disk(char *filename, int ncyl, int nsec, int ttd) {
    _ncyl = ncyl;
    _nsec = nsec;
    ttd = ttd;
    // do some initialization...
    long FILESIZE = (long)ncyl * nsec * BLOCKSIZE;    
    // open file
    int fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        Log("Failed to open file: %s", filename);
        return 1;
    }
    if(lseek(fd, 0, SEEK_END) == 0){
        // stretch the file
        int result = lseek(fd, FILESIZE - 1, SEEK_SET);
        if (result < 0) {
            Log("Failed to lseek file: %s", filename);
            close(fd);
            return 1;
        }
        // write a null byte to the end of the file
        result = write(fd, "", 1);
        if (result < 0) {
            Log("Failed to write file: %s", filename);
            close(fd);
            return 1;
        }
    }
    _fd = fd;
    // mmap
    char *data = (char*)mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        Log("Failed to mmap file: %s", filename);
        close(fd);
        return 1;
    }
    _data = data;
    
    Log("Disk initialized: %s, %d Cylinders, %d Sectors per cylinder", filename, ncyl, nsec);
    return 0;
}

// all cmd functions return 0 on success
int cmd_i(int *ncyl, int *nsec) {
    // get the disk info
    *ncyl = _ncyl;
    *nsec = _nsec;
    return 0;
}

int cmd_r(int cyl, int sec, char *buf) {
    // read data from disk, store it in buf
    if (cyl >= _ncyl || sec >= _nsec || cyl < 0 || sec < 0) {
        Log("Invalid cylinder or sector");
        return 1;
    }
    long offset = (long)cyl * _nsec * BLOCKSIZE + (long)sec * BLOCKSIZE;
    memcpy(buf, _data+ offset, BLOCKSIZE);
    return 0;
}

int cmd_w(int cyl, int sec, int len, char *data) {
    // write data to disk
    printf("cyl: %d, sec: %d, len: %d\n", cyl, sec, len);
    if (cyl >= _ncyl || sec >= _nsec || cyl < 0 || sec < 0) {
        Error("Invalid cylinder or sector");
        return 1;
    }
    if (len > BLOCKSIZE) {
        Error("Data length exceeds block size");
        return 1;
    }
    long offset = (long)cyl * _nsec * BLOCKSIZE + (long)sec * BLOCKSIZE;
    char zeropad[BLOCKSIZE] = {0};
    memcpy(_data+offset, data, len);
    // pad the rest of the block with zeros
    memcpy(_data + offset + len, zeropad, BLOCKSIZE - len);
    return 0;
}

void close_disk() {
    // close the file
    if (munmap(_data, (long)_ncyl * _nsec * BLOCKSIZE) == -1) {
        Log("Failed to munmap");
    }
    if(close(_fd) == -1) {
        Log("Failed to close file");
    }
    Log("Disk closed");
    // free the global variables
    _data = NULL;
    _fd = -1;
    _ncyl = 0;
    _nsec = 0;
    ttd = 0;
}
