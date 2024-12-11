#include<sys/socket.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<sys/stat.h>
#include<unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<pthread.h>
#define PORT_NUMBER 8888

typedef struct newword_s
{
	char* st;
	struct newword_s* next;
} newword;

typedef struct {
	void** ptr_off_size;
	newword** node;
	pthread_mutex_t* newword_lock;
	int new_conn;
	unsigned long id;
} tas_args;

extern void *tas(void* sock);
void calc_ptrs_offset_by_size(void **__restrict__ ptr_off_size, void *__restrict__ words_ptr, const unsigned int *__restrict__ lettercount);
void re_init(void *__restrict__ *filebuf, void *__restrict__ *words);
int main()
{
	const int opt = 1;
	pthread_mutex_t newword_lock; pthread_mutex_init(&newword_lock, NULL);
	void *filebuf = 0, *words = 0; //file content ordered by word length (offsets in ptr_off_size)
	struct sockaddr_in addr_local = {.sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(PORT_NUMBER)}, addr_new_conn;
	socklen_t addrsize = sizeof(addr_local), addrsize_peer = sizeof(addr_new_conn);
	newword* node = 0;

	re_init(&filebuf, &words);

        unsigned int lettercount[46]; // Pneumonoultramicroscopicsilicovolcanoconiosis
        memset(&lettercount, 0, sizeof(int) * 46);


	void* filebuf_ptr = filebuf;
        for(unsigned long c, l = 0; (c = *((char*)filebuf_ptr++)) != '\0';  )
{
        if(c == '\n')
{
                lettercount[l]++;
                l = 0;
}
        else
                l++;
}

        void *ptr_off_size[46] = { 0 };
        void *ptr_off_size_tmp = malloc(sizeof(ptr_off_size));
	void* words_ptr = words;
        calc_ptrs_offset_by_size(ptr_off_size, words_ptr, lettercount); // ptr_off_size[len] - ptr_off_size[len+1] -> addresses of words length len
        memcpy(ptr_off_size_tmp, ptr_off_size, sizeof(ptr_off_size));

	#ifdef DEBUG
        for(unsigned char l = 0; l < 46; l++)
		printf("ptr_off_size[%u]:%lu\n",l,*(long*)(ptr_off_size_tmp + (l*8)));
        getchar();
        #endif

	filebuf_ptr = filebuf;
        void* filebuf_prev_word_ptr = filebuf_ptr;
        int l = 0;
        for(unsigned char c; (c = *((char*)filebuf_ptr++)) != '\0';  ) // order by len at offset memory address ptr_off_size[len]
{
        if(c == '\n')
{
                unsigned int size = l;
                for(; l > 0; l--) // copy \n too
{
                *((char*)((long*)ptr_off_size_tmp)[size]) = tolower(*((char*)filebuf_prev_word_ptr++));
                ((long*)ptr_off_size_tmp)[size]++;
}
                *((char*)((long*)ptr_off_size_tmp)[size]) = '\0';
                ((long*)ptr_off_size_tmp)[size]++;
                filebuf_prev_word_ptr++;
                l = 0;
}
        else
                l++;
}

        free(ptr_off_size_tmp);


	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1)
{
	printf("Can't create socket, ret: %d\n",sock);
	exit(1);
}

	int opts = fcntl(sock, F_GETFL, NULL);
	opts |= O_NONBLOCK;
	fcntl(sock, F_SETFL, opts);

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
	pthread_mutex_lock(&newword_lock);

	#ifdef DEBUG2
	printf("head node: %lu\n",(unsigned long)node);
	#endif

	if(node != 0)
{
	#ifdef DEBUG2
	printf("node != 0\n");
	#endif

	newword* node_a = node;
	newword* to_free;

	do
{
	#ifdef DEBUG2
	printf("word: %s\n", node_a->st);
	#endif

	free(node_a->st);
	to_free = node_a;
	node_a = node_a->next;
	free(to_free);
}
	while(node_a != 0);

	node = 0;
} // end if(node != 0)

	pthread_mutex_unlock(&newword_lock);

	sleep(1);
}

	else
{
	getpeername(fd, (struct sockaddr*)&addr_new_conn, &addrsize_peer);
	printf("New connection from %s (id: %lu)\n",inet_ntoa(addr_new_conn.sin_addr), id);

	tas_args* arg = malloc(sizeof(tas_args));
	arg->new_conn = fd;
	arg->id = id;
	arg->ptr_off_size = ptr_off_size;
	arg->newword_lock = &newword_lock;
	arg->node = &node;

	pthread_t nonuse;
	pthread_create(&nonuse, &attr, tas, arg);
	id++;

}

}



}



void calc_ptrs_offset_by_size(void **ptr_off_size, void *words_ptr, const unsigned int *lettercount) // order in mem by wlen
{
        ptr_off_size[0] = 0;
        for(unsigned int i = 1; i < 46; i++)
{
        words_ptr += lettercount[i-1] * i; // offset(1) always at 0
        ptr_off_size[i] = words_ptr;

}

}

void re_init(void *__restrict__ *filebuf, void *__restrict__ *words)
{

        int fd;

	if((fd = open("basic_english_2000.txt", O_RDONLY)) == -1)
{
        printf("Could not open dictionary file 'basic_english_2000.txt'\n");
        exit(1);
}

        struct stat filestats_s;
        fstat(fd, &filestats_s);
        unsigned long filesize = filestats_s.st_size;
        if(filesize <= 0)
{
        printf("err in getting filesize\n");
        exit(1);
}

	*filebuf = malloc(filesize);
        *words = malloc(filesize);

        long charsread = read(fd,*filebuf,filesize);
        if(charsread == -1)
{
        printf("err in reading\n");
        exit(1);
}

	close(fd);

}
