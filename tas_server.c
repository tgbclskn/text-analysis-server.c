#include<sys/socket.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#define PORT_NUMBER 8888
extern void *tas(void* sock);
typedef struct { int new_conn; unsigned long id; } tas_args;


int main()
{
	const int opt = 1;
	struct sockaddr_in addr_local = {.sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(PORT_NUMBER)}, addr_new_conn;
	socklen_t addrsize = sizeof(addr_local), addrsize_peer = sizeof(addr_new_conn);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1)
{
	printf("Can't create socket, ret: %d\n",sock);
	exit(1);
}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

	if(bind(sock, (struct sockaddr*)&addr_local, sizeof(addr_local)))
{
	printf("Can't bind socket\n");
	exit(1);
}

	if(listen(sock, 128))
{
	printf("Can't listen\n");
	exit(1);
}

	unsigned long id = 1;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	printf("listening...\n");

	while(1)
{
	int fd = accept(sock, (struct sockaddr*)&addr_local, &addrsize);
	if(fd == -1)
{
	printf("Can't accept connection\n");
	exit(1); //!
}

	getpeername(fd, (struct sockaddr*)&addr_new_conn, &addrsize_peer);
	printf("New connection from %s (id: %lu)\n",inet_ntoa(addr_new_conn.sin_addr), id);

	tas_args* arg = malloc(sizeof(tas_args));
	arg->new_conn = fd;
	arg->id = id;

	pthread_t nonuse;
	pthread_create(&nonuse, &attr, tas, arg);
	id++;
}



}
