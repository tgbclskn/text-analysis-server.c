#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "levenshtein.h"
#define _STR(x) #x
#define STR(x) _STR(x)
#define INPUT_CHARACTER_LIMIT 100
#define OUTPUT_CHARACTER_LIMIT 200
//#define LEVENSHTEIN_LIST_LIMIT 5
#define LEV_SIZE 5


typedef struct
{
	char* st;
	unsigned int len;
	unsigned int toadd;

} inputstr_s;


typedef struct
{
	void* ptr;
	unsigned int dist;

} lev_found_s;


typedef struct
{
	char* st;
	lev_found_s* lf_ptr;
	pthread_mutex_t* lk_ptr;
	void* mem_start;
	void* mem_end;
	unsigned int step;
	unsigned int w2len;
	#ifdef DEBUG
	unsigned int threadid;
	#endif
}	lev_thread_args_s;


typedef struct {
        void** ptr_off_size;
        pthread_mutex_t* file_lock;
        void* filebuf;
        int new_conn;
        unsigned long id;
} tas_args;


inputstr_s* get_input_words_struct(char *__restrict__ input);
void *lev_thread_f(void *__restrict__ args);
void *tas(void* sock);
void term(int sock, void* arg);
#ifdef DEBUG
void pr(const int threadid, lev_found_s* l);
#endif


void *tas(void* arg)
{
	int sock = ((tas_args*)arg)->new_conn;
	void** ptr_off_size = ((tas_args*)arg)->ptr_off_size;
//	void* filebuf = ((tas_args*)arg)->filebuf;
//	pthread_mutex_t* file_lock = ((tas_args*)arg)->file_lock;
//	const int id = ((tas_args*)arg)->id;


	char input[INPUT_CHARACTER_LIMIT + 5] = { 0 };

	write(sock, "Please enter your input string: \n=>", 35);

	read(sock, input, INPUT_CHARACTER_LIMIT + 3);


	for(unsigned int i = 0; ; i++)
{
	if(i == INPUT_CHARACTER_LIMIT + 1)
{
	const char* answer = "\nInput string is longer than " STR(INPUT_CHARACTER_LIMIT) "\n";
	write(sock, answer, strlen(answer));
	term(sock, arg);
}

	char c = input[i];

	if(c == 13 || c == 10 || c == 0)
{
	for(unsigned int j = i; j < sizeof(input); j++) input[j] = '\0';
	break;
}

	if(!((c >= 97 && c <= 122) || (c >= 65 && c <= 90) || c == 32))
{
	write(sock, "Input string contains unsupported characters\n", 45);
	term(sock, arg);
}

	input[i] = tolower(c);

}

#ifdef DEBUG
printf("input buffer::\n");
for(unsigned int i = 0; i < INPUT_CHARACTER_LIMIT; i++)
	printf("i:%u ch:|%c| d:%d\n",i,input[i],input[i]);
printf(":: end\n");
getchar();
#endif


inputstr_s *inputwords = get_input_words_struct(input);

#ifdef DEBUG
printf("inputstr list::\n");
for(inputstr_s* ptr = inputwords; ptr->len != 0; ptr++)
	printf("|%s| len:%d\n",ptr->st, ptr->len);
printf(":: end");
getchar();
#endif

lev_found_s lev_found[LEV_SIZE];
pthread_mutex_t lock;
pthread_mutex_init(&lock, NULL);
const unsigned int cpus = get_nprocs();
pthread_t ptid[cpus];
lev_thread_args_s thread_args[cpus];
int sign[2] = {-1, 1};

for(unsigned int i = 0; i < LEV_SIZE; i++)
{	lev_found[i].dist = 46;
	lev_found[i].ptr = 0;
}

unsigned int wn = 1;
for(inputstr_s* inputwords_ptr = inputwords;; inputwords_ptr++)
{

	if(inputwords_ptr->len == 0)
		break;


	for(int i = 0; i < 45; i++) // wlen + i / wlen - i
{

	pthread_mutex_lock(&lock);

	#ifdef DEBUG
	printf("lev_found[%u].dist(%u) < %u pass? ",(LEV_SIZE-1),lev_found[LEV_SIZE-1].dist,i);
	#endif

	if((unsigned int)i > lev_found[LEV_SIZE-1].dist)
{

	#ifdef DEBUG
	printf("yes\n");
	printf("going next\n");
	#endif

	pthread_mutex_unlock(&lock);
	break;
}

	#ifdef DEBUG
	printf("no\n");
	#endif

	pthread_mutex_unlock(&lock);
	for(unsigned int s = 0; s < 2; s++)
{
	#ifdef DEBUG
	printf("sign: %d\n",sign[s]);
	#endif

	if(sign[s] == -1 && (unsigned int)i >= inputwords_ptr->len)
		continue;

	unsigned int len_off = (inputwords_ptr->len) + (i*sign[s]);
	void* mem_start = ptr_off_size[len_off];
	void* mem_end = ptr_off_size[len_off+1];
	const unsigned int maxcpu = (mem_end-mem_start)/(len_off+1);

	#ifdef DEBUG
	printf("mem_start:%lu\n",(unsigned long)mem_start);
	printf("mem_end  :%lu\n",(unsigned long)mem_end);
	printf("signed offset:%d\n",i*sign[s]);
	printf("len_off->%u\n",len_off);
	printf("max cpu: %u\n",maxcpu);
	getchar();

	printf("listing::\n\n");
	for(void* temp_ptr = mem_start; temp_ptr < mem_end; temp_ptr += (len_off + 1))
		printf("%lu : %s\n",(long)temp_ptr, (char*)temp_ptr);
	printf(":: end list\n");
	getchar();
	#endif

	memset(thread_args, 0, sizeof(lev_thread_args_s) * cpus);
	for(unsigned int j = 0; j < cpus && j < maxcpu; j++)
{
	thread_args[j].st = inputwords_ptr->st;
	thread_args[j].mem_start = mem_start + j*(len_off + 1);
	thread_args[j].mem_end = mem_end;
	thread_args[j].w2len = len_off;
	thread_args[j].step = cpus;
	#ifdef DEBUG
	thread_args[j].threadid = j;
	#endif
	thread_args[j].lf_ptr = lev_found;
	thread_args[j].lk_ptr = &lock;

	#ifdef DEBUG
	printf("\nthread_args[%u].st:%s\n",j,thread_args[j].st);
	printf("thread_args[%u].mem_start:%lu\n",j,(unsigned long)thread_args[j].mem_start);
	printf("thread_args[%u].mem_end:%lu\n",j,(unsigned long)thread_args[j].mem_end);
	printf("thread_args[%u].w2len:%u\n",j,thread_args[j].w2len);
	printf("thread_args[%u].step:%u\n",j,thread_args[j].step);
	printf("thread_args[%u].threadid:%u\n\n",j,thread_args[j].threadid);
	printf("going to create thread: %u?\n",j);
	getchar();
	#endif

	pthread_create(&ptid[j], NULL, &lev_thread_f, &thread_args[j]);

	#ifdef DEBUG
	printf("ok, waiting\n");
	#endif

} //end j

	for(unsigned int j = 0; j < cpus && j < maxcpu; j++)
		pthread_join(ptid[j],NULL);
	if(i == 0)
		break; // 0 * sign is same twice

} //end sign


} //end wlen

	char outbuf[100]; int ans_len;
	ans_len = sprintf(outbuf, "\nWord %u: %s\nMATCHES: :", wn++, (char*)inputwords_ptr->st);
	write(sock, outbuf, ans_len);

	for(unsigned int i = 0; i < LEV_SIZE; i++)
{
	if(lev_found[i].ptr == 0) break;
	//memset(outbuf, 0, 100);
	ans_len = sprintf(outbuf, " %s(%u)", (char*)lev_found[i].ptr, lev_found[i].dist);
	write(sock, outbuf, ans_len);

} write(sock, "\n", 1);

	unsigned int answer = 'N';

	inputwords_ptr->toadd = (answer == 'y' || answer == 'Y')? 1 : 0;

	for(unsigned int i = 0; i < LEV_SIZE; i++)
{
	lev_found[i].ptr = 0;
	lev_found[i].dist = 46;
}


} //end inputstr

//	printf("end\n");


	// add new words to file
/*
	const unsigned long shift[] = {
        0x10000000000,
        0x08000000000,
        0x04000000000,
        0x02000000000,
        0x01000000000,
        0x00800000000,
        0x00400000000,
        0x00200000000,
        0x00100000000,
        0x00080000000,
        0x00040000000,
        0x00020000000,
        0x00010000000,
        0x00008000000,
        0x00004000000,
        0x00002000000,
        0x00001000000,
        0x00000800000,
        0x00000400000,
        0x00000200000,
        0x00000100000,
        0x00000080000,
        0x00000040000,
        0x00000020000,
        0x00000010000,
        0x00000008000,
        0x00000004000,
        0x00000002000,
        0x00000001000,

};


	unsigned int shcou = 0;
	for(unsigned int i = 0; i < 29; i++)
{
	if((filesize&shift[i]) == shift[i])
{
	shcou = 29-i;
	break;
}

}

//	printf("shift by:%u\n",shcou);


	for(inputstr_s* inputwords_ptr = inputwords; inputwords_ptr->st != 0 && inputwords_ptr->toadd == 1; inputwords_ptr++)
{ //for every word to be added

	filebuf_ptr = filebuf;

//	const unsigned long iwlen = strlen(inputwords_ptr);

	//start binary search
	for(unsigned int i = 0; i < shcou; i++)
{
	//



}



}*/


write(sock, "\nbye\n\n", 6);
free(inputwords);
term(sock, arg);

exit(1);
} //end main


inputstr_s* get_input_words_struct(char* input)
{
	char* input_p = input;
	unsigned int inputlen = 0;

	for(; *input_p != '\0'; input_p++) inputlen++;
	input_p = input;

	const char *input_prev_p = input_p;
	char* cursor = 0;
	inputstr_s *inputwords = malloc(sizeof(inputstr_s) * (inputlen + 1)), *inputwords_p = inputwords;
	memset(inputwords, 0, sizeof(inputstr_s) * inputlen);
	for(unsigned int i = 0, cur = 0; i < inputlen + 1; i++ )
{
	if(*input_p == ' ' || *input_p == '\0')
{
	if((i-cur) == 0)
{
	input_p++;
	input_prev_p++;
	cur++;
	continue;
}

	inputwords_p->len = i - cur;
	for(unsigned int j = 0; j < inputwords_p->len; j++)
{
	if(!cursor)
{
		inputwords_p->st = malloc(inputwords_p->len + 1);
		cursor = inputwords_p->st;
}
	*cursor++ = *input_prev_p++;


}
	*cursor = '\0';
	cursor = 0;
	inputwords_p->toadd = 0;
	inputwords_p++;
	input_prev_p++;
	cur = i+1;

} //end if( == ' ')

	input_p++;

}

	return inputwords;

}


void *lev_thread_f(void *void_arg)
{
	lev_thread_args_s* args = void_arg;
	const char* st = args->st;
	lev_found_s* lev_found = args->lf_ptr;
	pthread_mutex_t* lock_p = args->lk_ptr;
	void* mem_start = args->mem_start;
	void* mem_end = args->mem_end;
	const unsigned int step = args->step;
	#ifdef DEBUG
	const unsigned int threadid = args->threadid;
	#endif
	const unsigned int w2len = args->w2len;
	const unsigned int wlen = strlen(st);

	#ifdef DEBUG
	printf("thread (%u) args:\n",threadid);
	printf("st:%s\n",st);
	printf("mem_start:%lu\n",(long)mem_start);
	printf("mem_end:%lu\n",(long)mem_end);
	printf("w2len:%u\n",w2len);
	printf("step:%u\n",step);
	getchar();
	#endif

	unsigned int closest_dist_arr[LEV_SIZE];
	void* closest_ptr_arr[LEV_SIZE];
	for(unsigned int i = 0; i < LEV_SIZE; i++)
{
	closest_dist_arr[i] = 46;
	closest_ptr_arr[i] = 0;
}

	unsigned int dist_tmp1, dist_tmp2;
	void *ptr_tmp1, *ptr_tmp2;

	for(void* mem_ptr = mem_start; mem_ptr < mem_end; mem_ptr += ((w2len + 1) * step))
{


//FOR
	unsigned int l = levenshtein_n((char*)mem_ptr, w2len, st, wlen);

	#ifdef DEBUG
	printf("\n(%u)new word:%s %u\n%lu\n",threadid,(char*)mem_ptr,l,(unsigned long)mem_ptr);
	#endif

	for(unsigned int i = 0; i < LEV_SIZE; i++)
{ //FOR L

	#ifdef DEBUG
	printf("(%u) add at %u?: ",threadid,i);
	#endif

	if(l < closest_dist_arr[i])
{ //IF

	#ifdef DEBUG
	printf("(%u) yes\n",threadid);
	printf("(%u)add at %u\n",threadid,i);
	#endif

	dist_tmp1 = l;
	ptr_tmp1 = mem_ptr;

	for(unsigned int j = i; j < LEV_SIZE; j++)
{
	ptr_tmp2 = closest_ptr_arr[j];
	dist_tmp2 = closest_dist_arr[j];
	closest_ptr_arr[j] = ptr_tmp1;
	closest_dist_arr[j] = dist_tmp1;
	ptr_tmp1 = ptr_tmp2;
	dist_tmp1 = dist_tmp2;
}
	#ifdef DEBUG
	printf("\nswapped\n");
	for(unsigned int k = 0; k < LEV_SIZE; k++)
{
	printf("(%u) [%u]: %s %u\n",threadid,k,(char*)closest_ptr_arr[k],closest_dist_arr[k]);

}
	getchar();
	#endif

	break;

} //IF

	#ifdef DEBUG
else
{
	printf("(%u) dont add at %u\n",threadid,i);
}
	#endif

} //FOR L



//FOR

} //end for mem_ptr


	#ifdef DEBUG
	printf("(%u) end thread\n\n",threadid);
	for(unsigned int i = 0; i < LEV_SIZE; i++)
		printf("(%u) [%u]:%s  dist:%u\n",threadid,i,(char*)closest_ptr_arr[i],closest_dist_arr[i]);
	printf("\n\n\n");
	printf("adding to lev_found[]\n");
	#endif

	pthread_mutex_lock(lock_p);
	unsigned int lastpos = 0;

	for(unsigned int i = 0; i < LEV_SIZE; i++)
{
	if(closest_ptr_arr[i] == 0)
		break;

	for(unsigned int j = lastpos; j < LEV_SIZE; j++)

{
	lastpos = LEV_SIZE;
	if(closest_dist_arr[i] <= lev_found[j].dist)

{
	if(closest_dist_arr[i] == lev_found[j].dist && strcmp(closest_ptr_arr[i],(char*)lev_found[j].ptr) > 0)
		continue;

	ptr_tmp1 = closest_ptr_arr[i];
	dist_tmp1 = closest_dist_arr[i];

	for(unsigned int k = j; k < LEV_SIZE; k++)
{
	ptr_tmp2 = lev_found[k].ptr;
	dist_tmp2 = lev_found[k].dist;
	lev_found[k].ptr = ptr_tmp1;
	lev_found[k].dist = dist_tmp1;
	ptr_tmp1 = ptr_tmp2;
	dist_tmp1 = dist_tmp2;
}

	lastpos = j;
	break; // break on add

} //end if



} //end for j



} //end for i

#ifdef DEBUG
printf("(%u) added::\n",threadid);
pr(threadid, lev_found);
printf("\n");
getchar();
#endif

pthread_mutex_unlock(lock_p);
pthread_exit(NULL);

}


#ifdef DEBUG
void pr(const int threadid, lev_found_s* l)
{
	for(unsigned int i = 0; i < LEV_SIZE; i++)
{
	printf("(%u) [%u]->str:%s  dist:%u\n",threadid,i,(char*)l[i].ptr,l[i].dist);

}
	printf("\n");

}
#endif


void term(int sock, void* arg)
{
	close(sock);
	free(arg);
	pthread_exit(NULL);

}


