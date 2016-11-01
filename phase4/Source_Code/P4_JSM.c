#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include "P4_JSM.h"


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


#define BEGIN 'b'
#define INSTALL 'i'
#define COMMIT 'c'
#define ABORT 'a'
#define UNDO 'u'
#define REDO 'r'
#define EMPTY "empty"


/*-------------------------------------------------------------------------------------------------------------
Variable declaration
*/
sem_t mutex;
sem_t pre_mutex;
sem_t ppp_mutex;
pthread_mutex_t lock;
FILE* fp;
int action_id = 1;
int mark_point = -1;
int mk = 0;

static BOOL_T check_former_pending(uchar* file_name, int ac_id){
	uchar* journal_file_struct = JOURNAL_STORAGE + JOURNAL_FILE_STRUCT_OFFSET;
	int ac;
	int i = 0;
	int flag = 0;
	for(i = 0; i < 1024; i++){
		if(memcmp((void *)(journal_file_struct + i*JOURNAL_FILE_STRUCT_SIZE), (void *)file_name, JOURNAL_FILE_NAME_LEN) == 0){
			ac = *(journal_file_struct + i*JOURNAL_FILE_STRUCT_SIZE + JOURNAL_FILE_METADATA_OFFSET) - 0x00;
			if(ac < ac_id) return TRUE;
		}
	}
	return FALSE;
}

/* WRITE_CURRENT_VALUE interface */
static void write_current_value(void *JSM_input, int thread_num, int ac_id){
	
	write_input input = *((write_input *)(JSM_input));
	int journal_opt_num = -1;
	uchar *journal_file_struct = NULL;
	
	
	journal_opt_num = find_free_journal_opt_num();
	
	if(journal_opt_num == -1){
		printf("Action %d: Fail!(No more journal storage space for Write)\n",ac_id);
		return;
	}
	
	set_journal_bm_bit(journal_opt_num);
	journal_file_struct = get_journal_file_struct(journal_opt_num);
	set_journal_file_name(journal_file_struct, input.file_name);
	
	//set_journal_file_metadata(journal_file_struct, input.journal_file_metadata);
	set_journal_file_data(journal_file_struct, input.file_data);
	set_journal_file_metadata(journal_file_struct, ac_id);
	printf("Thread %d, Action %d: Write successful! \n",thread_num,ac_id);
	return;
}

static void fault_write_current_value(void *JSM_input, int thread_num, int ac_id){
	
	write_input input = *((write_input *)(JSM_input));
	int journal_opt_num = -1;
	uchar *journal_file_struct = NULL;
	
	
	journal_opt_num = find_free_journal_opt_num();
	
	if(journal_opt_num == -1){
		printf("Action %d: Fail!(No more journal storage space for Write)\n",ac_id);
		return;
	}
	
	set_journal_bm_bit(journal_opt_num);
	journal_file_struct = get_journal_file_struct(journal_opt_num);
	set_journal_file_name(journal_file_struct, input.file_name);
	
	//set_journal_file_metadata(journal_file_struct, input.journal_file_metadata);
	set_journal_file_data(journal_file_struct, input.file_data);
	set_journal_file_metadata(journal_file_struct, 255);
	printf("Thread %d, Action %d: Write successful! \n",thread_num,ac_id);
	return;
}

static void write_to_cell(uchar *file_name, uchar *journal_file_struct,int ac_id){
	
	uchar *file_data = NULL;
	uchar file_index = 0;
	uchar *data = NULL;
	data = (uchar *)calloc(1, JOURNAL_FILE_DATA_LEN);
	file_data = get_journal_file_data(journal_file_struct);
	file_index = get_cell_file_index(file_name);
	if(file_index == CELL_INVALID_FILE_INODE){
		file_index = find_cell_free_index();
		set_cell_file_name(file_index, file_name);
		set_cell_inode_reverse_usage(file_index);
		data = EMPTY;
	}
	else{
		data = get_cell_file_data(file_name);
	}
	
	fprintf(fp,"%c\n%d\n%c\n%s\n%s\n", INSTALL, ac_id, UNDO, file_name, data);	
	fclose(fp);
	fp = fopen("log.txt", "a+");
	set_cell_file_data(file_name, file_data);
	fprintf(fp,"%c\n%d\n%c\n%s\n%s\n", INSTALL, ac_id, REDO, file_name, file_data);
	fclose(fp);
	fp = fopen("log.txt", "a+");
	return;
}

/* COMMIT interface */

static void commit_operation (int ac_id, int thread_num){
	int f = 0;
	int m = 0;
	int flag = 0;
	int journal_opt_num = 0;
	uchar *journal_file_struct = NULL;
	//uchar *check_file_name = (uchar *) calloc(1, JOURNAL_FILE_NAME_LEN);
	
	for(journal_opt_num = 0; journal_opt_num < 1024; journal_opt_num++){
		journal_file_struct = get_journal_file_struct(journal_opt_num);
		//check_file_name = get_journal_file_name(journal_file_struct);
		//if(memcmp((void *) check_file_name, (void *) file_name, sizeof(file_name)) == 0){
			if(ac_id == get_journal_file_metadata(journal_file_struct) - 0x00){
				uchar * file_name = NULL;
				file_name = (uchar *)calloc(1, JOURNAL_FILE_NAME_LEN);
				memcpy((void *)file_name,(void *)(journal_file_struct), JOURNAL_FILE_NAME_LEN);
				while(check_former_pending(file_name,ac_id)){
					if(m == 0){
						sem_post(&mutex); m = m +1;	
					}
					f = 1;		
					sleep(2);
				}
				if(f == 1) sem_wait(&mutex);
				reset_journal_bm_bit(journal_opt_num);
				write_to_cell(file_name, journal_file_struct,ac_id);
				memset(journal_file_struct, 0, JOURNAL_FILE_STRUCT_SIZE);
			}
		//}
	}
	printf("Thread %d: Action %d is committed successfully!\n", thread_num, ac_id);
	return;
}

static void abort_operation (int ac_id, int thread_num){
	int flag = 0;
	int journal_opt_num = 0;
	uchar *journal_file_struct = NULL;
	uchar *check_file_name = (uchar *)calloc(1, JOURNAL_FILE_NAME_LEN);
	for(journal_opt_num = 0; journal_opt_num < 1024; journal_opt_num++){
		journal_file_struct = get_journal_file_struct(journal_opt_num);
		check_file_name = get_journal_file_name(journal_file_struct);
		//if(memcmp((void *) check_file_name, (void *) file_name, sizeof(file_name) == 0)){
			if(ac_id == get_journal_file_metadata(journal_file_struct) - 0xFF){
				reset_journal_bm_bit(journal_opt_num);	
				memset(journal_file_struct, 0, JOURNAL_FILE_STRUCT_SIZE);
				printf("Thread %d: Action %d is aborted successfully!\n", thread_num, ac_id);
				break;
			}
		//}
	}
	
	return;
}


void func(void *args){
	int flag = 1;
	int thread_num = *(int*)args;
	CELL_STORAGE = (uchar *) calloc(1, CELL_SIZE);
	unsigned int input = 0;
	
	int in;
	while(1){
		sem_wait(&mutex);
		int ac_id = action_id++;
		printf("Thread %d: Do you want to initialize a new action? 1/0\n", thread_num);
		
		scanf("%d", &in);
		
		if(in == 1){
			int i = 0;
			printf("New Action is created!\n");
			printf("Thread %d: The action_id is %d\n", thread_num, ac_id);
			fprintf(fp,"%c\n%d\n%c\n%s\n%s\n", BEGIN, ac_id, 'e', EMPTY, EMPTY);
			fclose(fp);
			fp = fopen("log.txt", "a+");
		}
		sem_post(&mutex);
			sleep(2);
		
		
		
		
		if(in == 1){
			flag = 1;
			while(flag == 1){
				sem_wait(&mutex);
				printf("Thread %d, action %d: Choose one of the following operation\n", thread_num, ac_id);
				printf("1.READ_CURRENT_VALUE   2.WRITE_CURRENT_VALUE   3.ANOUNCE_MARK_POINT\n4.COMMIT   5.ABORT   6.FAULT_WRITE\n7.SYSTEM_CRASH   8.SYSTEM_RECOVERY   9.CLEAR_LOG\n10.EXIT\n");
				scanf("%d", &input);
				sem_post(&mutex);
			sleep(2);
				
				switch(input){
					case 1:{
						sem_wait(&mutex);
						uchar file_name[JOURNAL_FILE_NAME_LEN] ={0};
						uchar *file_data = NULL;
						printf("Thread %d Action %d: Enter the file name to be read: ", thread_num,ac_id);
						scanf("%s", file_name);
						printf("\n");
						uchar *cell_file_data = NULL;
						file_data = get_cell_file_data(file_name);
						if(file_data == NULL){
							printf("Thread %d Action %d: Fault: The File is not exist!\n", thread_num, ac_id);
							sem_post(&mutex);
							sleep(2);
							break;
						} 
						else printf("Thread %d Action %d: file_data is: %s\n",thread_num,ac_id,file_data);
						sem_post(&mutex);
	sleep(2);
				}
					break;
					case 2:{
						sem_wait(&mutex);
						if(JOURNAL_STORAGE == NULL){
							printf("Thread %d Action %d: Fault! This Action is not initialized!\n", thread_num, ac_id);
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
						sem_wait(&ppp_mutex);
						scanf("%s", file_data);
						sem_post(&ppp_mutex);
						printf("\n");
						JSM_input->file_data = (uchar *) calloc(1, JOURNAL_FILE_DATA_LEN);
						JSM_input->journal_file_metadata = strlen(file_data) + 1;
						memcpy(JSM_input->file_data, file_data, JOURNAL_FILE_DATA_LEN);
						write_current_value(JSM_input, thread_num, ac_id);
						sem_post(&mutex);sleep(2);

					}
					break;
					case 3:{
						while(!(mk == (ac_id -1))){
							sleep(2);
						}
						mk++;
					}
					break;
					case 4:{
						while(!(mk >= (ac_id -1))){
							sleep(2);
						}
						
						
						sem_wait(&mutex);
						commit_operation(ac_id, thread_num);
						sem_post(&mutex);sleep(2);

					}
					break;
					case 5:{
						sem_wait(&mutex);
						abort_operation (ac_id, thread_num);
						sem_post(&mutex);sleep(2);

					}
					break;
					
					case 6:{
						sem_wait(&mutex);
						if(JOURNAL_STORAGE == NULL){
							printf("Thread %d Action %d: Fault! This Action is not initialized!\n",thread_num,ac_id);
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
						sem_wait(&ppp_mutex);
						scanf("%s", file_data);
						sem_post(&ppp_mutex);
						printf("\n");
						JSM_input->file_data = (uchar *) calloc(1, JOURNAL_FILE_DATA_LEN);
						JSM_input->journal_file_metadata = strlen(file_data) + 1;
						memcpy(JSM_input->file_data, file_data, JOURNAL_FILE_DATA_LEN);
						fault_write_current_value(JSM_input, thread_num, ac_id);
						sem_post(&mutex);sleep(2);

					}
					break;
					case 7:{
						sem_wait(&mutex);
						memset((void *)CELL_STORAGE, 0, CELL_SIZE);
						sem_post(&mutex);sleep(2);

					}
					break;
					case 8:{
						sem_wait(&mutex);
						rewind(fp);
						int i = 0; 
						while(feof(fp) == 0){
							LOG_FILE_NAME[i] = (uchar *)calloc(1, JOURNAL_FILE_NAME_LEN);
							LOG_DATA[i] = (uchar *)calloc(1, JOURNAL_FILE_DATA_LEN);
							fscanf(fp,"%c\n%d\n%c\n%s\n%s\n", &LOG_RECORD_STATUS[i], &LOG_AC_ID[i], &LOG_RUDO[i], LOG_FILE_NAME[i], LOG_DATA[i]);
							i = i+1;
						}
							
								int j = 0;
								while(j < i){
								
									if(LOG_RUDO[j] == 'r'){
										uchar file_index = find_cell_free_index();
										set_cell_file_name(file_index, LOG_FILE_NAME[j]);
										set_cell_inode_reverse_usage(file_index);
										set_cell_file_data(LOG_FILE_NAME[j], LOG_DATA[j]);
									}
									j++;
								}
						sem_post(&mutex);sleep(2);

					}
					break;
					case 9:{
						sem_wait(&mutex);
						fclose(fp);
						fp = fopen("log.txt", "w+");
						fclose(fp);
						fp = fopen("log.txt", "a+");
						sem_post(&mutex);sleep(2);

					}
					case 10:{
						sem_wait(&mutex);
						fprintf(fp,"%c\n%d\n%c\n%s\n%s\n", COMMIT, ac_id, 'e', EMPTY, EMPTY);
						fclose(fp);
						fp = fopen("log.txt", "a+");
						printf("Thread %d Action %d exits now!\n",thread_num,ac_id);
						flag = 0;
						sem_post(&mutex);sleep(2);

					}
					break;
					default:{
						sem_wait(&mutex);
						sem_post(&mutex);
					}
					break;
				}
		
			}			
		}
		
		else{
			sleep(2);
		}
	}
	return;
}


int main(){
	JOURNAL_STORAGE = (uchar *) calloc(1, JOURNAL_SIZE);
	fp = fopen("log.txt", "a+");
	pthread_t thread1;
	pthread_t thread2;
	int t0 = 0;
	int t1 = 1;
	int t2 = 2;
	sem_init(&mutex, 0, 1);
	sem_init(&pre_mutex, 0, 1);
	sem_init(&ppp_mutex, 0, 1);
	
	pthread_create(&thread1, NULL,(void *)&func, &t1);
	pthread_create(&thread2, NULL,(void *)&func, &t2);
	func(&t0);
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	sem_destroy(&mutex);
	sem_destroy(&pre_mutex);
	sem_destroy(&ppp_mutex);
	fclose(fp);
	return 0;
}
