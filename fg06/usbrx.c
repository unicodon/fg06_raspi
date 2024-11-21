#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

static const uint8_t HEADER = 0xFF;

static int find_header(uint8_t* buf, size_t size)
{
    for (int i = 0; i < size; i++) {
        if (buf[i] == HEADER) {
            printf("header found\n");
            return i;
        }
    }
    return -1;
}

static int get_num(uint8_t* buf, size_t size)
{
    for (int i = 0; i < size; i++) {
        if (buf[i] == HEADER) {
            return i;
        }
    }
    return -1;
}

int main(void)
{
    static const int NUM = 111;
    uint8_t buf[sizeof(uint32_t) * NUM];

    int fd;
    int ret;

    fd = open("/dev/hidg0", O_RDONLY);
    if (fd < 0) {
        printf("error: open() failed: %d %d\n", fd, errno);
        return fd;
    }

    int num = 0;
    int hi = 0;
    while(1)
    {
        ret = read(fd, buf, sizeof(buf));
        if (ret < 0) {
            printf("error: read() failed: %d %d\n", ret, errno);
            return ret;
        }
        hi = find_header(buf, sizeof(buf));
        if (hi < 0 || hi == (sizeof(buf) - 1)) {
            continue;
        }
        //printf("%x", buf[1]);
    }

    close(fd);
}
