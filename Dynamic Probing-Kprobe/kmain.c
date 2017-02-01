#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "ht530_ioctl.h"
#include <stdint.h>
#include <stdint.h>
#include <semaphore.h>

typedef struct {
	uint64_t TSC;
	unsigned long  ADDR;
	pid_t PID;
	int G_VALUE;
	int L_VALUE;
}data, *Pdata;

typedef struct {
	unsigned long BUF_ADDR;
	unsigned long GLOBAL_ADDR;
	unsigned long LOCAL_OFF;
}Saddress, *PSaddress;
	
#define PRIO 10
#define NUM_THREADS  4 
#define SIZE_OF_BUF  10

int COUNT_WRITE_HT= 0;
int set_retval;
int KEY = 20 ;
int DATA = 500;

sem_t full;
pthread_mutex_t write_mutex=PTHREAD_MUTEX_INITIALIZER;

int ht_530_read_key(int fd, int key)     //call to give input key to the read function
{
	if((set_retval = ioctl(fd,HT_530_READ_KEY,(unsigned long)&key))!=0){
	printf("ioctl error %d\n", set_retval);}
	
	if(set_retval)
		printf("ht530_dev_error: unable to perform ht_530_read_key \n");
	
	return 0;
}

int dump_ioctl(int fd, int n, ht_obj_t object_array[8])     
{
	int i;
	dump_arg Sdump_arg;
	Sdump_arg.in.in_n = n;
	for(i=0; i<8;i++)
	{Sdump_arg.out.out_object_array[i] = object_array[i];}
	set_retval = ioctl(fd,DUMP_IOCTL,(unsigned long)&Sdump_arg);
	if(set_retval)
	printf("ht530_dev_error: unable to perform dump_ioctl \n");
	
	return Sdump_arg.RetVal;
}

void *test_func(void *fd)
{
	int res,set, i, buf;
	int fptr = *(int*)fd;
	Pht_obj_t new_obj;
	new_obj = (Pht_obj_t) malloc(sizeof(ht_obj_t));
	memset(new_obj, 0, sizeof(ht_obj_t));
	for(i=0; i<50; i++ )                       /*writing totally 200 value*/
	{
	new_obj->key = KEY++;
	new_obj->data = DATA++;
	pthread_mutex_lock(&write_mutex);
	COUNT_WRITE_HT++;
	res = write(fptr, new_obj, sizeof(ht_obj_t));
	if(res<0)
	printf("write operation failed");
	pthread_mutex_unlock(&write_mutex);
		
	if((COUNT_WRITE_HT% SIZE_OF_BUF)==0)         /*release the semaphore after 10 operations*/
	{
		sem_post(&full);
	}
	sleep(1);
	set = ht_530_read_key( fptr,  new_obj->key);     //call to give input key to the read function
	if ((set =read( fptr, &buf , sizeof(int)))==-1)    
	printf("no data is read\n");
	else
	printf("\nht530_drv: read data is %d\n ", buf);
	

	
	}
	
	pthread_exit(0);
}


void *read_k_func(void *fd1)
{
	int res, i,j; 
	int fptr_k = *(int*)fd1;
	int num = (200/SIZE_OF_BUF);		/*circular buffer values are printed after every 10 write operations*/
	for(j=0; j< num; j++)
	{
		sem_wait(&full);		/*wait till 10 objects are put in the circular buffer*/
	for(i=0; i<SIZE_OF_BUF;i++)
	{
		Pdata ptr = (Pdata)malloc(sizeof(data));
		if(ptr==NULL)
		printf("Bad malloc\n");
	
		Pdata read_buf = (Pdata)malloc(sizeof(data));
		if(read_buf == NULL)
		printf("Bad malloc\n");
	
		if ((res =read( fptr_k, read_buf , sizeof(int)))==-1)     //call to read from the ring buffer to the read function
		printf("Mprobe_drv: no data is read\n");
		else
		{	
		memcpy(ptr, read_buf, sizeof(data));
		printf("\nreading from the ring buffer\n The TSC Value = %llu\n The ADDR Value = %p\n The PID Value = %d\n The G_VALUE at the address passed = %d\n The L_VALUE at the offset passed = %d\n", ptr -> TSC, (void*)ptr->ADDR, ptr->PID, ptr->G_VALUE, ptr->L_VALUE);
		}	
	}

	}	
	
	pthread_exit(0);
}

int main(int argc, char **argv)
{
	int i = 0;
	int fd_k, fd_ht,res;
	ht_obj_t object_array[8];
	Saddress Spass_addr;

	
	
	fd_k = open("/dev/Mprobe", O_RDWR);
	if (fd_k < 0 ){
		printf("Can not open Mprobe device file.\n");		
		return 0;
	}
	else {
		printf("Mprobe device is open\n");
	}
        fd_ht = open("/dev/ht530", O_RDWR);
        if (fd_ht < 0 ){
                printf("Can not open ht530 device file.\n");
                return 0;
        }
        else {
                printf("ht530 device is open\n");
        }
	
	pthread_t tid_k;
	pthread_attr_t attr_k;
	pthread_attr_init(&attr_k);
	
	printf("Give an address where the probe has to hit\n");
	scanf("%lx", &(Spass_addr.BUF_ADDR));

		
	printf("Give an address from where the global data value has to be read\n");
	scanf("%lx",&(Spass_addr.GLOBAL_ADDR) );

	printf("Give an offset for the local variable whose value has to be read\n");
	scanf("%lx", &(Spass_addr.LOCAL_OFF));
	
	res = write(fd_k, &Spass_addr , sizeof(Saddress));
	if(res<0)
		printf("write in Mprobe_drv operation failed");
	
	
	pthread_t tid[NUM_THREADS];
	pthread_attr_t attr;
	struct sched_param param[NUM_THREADS];
	pthread_attr_init(&attr);
	sem_init(&full, 0, 0);
	
	for(i=0; i<NUM_THREADS; i++ )
	{
		param[i].sched_priority = PRIO + (i*2);
		pthread_attr_setschedparam (&attr, &param[i]);
		pthread_create(&tid[i],&attr,test_func,(void*)&fd_ht);
	}
	
	pthread_create(&tid_k,&attr_k,read_k_func,(void*)&fd_k);
	
	sleep(1);
	for(i=0;i<NUM_THREADS;i++)
		pthread_join(tid[i],NULL);
	
	for(i=0; i<128; i++) 			//dump all  128 values maximum upto 8 objects
	res = dump_ioctl( fd_ht, i, object_array);  
	
	pthread_join(tid_k,NULL);
	
	close(fd_ht);
	close(fd_k);	
exit(0);
}