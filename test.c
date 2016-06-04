#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#define MAX_READ        64
#define BUFFER_SIZE     64
#define MAX_CHAR        256

#define PRINT(fmt, args...) printf("%s %d: "fmt"\n", __func__, __LINE__, ##args)

typedef struct FileBucket
{
    uint64_t oldBytesLoad;

    uint64_t startLine;
    uint64_t lines;
    uint64_t currentBytesLoad;

    uint64_t lineEndPos[BUFFER_SIZE];

    char buffer[BUFFER_SIZE]; // 5M
} FileBucket;

int ReloadBuffer(int fd, FileBucket *bucket)
{
    int old = errno;
    int ret = 0;
    int lines = 0;
    uint64_t i = 0;
    size_t bytes = 0;
    size_t readn = 0;
    size_t start = bucket->currentBytesLoad;

    if (fd < 0 || NULL == bucket) {
        return -EINVAL;
    }

    while (readn < BUFFER_SIZE) {
        bytes = pread(fd, bucket->buffer + readn, MAX_READ, start);

        if (bytes < 0) {
            perror("pread");
            break;
        }
        else if (0 == bytes) {
            break;
        }

        readn += bytes;
        start += bytes;
    }

    for (i = 0; i < readn; ++i) {
        assert(bucket->buffer[i]);
        if ('\n' == bucket->buffer[i]) {
            bucket->lineEndPos[lines++] = i + 1;
        }
    }

    bucket->currentBytesLoad = readn;
    bucket->startLine = bucket->startLine + bucket->lines;
    bucket->line = lines;

    errno = old;

    return bucket->currentBytesLoad;
}

char *GenDict(const char *key, char dict[])
{
    int i = 0;
    int j = 0;
    int keyLen = strlen(key);

    for (i = 0; i < MAX_CHAR; ++i) {
        dict[i] = keyLen + 1;
    }

    for (i = 0; i < keyLen; ++i) {
        dict[key[i]] = keyLen - i;
    }

    return dict;
}

int SearchKey(FileBucket *bucket, const char *key)
{
    char dict[MAX_CHAR] = {0};
    int keyLenth = strlen(key);
    uint64_t srcLenth = bucket->currentBytesLoad;
    char *src = (char *)bucket->buffer;
    int j = 0;
    int i = 0;
    int count = 0;

    if (0 == keyLenth) {
        return 0;
    }

    if (0 == srcLenth) {
        return 0;
    }

    GenDict(key, dict);

    while (j < srcLenth) {
        for (i = 0; i + j < srcLenth && i < keyLenth && src[i + j] == key[i]; ++i) {
            ;
        }

        if (i == keyLenth) {
            PRINT("%d~%d", j, j+keyLenth-1);
            ++count;
        }

        j += dict[src[j+keyLenth]];
    }

    return count;
}

int SearchLine(int fd, FileBucket *bucket, uint64_t line, char buff[], size_t buffLen)
{
    uint64_t start = 0;
    uint64_t len = 0;
    int err = -1;
    FileBucket *bucketPtr = bucket;
    FileBucket *tmp = NULL;

    while (1) {
        PRINT("bucketPtr: %p, %lu, %lu, %lu", bucketPtr, bucketPtr->currentBytesLoad, bucketPtr->startLine, bucketPtr->lines);
        if (line >= bucketPtr->startLine) {
            if (line - bucketPtr->startLine <= bucketPtr->lines) {
                start = (line > 1 ? bucketPtr->lineEndPos[line - 1 - 1] : 0);
                len = bucketPtr->lineEndPos[line - 1] - start;

                PRINT("%lu, %lu", start, len);
                write(1, bucketPtr->buffer + start, len - 1);

                if (buff != NULL) {
                    len = (buffLen - 1 > len ? len : buffLen - 1);
                    memcpy(buff, bucketPtr->buffer + start, len);
                    buff[len] = '\0';
                }

                err = 0;

                break;
            }
            else {
                if (NULL != tmp) {
                    free(tmp);
                    tmp = NULL;
                }

                tmp = malloc(sizeof(FileBucket));
                if (NULL == tmp) {
                    perror("malloc");
                    return -1;
                }
                memset(tmp, 0, sizeof(FileBucket));

                tmp->currentBytesLoad = bucketPtr->currentBytesLoad;
                tmp->startLine = bucketPtr->startLine;
                tmp->lines = bucketPtr->lines;

                int ret = ReloadBuffer(fd, tmp);
                if (ret < 0) {
                    PRINT("fail to ReloadBuffer");
                    free(tmp);
                    return ret;
                }
                if (0 == ret) {
                    PRINT("not more data to load from file");
                    free(tmp);
                    return ret;
                }

                bucketPtr = tmp;
            }
        }
        else {
            if (NULL != tmp) {
                free(tmp);
                tmp = NULL;
            }

            tmp = malloc(sizeof(FileBucket));
            if (NULL == tmp) {
                perror("malloc");
                return -1;
            }
            memset(tmp, 0, sizeof(FileBucket));

            tmp->currentBytesLoad = 0;
            int ret = ReloadBuffer(fd, tmp);
            if (ret < 0) {
                PRINT("fail to ReloadBuffer");
                free(tmp);
                return ret;
            }

            if (0 == ret) {
                PRINT("not more data to load from file");
                free(tmp);
                return ret;
            }
 
            bucketPtr = tmp;
        }

        sleep(1);
    }

    if (NULL != tmp) {
        free(tmp);
    }

    return err;
}

int main(int argc, char *argv[])
{
    int fd = -1;
    FileBucket *bucket = NULL;
    char key[1024] = {"hello"};
    char *res = NULL;

    errno = 0;

    bucket = malloc(sizeof(FileBucket));
    if (NULL == bucket) {
        perror("malloc");
        return -1;
    }
    memset(bucket, 0, sizeof(FileBucket));

    res = malloc(BUFFER_SIZE);
    if (NULL == res) {
        perror("malloc");
        return -1;
    }
    memset(res, 0, BUFFER_SIZE);

    fd = open("db", O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (ReloadBuffer(fd, bucket) < 0) {
        perror("ReloadBuffer");
        return -1;
    }

    //PRINT("%d, \n", SearchKey(bucket, argv[1]));

    SearchLine(fd, bucket, atoi(argv[2]), NULL, 0);

    close(fd);

    free(bucket);

    return 0;
}
