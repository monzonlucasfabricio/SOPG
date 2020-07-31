#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#define FIFO_NAME "myfifo"
#define BUFFER_SIZE 300

void Save(uint8_t option, char *input);
uint8_t CheckInput(char *input);

char inputBuffer[BUFFER_SIZE];
int32_t bytesRead, returnCode, fd;
uint8_t result;

int main(void)
{
    /* Create named fifo. -1 means already exists so no action if already exists */
    if ((returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0)) < -1)
    {
        printf("Error creating named fifo: %d\n", returnCode);
        exit(1);
    }

    /* Open named fifo. Blocks until other process opens it */
    printf("waiting for writers...\n");
    if ((fd = open(FIFO_NAME, O_RDONLY)) < 0)
    {
        printf("Error opening named fifo file: %d\n", fd);
        exit(1);
    }

    /* open syscalls returned without error -> other process attached to named fifo */
    printf("got a writer\n");

    /* Loop until read syscall returns a value <= 0 */
    do
    {
        /* read data into local buffer */
        if ((bytesRead = read(fd, inputBuffer, BUFFER_SIZE)) == -1)
        {
            perror("read");
        }
        else
        {
            inputBuffer[bytesRead] = '\0';
            printf("reader: read %d bytes: \"%s\"\n", bytesRead, inputBuffer);
            result = CheckInput(inputBuffer);
            Save(result, inputBuffer);
        }
    } while (bytesRead > 0);

    return 0;
}


uint8_t CheckInput(char *input)
{
    char SIGN[] = {"SIGN"};
    char DATA[] = {"DATA"};
    uint8_t option = 0;
    if (strncmp(input, SIGN, strlen(SIGN)) == 0)
    {
        option = 1;
    }
    if (strncmp(input, DATA, strlen(DATA)) == 0)
    {
        option = 2;
    }
    return option;
}


void Save(uint8_t option, char *input)
{
    char *ret;
    switch (option)
    {
    case 1:
    {
        ret = strstr(input, " ");
        int filedesc = open("Sign.txt", O_WRONLY | O_APPEND);
        if (write(filedesc, ret, strlen(ret)) == -1)
            {
                perror("write");
            }
    }
    break;
    case 2:
    {
        ret = strstr(input, " ");
        int filedesc = open("Log.txt", O_WRONLY | O_APPEND);
        if (write(filedesc, ret, strlen(ret)) == -1)
            {
                perror("write");
            }
    }
    break;
    }
}
