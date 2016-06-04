#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

char *GenDict(const char *key, char dict[])
{
    int i = 0;
    int j = 0;
    int keyLen = strlen(key);

    for(i = 0; i < keyLen; ++i) {
        for (j = keyLen - 1; j > -1; --j) {
            if (key[i] == key[j]) {
                dict[key[i]] = keyLen - j;
                break;
            }
        }
    }

    return dict;
}


int sunday(const char *src, const char *dst, uint64_t *line, int64_t *pos)
{
    char dict[128] = {0};
    int srcLenth = strlen(src);
    int dstLenth = strlen(dst);
    int j = 0;
    int i = 0;

    *line = 0;
    *pos = -1;

    if (0 == dstLenth) {
        *pos = srcLenth;
        printf("%ld\n", *pos);
        return 0;
    }

    if (0 == srcLenth) {
        *pos = -1;
        printf("%ld\n", *pos);
        return 0;
    }

    GenDict(dst, dict);
    for (i = 0; i < 128; ++i) {
        if (dict[i] > 0) {
            printf("%c: %d\n", i, dict[i]);
        }
    }

    while (j < srcLenth) {
        int _pos = 0;
        for (i = 0; src[i + j] && dst[i] && src[i + j] == dst[i]; ++i) {
            ;
        }

        if (i == dstLenth) {
            printf("%d~%d\n", j, j+dstLenth-1);
            *line = 0;
            *pos = j;
        }

        _pos = dict[src[j+dstLenth]];
        j += (_pos ? _pos : (dstLenth + 1));
    }
}

int main()
{
    int i = 0;
    uint64_t line = 0;
    uint64_t pos = 0;

    //sunday("example: this is a sample example, yes, it is a example", "", &line, &pos);//"example");
    sunday("a", "asdf", &line, &pos);//"example");

    //for (i = 0; i < 128; ++i) {
    //    if (dict[i] > 0) {
    //        printf("%c: %d\n", i, dict[i]);
    //    }
    //}

    puts("");

    return 0;
}
