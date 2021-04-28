#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE (1024 * 1024 * 16)
#define FILE_PATH_LEN 1024

unsigned int hash(const char *str);
int hashIdx(const char* path, int fnCnt);

char** dirs;
int dirCnt;
int blockSize;

typedef struct qfile {
    const char* path;
    const char* mode;
    FILE** fhs;
    int fhCnt;
    fpos_t offset;
    int startIdx;
} qfile;

int qinit(char** dirs_, int dirCnt_, int blockSize_);

int quninit();

qfile * qfopen(const char* path, const char* mode);

size_t qfwrite (void* ptr, size_t size, size_t count, qfile *qfh);

size_t qfread (void* ptr, size_t size, size_t count, qfile *qfh);

int qfseek(qfile *qfh, long offset, int origin);

int qftell(qfile* qfh);

int qfclose(qfile *qfh);

int qremove(const char *path); 

//---------------------------------------------------------move to c-------------------------------

unsigned int hash(const char *str)
{
    unsigned int hash = 5381;
    int c = 0;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int hashIdx(const char* path, int fnCnt) {
    return hash(path) % fnCnt;
}

int qinit(char** dirs_, int dirCnt_, int blockSize_) {
    //todo check with old param

    dirs = (char **) malloc(dirCnt_*sizeof(const char*));
    for (int i = 0; i < dirCnt_; i+=1) {
        dirs[i] = (char*)malloc(strlen(dirs_[i])+1);
        strcpy(dirs[i], dirs_[i]);
    }
    dirCnt = dirCnt_;
    if (blockSize_ != 0) {
        blockSize = blockSize_;
    } else {
        blockSize = BLOCK_SIZE;
    }
    return 0;
}

int quninit() {
    for (int i = 0; i < dirCnt; i+=1) {
        free(dirs[i]);
    }
    free(dirs);
    dirs = NULL;
    return 0;
}

qfile* qfopen (const char* path, const char* mode) {
    qfile *qfh = (qfile*)malloc(sizeof(qfile));
    memset(qfh, 0, sizeof(qfile));
    qfh->path = path;
    qfh->mode = mode;
    qfh->fhCnt = dirCnt;
    qfh->startIdx = hashIdx(qfh->path, qfh->fhCnt);

    qfh->fhs = (FILE**)malloc(qfh->fhCnt * sizeof(FILE*));
    memset(qfh->fhs, 0, qfh->fhCnt * sizeof(FILE*));
    return qfh;
}

FILE* qfopenIdx (qfile* qfh, int idx) {
    char buf[FILE_PATH_LEN];
    memset(buf, 0, sizeof(buf));
    char* filePath = strcat(strcpy(buf, dirs[idx]), qfh->path);
    return fopen(filePath, qfh->mode);
}

size_t qfwrite (void* ptr, size_t size, size_t count, qfile *qfh) {
    size_t totalWriteCount = 0;

    while (count > 0) {
        size_t writeSize = blockSize - qfh->offset % blockSize;
        if (writeSize >= size * count) {
            writeSize = size * count;
        } else if (writeSize % size != 0) { //size not aligned
            return 0;
        }

        size_t writeCount = writeSize / size;
        int idx = (qfh->startIdx + qfh->offset / blockSize) % qfh->fhCnt;
        FILE* fh = qfh->fhs[idx];
        if (NULL == fh) {
            if (qfh->offset / blockSize >= qfh->fhCnt) {
                return totalWriteCount;
            }
            fh = qfopenIdx(qfh, idx);
            if (NULL == fh) {
                return totalWriteCount;
            }
            qfh->fhs[idx] = fh;
        }

        int retCount = fwrite(ptr, size, writeCount, fh);
        totalWriteCount += writeCount;
        if (retCount != writeCount) {
            return totalWriteCount;
        }

        count -= writeCount;
        ptr = (void*)((char*)ptr + writeSize);
        qfh->offset += writeSize;
    }
    return totalWriteCount;
}

size_t qfread (void* ptr, size_t size, size_t count, qfile *qfh) {
    size_t totalReadCount = 0;
    while (count > 0) {
        size_t readSize = blockSize - qfh->offset % blockSize;
        if (readSize >= size * count) {
            readSize = size * count;
        } else if (readSize % size != 0) {
            return 0;
        }

        size_t readCount = readSize / size;
        int idx = (qfh->startIdx + qfh->offset / blockSize) % qfh->fhCnt;
        FILE* fh = qfh->fhs[idx];
        if (NULL == fh) {
            if (qfh->offset / blockSize >= qfh->fhCnt) {
                return totalReadCount;
            }
            fh = qfopenIdx(qfh, idx);
            if (NULL == fh) {
                return totalReadCount;
            }
            qfh->fhs[idx] = fh;
        }

        int retCount = fread(ptr, size, readCount, fh);
        totalReadCount+=retCount;
        if (retCount != readCount) {
            printf("qfread retCount:%d readCount:%d offset:%d \n", retCount, readCount, fh->_offset);
            return totalReadCount;
        }

        count -= readCount;
        ptr = (void*)((char*)ptr + readSize);
        qfh->offset += readSize;
    }
    return totalReadCount;
}

int qfseek(qfile *qfh, long offset, int whence) {
    if (whence >= SEEK_END) { // need not support
        return -1;
    }
    if (SEEK_CUR == whence) {
        if (qfh->offset + offset < 0) {
            return -1;
        }
        whence = SEEK_SET;
        offset = qfh->offset + offset;
    }
    if (offset < 0) {
        return -1;
    }

    int skipBlocks = offset / blockSize;
    int loops = skipBlocks / qfh->fhCnt;
    int blocksRemaining = skipBlocks % qfh->fhCnt;
    int offsetRemaining = offset % blockSize;
    
    for (int i = 0; i < qfh->fhCnt; i+=1) {
        int skipSize = loops * blockSize;
        if (blocksRemaining > 0) {
            skipSize += blockSize;
            blocksRemaining -=1;
        } else if (offsetRemaining > 0) {
            skipSize += offsetRemaining;
            offsetRemaining = 0;
        }
        int idx = (qfh->startIdx + i) % qfh->fhCnt;
        FILE* fh = qfh->fhs[idx];
        if (fh == NULL) {
            fh = qfopenIdx(qfh, idx);
            if (fh == NULL) {
                return -1;
            }
            qfh->fhs[idx] = fh;
        }
        int ret = fseek(fh, skipSize, whence);
        if (ret != 0) {
            return ret;
        }
    }
    qfh->offset = offset;
    return 0;
}

int qftell(qfile* qfh) {
    int size = 0;
    int startIdx = qfh->startIdx;
    for (int i = 0; i < qfh->fhCnt; i+=1) {
        int idx = (startIdx+i)%qfh->fhCnt;
        if (qfh->fhs[idx] != NULL) {
            size += ftell(qfh->fhs[idx]);
        }
    }
    return size;
}

int qfclose(qfile *qfh) {
    int startIdx = qfh->startIdx;
    int ret = 0;
    for (int i = 0; i < qfh->fhCnt; i+=1) {
        int idx = (startIdx+i)%qfh->fhCnt;
        if (qfh->fhs[idx] != NULL) {
            int ret2 = fclose(qfh->fhs[idx]);
            if (i == 0) {
                ret = ret;
            }
            qfh->fhs[idx] = NULL;
        }
    }
    free(qfh->fhs);
    qfh->fhs = NULL;
    free(qfh);
    return ret;
}

int qremove(const char *path) {    
    for (int i = 0; i < dirCnt; i+=1) {
        char buf[FILE_PATH_LEN];
        memset(buf, 0, sizeof(buf));
        char* filePath = strcat(strcpy(buf, dirs[i]), path);
        remove(filePath);
    }
    return 0;
}

