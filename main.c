#include <stdio.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "qfile.h"

#define DIRCNT_MIN 1
#define DIRCNT_MAX 4
#define TEST_CNT 100
#define DIR_PREFIX "dir"
#define BLOCK_SIZE 1024
#define FSIZE_MIN 100
#define FSIZE_MAX BLOCK_SIZE*DIRCNT_MAX*100

int randInt(int min, int max) {
    if (min == max) {
        return 0;
    }
    return min + rand() % (max-min);
}

int test();

int main() {
    srand((unsigned)time(NULL));

    printf("%c \n", 0 +'0');

    // char buf[10];
    // memset(buf, 0, sizeof(buf));
    // char* p1 = strcat(strcpy(buf, "ldc/"), "yq");
    // printf("p1:%s \n", p1);

    FILE* fh = fopen("a", "w+");
    fseek(fh, 100, SEEK_SET);
    fseek(fh, -10, SEEK_CUR);
    char* buf = "ldcso";
    fwrite(buf, 1, strlen(buf), fh);
    fseek(fh, -strlen(buf)-1, SEEK_CUR);
    int ret = ftell(fh);
    printf("ftell %d \n", ret);
    char rbuf[10];
    memset(rbuf, 0, 10);
    ret = fread(rbuf, 1, 10, fh);
    fclose(fh);
    printf("qfread 1. ret:%d \n", ret);
    printf("qfread 2. rpbuf:%c \n", rbuf[1]);

    fh = fopen("a", "r");
    fseek(fh, 0, SEEK_END);
    ret = ftell(fh);
    fclose(fh);
    printf("fsize ret:%d \n", ret);
    // ret = fread(rbuf, 1, 10, fh);
    // printf("qfread 1. ret:%d \n", ret);
    // printf("qfread 2. rpbuf:%c \n", rbuf[1]);

    for (int i = 0; i < TEST_CNT; i += 1) {
        int ret = test();
        if (ret != 0) {
            printf("test failed. ret:%d \n", ret);
        }
    }

    printf("test success \n");
    return 0;
}

char** initDirs(int dirCnt, int blockSize) {
    char** dirs = (char**)malloc(dirCnt*sizeof(char*));
    for (int i = 0; i < dirCnt; i += 1) {
        dirs[i] = (char*)malloc(strlen(DIR_PREFIX)+1+1+1);
        strcpy(dirs[i], DIR_PREFIX);
        dirs[i][strlen(DIR_PREFIX)] = i+1+'0';
        dirs[i][strlen(DIR_PREFIX)+1] = '/';
        dirs[i][strlen(DIR_PREFIX)+2] = '\0';

        int ret = mkdir(dirs[i], S_IRWXU);
        if (ret != 0) {
            printf("mkdir failed dir:%s ret:%d \n", dirs[i], ret);
            return NULL;
        }
    }

    printf("after mkdir \n");

    int ret = qinit(dirs, dirCnt, blockSize);
    if (ret != 0) {
        printf("qinit failed \n");
        return NULL;
    }

    printf("after qinit \n");

    return dirs;
}

int uninitDirs(char** dirs, int dirCnt) {
    for (int i = 0; i < dirCnt; i+=1){
        int ret = rmdir(dirs[i]);
        if (ret != 0) {
            return ret;
        }
    }
    int ret = quninit();
    if (ret != 0) {
        printf("quninit failed \n");
        return ret;
    }
    return 0;
}

int checkqfh(qfile* qfh) {
    if (qfh == NULL) {
        printf("qfh is null \n");
        getchar();
        exit(0);
    }
    return 0;
}

int checkret(int ret) {
    if (ret != 0) {
        printf("ret is not 0 \n");
        getchar();
        exit(0);
    }
    return 0;
}

int checkequal(int param1, int param2) {
    if (param1 != param2) {
        printf("not equal param1:%d param2:%d \n", param1, param2);
        getchar();
        exit(0);
    }
    return 0;
}

// 小于一个块的文件操作
int test0(char** dirs, int dirCnt, int blockSize) {
    const char* filename = "test0";
    printf("test0 start \n");

    int ret = 0;
    qfile* qfh = qfopen(filename, "r");
    checkqfh(qfh);

    ret = qfclose(qfh);  
    checkret(ret);

    printf("qfclose 1 \n");

    qfh = qfopen(filename, "w+");
    checkqfh(qfh);

    ret = qfseek(qfh, 100, SEEK_SET);
    checkret(ret);

    printf("qfseek 1 \n");

    ret = qftell(qfh);
    checkequal(ret, 100);

    ret = qfseek(qfh, -10, SEEK_CUR);
    checkret(ret);

    printf("qfseek 2. offset:%d \n", qfh->offset);

    char* buf = "ldcso";
    ret = qfwrite(buf, 1, strlen(buf), qfh);
    checkequal(ret, strlen(buf));

    printf("qfwrite 1. offset:%d \n", qfh->offset);
    ret = qfseek(qfh, -strlen(buf)-1, SEEK_CUR);
    checkret(ret);

    printf("qfseek 3. offset:%d \n", qfh->offset);

    char rbuf[10];
    memset(rbuf, 0, 10);
    ret = qfread(rbuf, 1, 10, qfh);
    printf("qfread 1. ret:%d \n", ret);
    checkret(ret!=(strlen(buf)+1));

    printf("qfread 2. rpbuf:%c \n", rbuf[1]);
    checkret(rbuf[1] != 'l');

    ret = qfclose(qfh);
    checkret(ret);

    printf("qfclose 2 \n");

    ret = qremove(filename);
    checkret(ret);

    printf("qremove 1 \n");

    printf("test0 end \n");
    return 0;
}

int testWriteSeekRead(char** dirs, int dirCnt, int blockSize) {
    int fsize = randInt(FSIZE_MIN, FSIZE_MAX);
    char* fileName = "testwrite";

    printf("test write start. fileName:%s fsize:%d startIdx:%d \n", fileName, fsize, hashIdx(fileName, dirCnt));

    qfile* qfh = qfopen(fileName, "w+");
    checkqfh(qfh);

    char* buf = (char*)malloc(fsize);
    for (int i = 0; i < fsize; i+=1) {
        buf[i] = i % 10 + '0';
    }

    printf("begin write. buf[0]:%c \n", buf[0]);

    int writeTotal = 0;
    int ret = 0;

    while (writeTotal < fsize) {
        int writeSize = randInt(1, blockSize * 6);
        if (writeSize > fsize-writeTotal) {
            writeSize = fsize-writeTotal;
        }
        ret = qfwrite(&buf[writeTotal], 1, writeSize, qfh);
        checkret(ret != writeSize);
        writeTotal += writeSize;
    }

    printf("begin seek. fsize:%d writeTotal:%d \n", qftell(qfh), writeTotal);
    checkret(fsize != qftell(qfh));

    int seekCnt = 100;
    for (int i = 0; i < seekCnt; i+=1) {
        int offset = randInt(0, fsize);
        ret = qfseek(qfh, offset, SEEK_SET);
        checkret(ret);
        ret = qftell(qfh);
        checkret(ret != offset);
        char bufr1[1];
        qfread(bufr1, 1, 1, qfh);
        char ch = offset % 10 + '0';
        checkret(bufr1[0] != ch);
    }

    ret = qfseek(qfh, 0, SEEK_SET);

    printf("begin read. offset:%d tell:%d \n", qfh->offset, qftell(qfh));

    int readTotal = 0;
    char* rbuf = (char*)malloc(fsize);
    memset(rbuf, 0, fsize);

    while (readTotal < fsize) {
        int readSize = randInt(1, blockSize * 6);
        if (readSize > fsize-readTotal) {
            readSize = fsize-readTotal;
        }
        ret = qfread(&rbuf[readTotal], 1, readSize, qfh);
        if (ret != readSize) {
            printf("readTotal:%d readSize:%d ret:%d \n", readTotal, readSize, ret);
        }
        checkret(ret != readSize);
        readTotal += readSize;
    }

    printf("after read. \n");

    for (int i = 0; i < fsize; i++){
        checkret(buf[i] != rbuf[i]);
    }

    qfclose(qfh);
    qremove(fileName);
    free(buf);

    printf("test write end \n");
    return 0;
}

int test() {
    int ret = 0;
    int dirCnt = randInt(DIRCNT_MIN, DIRCNT_MAX);
    int blockSize = BLOCK_SIZE;
    printf("test start dirCnt:%d blockSize:%d \n", dirCnt, blockSize);

    char** dirs = initDirs(dirCnt,blockSize);
    if (NULL == dirs) {
        printf("init dir failed \n");
        return -1;
    }

    ret = test0(dirs, dirCnt, blockSize);
    checkret(ret);

    ret = testWriteSeekRead(dirs, dirCnt, blockSize);
    checkret(ret);

    ret = uninitDirs(dirs, dirCnt);
    checkret(ret);

    printf("test end \n");
    return ret;
}