#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#define FIFO_NAME "myfifo"
#define BUFFER_SIZE 300

uint32_t bytesWrote, dataWrote;
int32_t returnCode, fd;

char getinfo[BUFFER_SIZE];
char data[BUFFER_SIZE];

char* SIGUSER1 = "SIGN: 1";
char* SIGUSER2 = "SIGN: 2";

/* PROTOTIPOS */
void signalwritesigusr1(int sig);
void signalwritesigusr2(int sig);

int main(void)
{
	signal(SIGUSR1,signalwritesigusr1);
	signal(SIGUSR2,signalwritesigusr2);

	/*	Crear fifo nombrada*/
	/*	returnCode devuelve un numero que hace referencia al file descriptor */
	if ((returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0)) < -1)
	{
		printf("Error creando la fifo nombrada: %d\n", returnCode);
		exit(1);
	}

	/* Abro la named fifo y se bloquea el proceso esperando lectores del otro lado */
	printf("Esperando lectores\n");
	if ((fd = open(FIFO_NAME, O_WRONLY)) < 0)
	{
		printf("Error abriendo la fifo nombrada: %d\n", fd);
		exit(1);
	}

	/* Loop forever */
	while (1)
	{
		/* Recibir texto por consola */
		fgets(getinfo, BUFFER_SIZE, stdin);
		/* Agrego "DATA:" en el buffer a enviar */
		sprintf(data, "DATA: %s", getinfo);
		/* Write buffer to named fifo. Strlen - 1 to avoid sending \n char */
		if ((bytesWrote = write(fd, data, strlen(data) - 1)) == -1)
		{
			perror("write");
		}
		else
		{
			printf("writer: wrote %d bytes\n", bytesWrote);
		}
	}
	return 0;
}

void signalwritesigusr1(int sig)
{
	write(fd, SIGUSER1, sizeof(SIGUSER1));
}

void signalwritesigusr2(int sig)
{
	write(fd, SIGUSER2, sizeof(SIGUSER2));
}
