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

#define MAX_READ        4096
#define BUFFER_SIZE     10 * 1024 * 1024
#define MAX_CHAR        256

#if 0
#define DEBUG(fmt, args...) printf("[%s] [%s:%d]: "fmt"\n",\
                                   __func__, __FILE__, __LINE__, ##args)
#else
#define DEBUG(fmt, args...)
#endif

#define PERROR(fmt, args...) fprintf(stderr, "ERROR: [%s] [%s:%d]: "fmt"\n",\
                                     __func__, __FILE__, __LINE__, ##args)

void help(const char *proc)
{
    printf("Usage: %s <-f file> "
                     "[-k key, keyword to search from file] "
                     "[-l line, line to print info in file]\n", proc);
}

typedef struct FileBucket
{
    off_t offsetInFile;

    uint64_t baseLine;
    uint64_t lines;
    char *lineEndPos[BUFFER_SIZE];

    ssize_t bytesLoad;
    char buffer[BUFFER_SIZE]; // 10M
} FileBucket;

const char *BucketToString(const FileBucket *bucket)
{
    static char buffer[4096] = {0};

    if (NULL == bucket) {
        PERROR("bucket is NULL");
        return NULL;
    }

    sprintf(buffer, "offsetInFile: %lld, baseLine: %llu, lines: %llu, bytesLoad: %zd",
                    bucket->offsetInFile, bucket->baseLine, bucket->lines, bucket->bytesLoad);

    return buffer;
}

#define BucketLineStart(bucket, line) ((line) > 1 ? (bucket)->lineEndPos[(line) - 1] + 1 : (bucket->buffer))
#define BucketBytesBeforeLine(bucket, line) ((line) > 0 ? (bucket)->lineEndPos[(line) - 1] - (bucket)->buffer + 1 : 0)
ssize_t BucketLineLen(FileBucket * bucket, uint64_t line)
{
    ssize_t len = 0;

    if (NULL == bucket) {
        PERROR("bucket is NULL");
        abort();
    }
    if (line < 1) {
        PERROR("line: %llu < 1", line);
        abort();
    }

    if (line - bucket->baseLine == bucket->lines) { // critical condtion
        if (0 == bucket->baseLine) {
            len = bucket->lineEndPos[line - bucket->baseLine] - bucket->lineEndPos[line - bucket->baseLine - 1];
        }
        else if (0 == bucket->lines) {
            len = bucket->bytesLoad;
        }
        else {
            len = (bucket->bytesLoad - BucketBytesBeforeLine(bucket, line));
        }
    }
    else if (line - bucket->baseLine > bucket->lines){
        PERROR("ERROR: bucket: %s, never to go this !!!", BucketToString(bucket));
        abort();
    }
    else {
        len = bucket->lineEndPos[line - bucket->baseLine] - bucket->lineEndPos[line - bucket->baseLine - 1];
    }

    return len;
}

int ReloadBuffer(int fd, FileBucket *bucket)
{
    int old = errno;
    int ret = 0;
    int lines = 0;
    uint64_t i = 0;
    ssize_t bytes = 0;
    ssize_t readn = 0;
    ssize_t start = bucket->offsetInFile + bucket->bytesLoad;

    if (fd < 0 || NULL == bucket) {
        PERROR("fd: %d < 0 or bucket: %p is NULL", fd, bucket);
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

    // FIXME: tips
    bucket->lineEndPos[0] = bucket->buffer - 1;

    for (i = 0; i < readn; ++i) {
        assert(bucket->buffer[i]);

        if ('\n' == bucket->buffer[i]) {
            bucket->lineEndPos[++lines] = &bucket->buffer[i];
        }
    }

    if (readn > 0) {
        bucket->offsetInFile += bucket->bytesLoad;
        bucket->bytesLoad = readn;

        bucket->baseLine += bucket->lines;

        bucket->lines = lines;
    }

    errno = old;

    return bucket->bytesLoad;
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

// Algorithm:
// sunday: http://blog.163.com/yangfan876@126/blog/static/80612456201342205056344
// preprocessing: O(m), calc: O(m * n)
int SearchKey(int fd, FileBucket *bucket, const char *key)
{
    // TODO: maybe need to ReloadBuffer from fd if file size bigger than BUFFER_SIZE
    ssize_t srcLenth = bucket->bytesLoad;

    char dict[MAX_CHAR] = {0};
    int keyLenth = strlen(key);
    const char *src = bucket->buffer;
    int j = 0;
    int i = 0;
    int count = 0;

    if (fd < 0) {
        PERROR("fd < 0");
        return -1;
    }

    if (0 == keyLenth) {
        return 0;
    }

    if (0 == srcLenth) {
        return 0;
    }

    if (NULL == bucket || NULL == key) {
        PERROR("bucket: %p or key: %p is NULL", bucket, key);
        return -1;
    }

    // for debug
    DEBUG("%s", BucketToString(bucket));

    GenDict(key, dict);

    while (j < srcLenth) {
        for (i = 0; i + j < srcLenth && i < keyLenth && src[i + j] == key[i]; ++i) {
            ;
        }

        if (i == keyLenth) {
            printf("%d~%d\n", j, j + keyLenth - 1);
            ++count;
        }

        j += dict[src[j + keyLenth]];
    }

    return count;
}

int SearchLine(int fd, FileBucket *bucket, uint64_t line, char buff[], size_t buffLen)
{
    int err = -1;

    if (fd < 0) {
        PERROR("fd < 0");
        return -1;
    }

    if (line < 1) {
        PERROR("request line: %llu less than 1", line);
        return -1;
    }

    if (NULL == bucket) {
        PERROR("bucket is NULL");
        return -1;
    }

    // for debug
    DEBUG("%s", BucketToString(bucket));

    if (line - bucket->baseLine <= bucket->lines) {
        write(1, BucketLineStart(bucket, line), BucketLineLen(bucket, line)); // printf '\n'
        //char *start = BucketLineStart(bucket, line);
        //ssize_t len = BucketLineLen(bucket, line);
        //ssize_t writen = 0;
        //if (len > 0) {
        //    writen = (len > 4096 ? 4096 : len);
        //    writen = write(1, start, writen);

        //    start += writen;
        //    len -= writen;
        //}
        DEBUG("\nstart: %ld, len: %ld\n", BucketLineStart(bucket, line) - BucketLineStart(bucket, 1),
              BucketLineLen(bucket, line));

        if (buff != NULL) {
            ssize_t len = (buffLen - 1 > BucketLineLen(bucket, line) ? BucketLineLen(bucket, line) : buffLen - 1);
            memcpy(buff, BucketLineStart(bucket, line), len);
            buff[len] = '\0';
        }

        err = 0;
    }
    else {
        // TODO: need to reload buffer from file
        PERROR("request line: %llu not found in FileBucket: %s, "
               "need to reload buffer from file! "
               "DO NOT DEAL THIS CASE !!!", line, BucketToString(bucket));

        err = -1;
    }

    return err;
}

int main(int argc, char *argv[])
{
    int i = 0;
    int fd = -1;
    int ret = 0;
    FileBucket *bucket = NULL;
    const char *key = NULL;
    const char *line = NULL;
    const char *file = NULL;

    if (argc < 3 || !(argc & 1)) {
        help(argv[0]);
        return -1;
    }

    errno = 0;

    for (i = 1; i < argc - 1; ++i) {
        if (0 == strcmp(argv[i], "-k")) {
            key = argv[++i];
        }
        else if (0 == strcmp(argv[i], "-l")) {
            line = argv[++i];
        }
        else if (0 == strcmp(argv[i], "-f")) {
            file = argv[++i];
        }
        else {
            PERROR("unknow param: %s", argv[i]);
            return -1;
        }
    }

    if (NULL == file) {
        PERROR("need param: file with '-f'");
        help(argv[0]);
        return -1;
    }

    // reload
    fd = open(file, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    bucket = malloc(sizeof(FileBucket));
    if (NULL == bucket) {
        perror("malloc");
        return -1;
    }
    memset(bucket, 0, sizeof(FileBucket));
    if (ReloadBuffer(fd, bucket) < 0) {
        perror("ReloadBuffer");
        close(fd);
        free(bucket);
        return -1;
    }

    if (NULL != key) {
        // search key in file
        // printf("find key count = %d\n", SearchKey(fd, bucket, argv[1]));
        printf("search key: %s\n", key);
        ret = SearchKey(fd, bucket, key);
    }

    if (NULL != line) {
        // show line info
        printf("show line: %s\n", line);
        ret = SearchLine(fd, bucket, atoll(line), NULL, 0);
    }

    close(fd);
    free(bucket);

    return (ret >= 0 ? 0 : -1);
}
