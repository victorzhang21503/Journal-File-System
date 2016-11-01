#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include "P2_JSM.h"


/*----------------------------------------------------------*/
/*Structure used for operation input*/

typedef struct{
	uchar *file_name;
} read_input;

typedef struct{
	uchar *file_name;
	uchar *file_data;
	ushort journal_file_metadata;
} write_input;


#define ALLOCATE 1
#define DEALLOCATE 2
#define READ 3
#define WRITE 4
#define COMMIT 5
#define ABORT 6


/*-------------------------------------------------------------------------------------------------------------
Variable declaration
*/
//sem_t mutex;
//sem_t pre_mutex;
//pthread_mutex_t lock;


/* WRITE_CURRENT_VALUE interface */
static void write_current_value(void *JSM_input, int ac_id, int thread_num){
	
	write_input input = *((write_input *)(JSM_input));
	uchar journal_opt_num = 0xFF;
	uchar *journal_file_struct = NULL;
	
	
	journal_opt_num = find_free_journal_opt_num(ac_id, thread_num);
	
	if(journal_opt_num == JOURNAL_INVALID_OPT_NUM){
		printf("Action %d: Fail!(No more journal storage space for Write)\n",ac_id);
		return;
	}
	
	set_journal_bm_bit(journal_opt_num,ac_id,thread_num);
	//set_journal_opt_num(journal_opt_num,ac_id);
	//set_journal_opt_type(WRITE);
	
	
	journal_file_struct = get_journal_file_struct(journal_opt_num,ac_id, thread_num);
	set_journal_file_name(journal_file_struct, input.file_name);
	
	//set_journal_file_metadata(journal_file_struct, input.journal_file_metadata);
	set_journal_file_data(journal_file_struct, input.file_data);
	
	printf("Thread %d, Action %d: Write successful! \n",thread_num,ac_id);
	return;
}


static void write_to_cell(uchar *file_name, uchar *journal_file_struct,int ac_id){
	
	uchar *file_data = NULL;
	//ushort journal_file_metadata = 0;
	uchar file_index = 0;
	//journal_file_metadata = get_journal_file_metadata(journal_file_struct);
	
	file_data = get_journal_file_data(journal_file_struct);
	//printf("operation write_to_cell: %s\n", file_data);
	file_index = get_cell_file_index(file_name);
	if(file_index == CELL_INVALID_FILE_INODE){
		file_index = find_cell_free_index();
		set_cell_file_name(file_index, file_name);
		set_cell_inode_reverse_usage(file_index);
	}
	uchar *cell_file_struct = get_cell_file_struct(file_index);
	//printf("operation write_to_cell: the file name is %s\n", file_name);
	memset(cell_file_struct, 0, CELL_FILE_STRUCT_SIZE);
	set_cell_file_data(file_name, file_data);
	
	return;
}

/* COMMIT interface */
/*
static void commit_operation (int ac_id, int thread_num){
	
	uchar journal_opt_num = 0xFF;
	uchar *journal_file_struct = NULL;
	uchar *file_name = (uchar *) calloc(1, JOURNAL_FILE_NAME_LEN);
	for(journal_opt_num = 0; journal_opt_num < 16; journal_opt_num++){
		if(check_free_journal_opt_num(journal_opt_num,ac_id, thread_num) == TRUE) break;
		journal_file_struct = get_journal_file_struct(journal_opt_num, ac_id, thread_num);
		file_name = get_journal_file_name(journal_file_struct);
		write_to_cell(file_name, journal_file_struct,ac_id);
	}
	JOURNAL_STORAGE[thread_num][ac_id] = NULL;
	printf("Thread %d: Action %d is committed successfully!\n", thread_num, ac_id);
	return;
}
*/
static void commit_operation (int ac_id, int thread_num, uchar* file_name){
	int flag = 0;
	uchar journal_opt_num = 0xFF;
	uchar *journal_file_struct = NULL;
	uchar *check_file_name = (uchar *) calloc(1, JOURNAL_FILE_NAME_LEN);
	for(journal_opt_num = 0; journal_opt_num < 16; journal_opt_num++){
		journal_file_struct = get_journal_file_struct(journal_opt_num, ac_id, thread_num);
		check_file_name = get_journal_file_name(journal_file_struct);
		if(memcmp((void *) check_file_name, (void *) file_name, sizeof(file_name)) == 0){
			flag = 1;
			break;
		}
	}
	if(flag == 1){
		reset_journal_bm_bit(journal_opt_num,ac_id, thread_num);	
		write_to_cell(file_name, journal_file_struct,ac_id);
		memset(journal_file_struct, 0, JOURNAL_FILE_STRUCT_SIZE);
		printf("Thread %d: Action %d is committed successfully!\n", thread_num, ac_id);
	}
	else{
		printf("Thread %d, Action %d: Error! The file could not found to be commited!\n", thread_num, ac_id);
	}
	
	return;
}
static void abort_operation (int ac_id, int thread_num, uchar* file_name){
	int flag = 0;
	uchar journal_opt_num = 0xFF;
	uchar *journal_file_struct = NULL;
	uchar *check_file_name = (uchar *) calloc(1, JOURNAL_FILE_NAME_LEN);
	for(journal_opt_num = 0; journal_opt_num < 8; journal_opt_num++){
		journal_file_struct = get_journal_file_struct(journal_opt_num, ac_id, thread_num);
		check_file_name = get_journal_file_name(journal_file_struct);
		if(memcmp(check_file_name, file_name, JOURNAL_FILE_NAME_LEN) == 0){
			flag = 1;
			break;
		}
	}
	if(flag == 1){
		reset_journal_bm_bit(journal_opt_num,ac_id, thread_num);	
		memset(journal_file_struct, 0, JOURNAL_FILE_STRUCT_SIZE);
		printf("Thread %d: Action %d is aborted successfully!\n", thread_num, ac_id);
	}
	else{
		printf("Thread %d, Action %d: Error! The file could not found to be aborted!\n", thread_num, ac_id);
	}
	
	return;
}

void func(void *args){
	int flag = 1;
	int thread_num = *(int*)args;
	CELL_STORAGE = (uchar *) calloc(1, CELL_SIZE);
	unsigned int input = 0;
	int ac_id = -1;
	int in;
	while(1){
		//sem_wait(&mutex);
		printf("Thread %d: Do you want to initialize a new action? 1/0\n", thread_num);
		scanf("%d", &in);
		if(in == 1){
			ac_id = (ac_id+1)%16;
			int i = 0;
			JOURNAL_STORAGE[thread_num][ac_id] = (uchar *) calloc(1, JOURNAL_SIZE);
			printf("New Action is created!\n");
			printf("Thread %d: The action_id is %d\n", thread_num, ac_id);
		}
		//sem_post(&mutex);
		
		
		
		if(in == 1){
			flag = 1;
			//sleep(1);
			//sem_wait(&mutex);
			while(flag == 1){
				printf("Thread %d, action %d: Choose one of the following operation\n", thread_num, ac_id);
				printf("1.READ_CURRENT_VALUE   2.WRITE_CURRENT_VALUE   3.COMMIT\n4.ABORT   5.EXIT\n");
				scanf("%d", &input);
				switch(input){
					case 1:{
						uchar file_name[JOURNAL_FILE_NAME_LEN] ={0};
						uchar *file_data = NULL;
						printf("Thread %d: Enter the file name to be read: ", thread_num);
						scanf("%s", file_name);
						printf("\n");
						uchar *cell_file_data = NULL;
						file_data = get_cell_file_data(file_name);
						if(file_data == NULL){
							printf("Thread %d: Fault: The File is not exist!\n", thread_num);
							break;
						} 
						else printf("Thread %d Action %d: file_data is: %s\n",thread_num,ac_id,file_data);
					}
					break;
					case 2:{
						if(JOURNAL_STORAGE[thread_num][ac_id] == NULL){
							printf("Fault: This Action is not initialized!\n");
							break;
						} 
						uchar file_name[JOURNAL_FILE_NAME_LEN] ={0};
						uchar file_data[JOURNAL_FILE_DATA_LEN] = {0};
						write_input *JSM_input = NULL;
						JSM_input = (write_input *) calloc(1, sizeof(write_input));
						printf("Thread %d Action %d: Enter the file name to be wrote: ",thread_num,ac_id);
						scanf("%s", file_name);
						printf("\n");
						JSM_input->file_name = (uchar *) calloc(1, JOURNAL_FILE_NAME_LEN);
						memcpy(JSM_input->file_name, file_name, JOURNAL_FILE_NAME_LEN);
						printf("Thread %d Action %d: Enter the data: ",thread_num,ac_id);
						scanf("%s", file_data);
						printf("\n");
						JSM_input->file_data = (uchar *) calloc(1, JOURNAL_FILE_DATA_LEN);
						JSM_input->journal_file_metadata = strlen(file_data) + 1;
						memcpy(JSM_input->file_data, file_data, JOURNAL_FILE_DATA_LEN);
						write_current_value(JSM_input, ac_id, thread_num);
					}
					break;
					case 3:{
						uchar file_name[JOURNAL_FILE_NAME_LEN] ={0};
						printf("Thread %d: Enter the file name to be committed: ", thread_num);
						scanf("%s", file_name);
						printf("\n");
						commit_operation(ac_id, thread_num, file_name);
						//JOURNAL_STORAGE[thread_num][ac_id] = NULL;
						//flag = 0;
					}
					break;
					case 4:{
						uchar file_name[JOURNAL_FILE_NAME_LEN] ={0};
						printf("Thread %d: Enter the file name to be aborted: ", thread_num);
						scanf("%s", file_name);
						printf("\n");
						abort_operation (ac_id, thread_num, file_name);
						//flag = 0;
					}
					break;
					case 5:{
						printf("Thread %d Action %d exits now!\n",thread_num,ac_id);
						JOURNAL_STORAGE[thread_num][ac_id] = NULL;
						flag = 0;
					}
					break;
					default:{
						printf("Choose an option from 1 to 6\n");
					}
					break;
				}
		
			}			
		}
		
		//else{
		//	sleep(1);
		//}
		//sem_post(&mutex);
	}
	return;
}


int main(){
	//pthread_t thread1;
	//pthread_t thread2;
	int t0 = 0;
	int t1 = 1;
	int t2 = 2;
	//sem_init(&mutex, 0, 1);
	//sem_init(&pre_mutex, 0, 1);
	
	//pthread_create(&thread1, NULL,(void *)&func, &t1);
	//pthread_create(&thread2, NULL,(void *)&func, &t2);
	func(&t0);
	//pthread_join(thread1, NULL);
	//pthread_join(thread2, NULL);
	//sem_destroy(&mutex);
	//sem_destroy(&pre_mutex);
	return 0;
}
