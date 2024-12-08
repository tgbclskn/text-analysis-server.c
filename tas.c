#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "levenshtein.h"
#define PTR_OFF_SIZE_TMP_TERM '\0'
#define _STR(x) #x
#define STR(x) _STR(x)
#define INPUT_CHARACTER_LIMIT 100
#define OUTPUT_CHARACTER_LIMIT 200
#define PORT_NUMBER 60000
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
	unsigned int threadid;
}	lev_thread_args_s;

int main();
void calc_ptrs_offset_by_size(void **__restrict__ ptr_off_size, void *__restrict__ words_ptr, const unsigned int *__restrict__ lettercount);
inputstr_s* get_input_words_struct(char *__restrict__ input);
void *lev_thread_f(void *__restrict__ args);
//void pr(const int threadid, lev_found_s* l);


int main()
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
	printf("err in getting filesize");
	exit(1);
}

	void *filebuf = malloc(filesize), *filebuf_ptr = filebuf;

	void *words = malloc(filesize), *words_ptr = words; //file content ordered by word length (offsets in ptr_off_size)
	long charsread = read(fd,filebuf_ptr,filesize);
	if(charsread == -1)
{
	printf("err in reading");
	exit(1);
}

	unsigned int lettercount[46]; // Pneumonoultramicroscopicsilicovolcanoconiosis
	memset(&lettercount, 0, sizeof(int) * 46);


	filebuf_ptr = filebuf;
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
	void *ptr_off_size_tmp = malloc(sizeof(ptr_off_size)); //copy pointer
	calc_ptrs_offset_by_size(ptr_off_size, words_ptr, lettercount); // ptr_off_size[len] - ptr_off_size[len+1] -> addresses of words length len
	memcpy(ptr_off_size_tmp, ptr_off_size, sizeof(ptr_off_size));

/*	for(unsigned char l = 0; l < 46; l++)
{
	printf("ptr_off_size[%u]:%lu\n",l,*(long*)(ptr_off_size_tmp + (l*8)));

}*/

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
		*((char*)((long*)ptr_off_size_tmp)[size]) = PTR_OFF_SIZE_TMP_TERM;
		((long*)ptr_off_size_tmp)[size]++;
		filebuf_prev_word_ptr++;
		l = 0;
}
	else
		l++;
}

	free(ptr_off_size_tmp);


	char input[INPUT_CHARACTER_LIMIT];

	printf("Please enter your input string: \n=>");
	for(unsigned long i = 0; ; i++)
{
	if(i == INPUT_CHARACTER_LIMIT)
{
	printf("Input string is longer than" STR(INPUT_CHARACTER_LIMIT) "\n");
	exit(1);
}

	char c = getchar();
	if(c == '\n')
{
	input[i] = '\0';
	break;
}

	if(!((c >= 97 && c <= 122) || (c >= 65 && c <= 90) || c == 32))
{
	printf("Input string contains unsupported characters\n");
	exit(1);
}

	input[i] = tolower(c);

}



	inputstr_s *inputwords = get_input_words_struct(input);

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
for(inputstr_s* inputwords_ptr = inputwords;; inputwords_ptr++) //count(*inputwords)
{

	if(inputwords_ptr->len == 0)
		break;


	for(int i = 0; i < 45; i++) // wlen + i / wlen - i
{

	pthread_mutex_lock(&lock);
//	printf("lev_found[%u].dist(%u) < %u pass? ",(LEV_SIZE-1),lev_found[LEV_SIZE-1].dist,i);
	if((unsigned int)i > lev_found[LEV_SIZE-1].dist)
{
//	printf("yes\n");
	pthread_mutex_unlock(&lock);
//	printf("going next\n");
	break;
}
//	printf("no\n");
	pthread_mutex_unlock(&lock);
	for(unsigned int s = 0; s < 2; s++)
{
	if(sign[s] == -1 && (unsigned int)i == inputwords_ptr->len)
		continue;

//	printf("signed offset:%d\n",i*sign[s]);
//	printf("len->%u\n",inputwords_ptr->len);
	unsigned int len_off = (inputwords_ptr->len) + (i*sign[s]);
//	printf("len_off->%u\n",len_off);
//	getchar();
	void* mem_start = ptr_off_size[len_off];
	void* mem_end = ptr_off_size[len_off+1];
//	printf("mem_start:%lu\n",(unsigned long)mem_start);
//	printf("mem_end  :%lu\n",(unsigned long)mem_end);

//	printf("listing::\n\n");
//	for(void* temp_ptr = mem_start; temp_ptr < mem_end; temp_ptr += (len_off + 1))
//		printf("%lu : %s\n",(long)temp_ptr, (char*)temp_ptr);
//	printf(":: end list\n");
//	getchar();

	memset(thread_args, 0, sizeof(lev_thread_args_s) * cpus);
	for(unsigned int j = 0; j < cpus; j++)
{
	thread_args[j].st = inputwords_ptr->st;
	if(j*(len_off+1) >= mem_end-mem_start) break; // break on cpu > count(wlen)
	thread_args[j].mem_start = mem_start + j*(len_off + 1);
	thread_args[j].mem_end = mem_end;
	thread_args[j].w2len = len_off;
	thread_args[j].step = cpus;
	thread_args[j].threadid = j;
	thread_args[j].lf_ptr = lev_found;
	thread_args[j].lk_ptr = &lock;
/*
	printf("\nthread_args[%u].st:%s\n",j,thread_args[j].st);
	printf("thread_args[%u].mem_start:%lu\n",j,(unsigned long)thread_args[j].mem_start);
	printf("thread_args[%u].mem_end:%lu\n",j,(unsigned long)thread_args[j].mem_end);
	printf("thread_args[%u].w2len:%u\n",j,thread_args[j].w2len);
	printf("thread_args[%u].step:%u\n",j,thread_args[j].step);
	printf("thread_args[%u].threadid:%u\n\n",j,thread_args[j].threadid);
	printf("going to create thread: %u?\n",j);
	getchar();*/
	pthread_create(&ptid[j], NULL, &lev_thread_f, &thread_args[j]);
//	printf("ok, waiting\n");
//	getchar();
} //end j

	for(unsigned int j = 0; j < cpus; j++)
		pthread_join(ptid[j],NULL);
	if(i == 0)
		break; // 0 * sign is same twice

} //end sign


} //end wlen

	printf("\nWord %u: %s\nMATCHES: :",wn++,(char*)inputwords_ptr->st);

	for(unsigned int i = 0; i < LEV_SIZE; i++)
{
	if(lev_found[i].ptr == 0) break;
	printf(" %s(%u)",(char*)lev_found[i].ptr, lev_found[i].dist);

} printf("\n");

	unsigned int answer = 'N';
//	printf("add (y/N)?:");
//	scanf("%c",&answer);

	inputwords_ptr->toadd = (answer == 'y' || answer == 'Y')? 1 : 0;

	for(unsigned int i = 0; i < LEV_SIZE; i++)
{
	lev_found[i].ptr = 0;
	lev_found[i].dist = 46;
}


} //end inputstr

//	printf("end\n");


	// add new words to file

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



/*
	for(unsigned long i = 0; i < iwlen;)
{
	if(inputwords_ptr->st[i] != *(char*)filebuf_ptr)
{
	while(*filebuf_ptr++ != '\n');
	i = 0;
	continue;
}
	i++;
	filebuf_ptr++;
}

	while(*--filebuf_ptr != '\n'); filebuf_ptr++;
*/



}
printf("bye\n");
exit(0);



} //end main

void calc_ptrs_offset_by_size(void **ptr_off_size, void *words_ptr, const unsigned int *lettercount) // order in mem by wlen
{
	ptr_off_size[0] = 0;
	for(unsigned int i = 1; i < 46; i++)
{
	words_ptr += lettercount[i-1] * i; // offset(1) always at 0
	ptr_off_size[i] = words_ptr;

}

}

inputstr_s* get_input_words_struct(char* input)
{
	char* input_p = input;
	unsigned int inputlen = 0;

	for(; *input_p != '\0'; input_p++) inputlen++;
	input_p = input;

	const char *input_prev_p = input_p;
	char* cursor = 0;
	inputstr_s *inputwords = malloc(sizeof(inputstr_s) * inputlen), *inputwords_p = inputwords;
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
//	const unsigned int threadid = args->threadid;
	const unsigned int w2len = args->w2len;
	const unsigned int wlen = strlen(st);
/*	printf("thread (%u) args:\n",threadid);
	printf("st:%s\n",st);
	printf("mem_start:%lu\n",(long)mem_start);
	printf("mem_end:%lu\n",(long)mem_end);
	printf("w2len:%u\n",w2len);
	printf("step:%u\n",step); */
//	return;
//	getchar();

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
//	printf("\n(%u)new word:%s %u\n%lu\n",threadid,(char*)mem_ptr,l,(unsigned long)mem_ptr);

	for(unsigned int i = 0; i < LEV_SIZE; i++)
{ //FOR L

//	printf("(%u) add at %u?: ",threadid,i);
	if(l < closest_dist_arr[i])
{ //IF

//	printf("(%u) yes\n",threadid);
//	printf("(%u)add at %u\n",threadid,i);
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
//	if(ptr_tmp1 == 0)
//		break;
}
//	printf("\nswapped\n");
/*	for(unsigned int k = 0; k < LEV_SIZE; k++)
{
	printf("(%u) [%u]: %s %u\n",threadid,k,(char*)closest_ptr_arr[k],closest_dist_arr[k]);

} getchar();*/
	break;

} //IF

else
{
//	printf("(%u) dont add at %u\n",threadid,i);
}

} //FOR L



//FOR

} //end for mem_ptr



//	printf("(%u) end thread\n\n",threadid);
//	for(unsigned int i = 0; i < LEV_SIZE; i++)
//		printf("(%u) [%u]:%s  dist:%u\n",threadid,i,(char*)closest_ptr_arr[i],closest_dist_arr[i]);
//	printf("\n\n\n");


//	printf("adding to lev_found[]\n");

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
/*
printf("(%u) added::\n",threadid);
pr(threadid, lf_ptr);
printf("\n");
getchar();*/
pthread_mutex_unlock(lock_p);
pthread_exit(NULL);

}


void pr(const int threadid, lev_found_s* l)
{
	for(unsigned int i = 0; i < LEV_SIZE; i++)
{
	printf("(%u) [%u]->str:%s  dist:%u\n",threadid,i,(char*)l[i].ptr,l[i].dist);

}
	printf("\n");

}

