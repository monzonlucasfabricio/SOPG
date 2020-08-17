#include <stdio.h>
#include <stdlib.h>
#include "SerialManager.h"
#include <sys/signal.h>
#include <sys/socket.h>
#include "SerialManager.h"
#include "rs232.h"
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define LISTENQUEUE 10
#define BAUDRATE 115200
#define TTYYUSB1 1
#define PUERTO 10000

int32_t fd_socket, listenreturn, bindreturn, newfd, n, retwrite;
uint32_t bytesLeidos;
socklen_t addr_len;
struct sockaddr_in serveraddr;
struct sockaddr_in clientaddr;
struct sigaction sa0;
struct sigaction sa1;
char serialbuffer[17];
char socketbuffer[128];
char auxbuffer[18];
char auxbuffer1[10];
uint32_t sizebuffer = sizeof(serialbuffer);
pthread_t thread;
bool conection,closeAll;
pthread_mutex_t mutexData = PTHREAD_MUTEX_INITIALIZER;

bool socket_setup(void);
bool serial_setup(void);
bool serial_check(void);
bool socket_send(void);
void *socket_thread(void *arg);
int32_t aceptar_conexiones(void);


void SIGTERM_HANDLER(int sig){
	closeAll = true;
}
void SIGINT_HANDLER(int sig){
	closeAll = true;
}

int main(void)
{
	sa0.sa_handler = SIGTERM_HANDLER;
	sa1.sa_handler = SIGINT_HANDLER;

	sa0.sa_flags = SA_RESTART;
	sa1.sa_flags = SA_RESTART;

	sigemptyset(&sa0.sa_mask);
	sigemptyset(&sa1.sa_mask);

	if(sigaction(SIGTERM,&sa0,NULL) == -1){
		perror("sigaction");
		exit(1);
	}
	if(sigaction(SIGINT,&sa1,NULL) == -1){
		perror("sigaction");
		exit(1);
	}


	/* Inicio el puerto serie */
	bool retserial = serial_setup();
	if (retserial == 1)
	{
		return retserial;
	}

	/* Inicio el socket */
	bool retsocket = socket_setup();
	if (retsocket == 1)
	{
		return retsocket;
	}

	conection = false;
	closeAll = false;

	if(pthread_create(&thread, NULL, socket_thread, NULL) != 0){
		printf("No se puede crear el thread");
		return 1;
	}

	while (1)
	{
		bool ret = serial_check();
		if(ret == true){
			socket_send();
		}

		if(closeAll){
			printf("Se cierra todo.. saludos\r\n");
			close(newfd);
			close(fd_socket);
			pthread_cancel(thread);
			int err = pthread_join(thread,NULL);
			if(err == EINVAL){
				printf("Thread no joineable\n");
			}
			exit(EXIT_FAILURE);
		}
	}

	exit(EXIT_SUCCESS);
	return 0;
}

bool socket_send(void)
{
	printf("RECIBI POR LA UART: %s\r\n",serialbuffer);

	//Creacion del buffer para enviar por el socket
	sprintf(auxbuffer1, ":LINE%cTG\n", serialbuffer[14]);
	printf("SE ENVIO POR EL SOCKET: %s\r\n", auxbuffer1);

	pthread_mutex_lock(&mutexData);
	if(conection == true){

	//Escribo por el socket
	int32_t retval = write(newfd, auxbuffer1, sizeof(auxbuffer1));
	if (retval == -1)
	{
		perror("Error escribiendo mensaje en el socket\r\n");
		exit(1);
	}
	}
	pthread_mutex_unlock(&mutexData);
}

bool serial_check(void)
{
	bool retval = 0;

	// Se usa mutex por el recurso compartido
	bytesLeidos = serial_receive(serialbuffer, sizebuffer);

	/* Si recibi algo checkear*/
	if (bytesLeidos > 0)
	{
		if(strcmp(serialbuffer,">OK\r\n") == 0){
			printf("SE RECIBIO OK\r\n");
		}

		if(strcmp(serialbuffer,">TOGGLE STATE:0\r\n")==0
		||	strcmp(serialbuffer,">TOGGLE STATE:1\r\n")==0
		||	strcmp(serialbuffer,">TOGGLE STATE:2\r\n")==0
		||	strcmp(serialbuffer,">TOGGLE STATE:3\r\n")==0)
		{
			retval = true;
		}
	} 	
	usleep(100);

	/* Retorna true si recibi un cambio de esado de pulsadores */
	return retval;
}

bool serial_setup(void)
{
	int32_t resultserial;
	printf("INICIO CONEXION CON LA EDU-CIAA\r\n");
	resultserial = serial_open(TTYYUSB1, BAUDRATE);
	return resultserial;
}

bool socket_setup(void)
{
	/* Se crea el socket. IP Y PUERTO LOCAL */
	int enable = 1;
	printf("INICIO EL SOCKET DE INTERNET\r\n");

	/* Inicio un socket */
	fd_socket = socket(AF_INET, SOCK_STREAM, 0);

	if (setsockopt(fd_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	{
		perror("setsockopt(SO_REUSEADDR) failed");
	}
 
	if (fd_socket < 0)
	{
		perror("socket() failed");
		exit(-1);
	}

	/* Cargo la estructura serveraddr para pasarle a bind */
	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(PUERTO); // Cargamos el puerto local

	if (inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr)) <= 0)
	{
		fprintf(stderr, "ERROR invalid server IP\r\n");
		return 1;
	}

	/* Hago un bind */
	//printf("Hago un bind\r\n");
	bindreturn = bind(fd_socket, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (bindreturn == -1)
	{
		close(fd_socket);
		perror("Listener: bind\r\n");
		return 1;
	}
	//printf("Bind devuelve: %i\r\n", bindreturn);

	/* Hago un listen */
	//printf("Hago un listen\r\n");
	listenreturn = listen(fd_socket, LISTENQUEUE);
	if (listenreturn == -1)
	{
		perror("Error en listen\r\n");
		exit(1);
	}
	//printf("Listen devuelve: %i\r\n", listenreturn);

	/* Si todo sale bien */
	return 0;
}

int32_t aceptar_conexiones(void)
{
	int32_t fd;
	printf("ESPERANDO CONEXION..\r\n");
	addr_len = sizeof(struct sockaddr_in);
	fd = accept(fd_socket, (struct sockaddr *)&clientaddr, &addr_len);
	if (fd == -1)
	{
		perror("Error en accept\r\n");
		exit(1);
	}
	//printf("El nuevo fd es: %i\r\n", fd);

	/* Retorno el nuevo file descriptor */
	return fd;
}

void *socket_thread(void *arg)
{
	while (1)
	{
		newfd = aceptar_conexiones();
		if (newfd > 0)
		{
			char ipClient[32];
			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
			printf("EL CLIENTE SE HA CONECTADO, DESDE: %s\r\n",ipClient);
			pthread_mutex_lock(&mutexData);
			conection = true;
			pthread_mutex_unlock(&mutexData);
		}
		while (conection == true)
		{
			// Espero a leer algo
			n = read(newfd, socketbuffer, 128);
			if (n == -1)
			{
				perror("Error leyendo mensaje en socket\r\n");
				return NULL;
			}

			if (n == 0)
			{
				printf("EL CLIENTE SE HA DESCONECTADO\r\n");

				/* Salgo de la conexion y vuelvo al accept */
				pthread_mutex_lock(&mutexData);
				conection = false;
				pthread_mutex_unlock(&mutexData);
			}

			if (n > 0)
			{
				socketbuffer[n] = 0x00;
				printf("RECIBI POR EL SOCKET: %s", socketbuffer);

				//Creacion del string para enviar por la UART
				sprintf(auxbuffer, ">OUTS:%c,%c,%c,%c\r\n", socketbuffer[7], socketbuffer[8], socketbuffer[9], socketbuffer[10]);
				printf("SE ENVIO POR LA UART : %s\r\n", auxbuffer);

				//Uso de mutex por el recurso compartido
				serial_send(auxbuffer, sizeof(auxbuffer));
			}
		}
	}
}
