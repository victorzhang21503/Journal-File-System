#include "string.h"

typedef enum{
	FALSE = 0,
	TRUE = 1
}BOOL_T;

typedef unsigned char uchar;
typedef unsigned short ushort;
uchar *JOURNAL_STORAGE[3][16] = {NULL};

/*----------------------------------------------------------*/
/*Variable Declarations*/
uchar *CELL_STORAGE;
/*----------------------------------------------------------*/
/* Macros for Journal storage data structure definition, which is descriped in design document.
*/

#define JOURNAL_OPT_TYPE_LEN 1
#define JOURNAL_OPT_NUM_LEN 1
#define JOURNAL_BM_LEN 1
#define JOURNAL_FILE_NAME_LEN 8
#define JOURNAL_FILE_METADATA_LEN 2
#define JOURNAL_FILE_DATA_LEN 128
#define JOURNAL_FILE_NUM 8


/*The file structure is defined as the name, metadata, data of the same file being viewed as a whole */

#define JOURNAL_FILE_STRUCT_SIZE  (JOURNAL_FILE_NAME_LEN + \
JOURNAL_FILE_METADATA_LEN + \
JOURNAL_FILE_DATA_LEN)
#define JOURNAL_SIZE (JOURNAL_OPT_TYPE_LEN + \
JOURNAL_OPT_NUM_LEN + \
JOURNAL_BM_LEN + \
JOURNAL_FILE_STRUCT_SIZE * \
JOURNAL_FILE_NUM)

/*The usage of OFFSET is to facilitate the location of each fields of journal storage*/

#define JOURNAL_OPT_TYPE_OFFSET 0
#define JOURNAL_OPT_NUM_OFFSET (JOURNAL_OPT_TYPE_OFFSET + JOURNAL_OPT_TYPE_LEN)
#define JOURNAL_BM_OFFSET (JOURNAL_OPT_NUM_OFFSET + JOURNAL_OPT_NUM_LEN)
#define JOURNAL_FILE_STRUCT_OFFSET (JOURNAL_BM_OFFSET + \
JOURNAL_BM_LEN)

#define JOURNAL_FILE_NAME_OFFSET 0
#define JOURNAL_FILE_METADATA_OFFSET (JOURNAL_FILE_NAME_OFFSET + \
JOURNAL_FILE_NAME_LEN)
#define JOURNAL_FILE_DATA_OFFSET (JOURNAL_FILE_METADATA_OFFSET + \
JOURNAL_FILE_METADATA_LEN)


/*----------------------------------------------------------*/
/* Macros for Cell storage data structure definition, which is descriped in design document.
*/

#define CELL_FILE_NAME_LEN 8
#define CELL_BM_LEN 16
#define CELL_FILE_METADATA_LEN 2
#define CELL_FILE_DATA_LEN 128
#define CELL_FILE_NUM 128
#define CELL_FILE_NAME_FIELD_SIZE (CELL_FILE_NAME_LEN * CELL_FILE_NUM)
#define CELL_FILE_STRUCT_SIZE  (CELL_FILE_METADATA_LEN + CELL_FILE_DATA_LEN)
#define CELL_SIZE  (CELL_FILE_NAME_FIELD_SIZE + \
CELL_BM_LEN + \
CELL_FILE_STRUCT_SIZE * \
CELL_FILE_NUM)

#define CELL_FILE_NAME_FIELD_OFFSET 0
#define CELL_BM_OFFSET (CELL_FILE_NAME_FIELD_OFFSET + \
CELL_FILE_NAME_FIELD_SIZE)
#define CELL_FILE_STRUCT_OFFSET (CELL_BM_OFFSET + CELL_BM_LEN)


#define CELL_FILE_METADATA_OFFSET 0
#define CELL_FILE_DATA_OFFSET (CELL_FILE_METADATA_OFFSET + \
CELL_FILE_METADATA_LEN)

/*This Function is used to bundle the bytes*/

static ushort MAKE_USHORT(uchar ch1, uchar ch2){
	return (ushort)((((ch2) << 8) & 0xFF00) | (ch1));
	}
//static uchar get_cell_file_data(uchar *file_name); 

/*----------------------------------------------------------*/
/*Invalid parameter macros*/
# define JOURNAL_INVALID_OPT_NUM 0xFF
# define CELL_INVALID_FILE_INODE 0xFF
# define CELL_INVALID_FILE_METADATA 0xFFFF 

/*----------------------------------------------------------*/
/*Function Declarations for journal storage data structure*/

/*This function returns the operation type in journal storage*/
/*
static uchar get_journal_opt_type(int ac_id){
	return *(JOURNAL_STORAGE[ac_id] + JOURNAL_OPT_TYPE_OFFSET);
}
*/
/*This function sets the operation type in journal storage*/

//static uchar set_journal_opt_type(uchar journal_opt_type){
	//*(JOURNAL_STORAGE + JOURNAL_OPT_TYPE_OFFSET) = journal_opt_type;
//}

/*This function returns the operation number in journal storage*/

static uchar get_journal_opt_num(int ac_id, int thread_num){
	return *(JOURNAL_STORAGE[thread_num][ac_id] + JOURNAL_OPT_NUM_OFFSET);
}

/*This function sets the operation number in journal storage*/

static void set_journal_opt_num(uchar journal_opt_num, int ac_id,int thread_num){
	*(JOURNAL_STORAGE[thread_num][ac_id] + JOURNAL_OPT_NUM_OFFSET)= journal_opt_num;
}

/*This function returns the bitmap in journal storage*/

static uchar get_journal_bm(int ac_id, int thread_num){
	return *(JOURNAL_STORAGE[thread_num][ac_id] + JOURNAL_BM_OFFSET);
}

/*This function finds the first free bit in journal bitmap that is set to 0*/

static uchar find_free_journal_opt_num(int ac_id, int thread_num){
	uchar journal_bm = get_journal_bm(ac_id, thread_num);
	uchar i = JOURNAL_INVALID_OPT_NUM;
	for(i = 0; i < 8; i++){
		if((journal_bm & (1 << i)) == 0){
			return i;
		}
	}
	return i;
}

/*This function check if the operation number is free*/
static BOOL_T check_free_journal_opt_num(uchar journal_opt_num, int ac_id, int thread_num){
	uchar journal_bm = get_journal_bm(ac_id, thread_num);
	if((journal_bm & (1 << journal_opt_num)) == 0) return TRUE;
	else return FALSE;
}


/*This function sets the journal bitmap */

static void set_journal_bm(uchar journal_bm, int ac_id, int thread_num){
	*(JOURNAL_STORAGE[thread_num][ac_id] + JOURNAL_BM_OFFSET) = journal_bm;	
}

/*This function sets the journal bitmap bit from free to being used */

static void set_journal_bm_bit(uchar i, int ac_id, int thread_num){
	uchar journal_bm =get_journal_bm(ac_id, thread_num);
	journal_bm |= (1 << i);
	set_journal_bm(journal_bm,ac_id, thread_num);
}

/*This function resets the journal bitmap bit from being used to free*/

static void reset_journal_bm_bit(uchar i, int ac_id, int thread_num){
	uchar journal_bm =get_journal_bm(ac_id, thread_num);
	journal_bm &= ~(1 << i);
	set_journal_bm(journal_bm,ac_id, thread_num);
}

/*This function gets the file structure in journal storage*/

static uchar *get_journal_file_struct(uchar journal_opt_num, int ac_id, int thread_num){
	return (JOURNAL_STORAGE[thread_num][ac_id] + JOURNAL_FILE_STRUCT_OFFSET + (journal_opt_num * JOURNAL_FILE_STRUCT_SIZE)); 
}

/* This function sets the file structure in journal storage */

static void set_journal_file_struct(uchar journal_opt_num, uchar *journal_file_struct, int ac_id, int thread_num){
	memcpy(JOURNAL_STORAGE[thread_num][ac_id] + JOURNAL_FILE_STRUCT_OFFSET + journal_opt_num * JOURNAL_FILE_STRUCT_SIZE,journal_file_struct ,JOURNAL_FILE_STRUCT_SIZE);
}

/* This function gets the file name in journal storage */
/*
static void get_journal_file_name(uchar *journal_file_struct, uchar *file_name){
	memcpy(file_name, journal_file_struct, JOURNAL_FILE_NAME_LEN);
}
*/

static uchar *get_journal_file_name(uchar *journal_file_struct){
	return (uchar *)(journal_file_struct);
}

/* This function sets the file name in journal storage */

static void set_journal_file_name(uchar *journal_file_struct, uchar *file_name){
	memcpy(journal_file_struct, file_name, JOURNAL_FILE_NAME_LEN);
}


/* This function gets the metadata of file in journal storage */
/*
static ushort get_journal_file_metadata(uchar *journal_file_struct){
	 //printf("inner function: journal_file_metadata is %x\n", (journal_file_struct+JOURNAL_FILE_METADATA_OFFSET)[0]);
	 //printf("inner function: journal_file_metadata is %x\n", (journal_file_struct+JOURNAL_FILE_METADATA_OFFSET)[1]);
	 ushort journal_file_metadata = MAKE_USHORT(
		(journal_file_struct + JOURNAL_FILE_METADATA_OFFSET)[0], (journal_file_struct + JOURNAL_FILE_METADATA_OFFSET)[1]);
	 return journal_file_metadata;
}
*/
/* This function sets the metadata of file in journal storage */
/*
static void set_journal_file_metadata(uchar *journal_file_struct, ushort journal_file_metadata){
	uchar metadata_byte_0 = journal_file_metadata & 0x00FF;
	uchar metadata_byte_1 = (journal_file_metadata & 0xFF00) >> 8;
	
	*(journal_file_struct + JOURNAL_FILE_METADATA_OFFSET) = metadata_byte_0;
	*(journal_file_struct + JOURNAL_FILE_METADATA_OFFSET + 1) = metadata_byte_1;  
}
*/
/* This function gets the data of file in journal storage */

static uchar *get_journal_file_data(uchar *journal_file_struct){
	uchar *storage_file_data = journal_file_struct + JOURNAL_FILE_DATA_OFFSET;
	return storage_file_data;	
}

/* This function sets the data of file in journal storage */

static void set_journal_file_data(uchar *journal_file_struct, uchar *file_data){
	uchar *storage_file_data = journal_file_struct + JOURNAL_FILE_DATA_OFFSET;
	//ushort journal_file_metadata = get_journal_file_metadata(journal_file_struct);
	memcpy(storage_file_data, file_data,  JOURNAL_FILE_DATA_LEN);	
}


/*----------------------------------------------------------*/
/*Function Declarations for journal storage data structure*/

/* This function returns the index of the file in the cell storage */

static uchar get_cell_file_index(uchar *file_name){
	uchar i = 0;
	uchar file_index = CELL_INVALID_FILE_INODE;
	for(i = 0; i < CELL_FILE_NUM; i++){
		uchar cell_file_name[CELL_FILE_NAME_LEN] = {0};
		uchar *file_inode_entry = CELL_STORAGE + (i * CELL_FILE_NAME_LEN);

		memcpy(cell_file_name, file_inode_entry,CELL_FILE_NAME_LEN);
		
		if(memcmp(file_name, cell_file_name, CELL_FILE_NAME_LEN) == 0){
			file_index = i;
			break;
		}
	}
	return file_index;	
}

/*This function sets the file name in the inode at the given inode and file name*/

static void set_cell_file_name(uchar file_index, uchar *file_name){
	uchar *cell_file_name = CELL_STORAGE + file_index * CELL_FILE_NAME_LEN;
	memcpy(cell_file_name, file_name ,CELL_FILE_NAME_LEN);
}

/*This function checks the bitmap to see if the inode is in used */

static BOOL_T check_free_cell_inode(uchar file_index){
	uchar *cell_bm = CELL_STORAGE + CELL_BM_OFFSET;
	uchar offset = file_index / 8;
	uchar bitset = file_index % 8;
	if((cell_bm[offset] & (1 << bitset)) == 0) return TRUE;
	else return FALSE;
}

/* This function returns the index of the first free inode entry */

static uchar find_cell_free_index(void){
	uchar file_index = CELL_INVALID_FILE_INODE;
	uchar i = 0;
	for(i = 0; i < CELL_FILE_NUM; i++){
		if(check_free_cell_inode(i) == TRUE){
			file_index = i;
			break;
		}
	}
	return file_index;
}


/*This function sets the file reverse usage status in the cell bitmap*/

static void set_cell_inode_reverse_usage(uchar file_index){
	uchar *cell_bm = CELL_STORAGE + CELL_BM_OFFSET;
	uchar offset = file_index / 8;
	uchar bitset = file_index % 8;
	cell_bm[offset] = (cell_bm[offset] ^ (1 << bitset));
}

/* This function gets the cell file struct given then index */

static uchar *get_cell_file_struct(uchar file_index){
	uchar *cell_file_struct = NULL;
	cell_file_struct = CELL_STORAGE + CELL_FILE_STRUCT_OFFSET
					+ file_index * CELL_FILE_STRUCT_SIZE;
	return  cell_file_struct;
}

/* This function returns the metadata of the file given the file name*/

static ushort get_cell_file_metadata(uchar *file_name){
	
	uchar *cell_file_struct = NULL;
	ushort cell_file_metadata = 0xFFFF;
	uchar file_index = 0xFF;
	
	file_index = get_cell_file_index(file_name);
	if(file_index == CELL_INVALID_FILE_INODE){
		printf("read succssful!\n");
		return cell_file_metadata;
	} 
	
	if(check_free_cell_inode(file_index) == TRUE) return cell_file_metadata;
	
	cell_file_struct = CELL_STORAGE + CELL_FILE_STRUCT_OFFSET+ file_index * CELL_FILE_STRUCT_SIZE;
	cell_file_metadata = MAKE_USHORT(*cell_file_struct, *(cell_file_struct+1));
	return cell_file_metadata;
}

/* This function sets the metadata of the file given the file name */
/*
static void set_cell_file_metadata(uchar *file_name, ushort file_metadata){
	uchar cell_file_metadata_0 = file_metadata & 0x00FF;
	uchar cell_file_metadata_1 = file_metadata & 0xFF00 >> 8;
	uchar *cell_file_struct = NULL;
	uchar file_index = CELL_INVALID_FILE_INODE;
	
	file_index = get_cell_file_index(file_name);
	if(file_index == CELL_INVALID_FILE_INODE) return;
	if(check_free_cell_inode(file_index) == TRUE) return;
	cell_file_struct = CELL_STORAGE + CELL_FILE_STRUCT_OFFSET
					+ file_index * CELL_FILE_STRUCT_SIZE;
	*cell_file_struct = cell_file_metadata_0;
	*(cell_file_struct + 1) = cell_file_metadata_1;
}
*/
/*This function get the data of the file given the file name*/

static uchar *get_cell_file_data(uchar *file_name){
	uchar *cell_file_struct = NULL;
	uchar *cell_file_data = NULL;
	uchar file_index = CELL_INVALID_FILE_INODE;

	file_index = get_cell_file_index(file_name);
	if(file_index == CELL_INVALID_FILE_INODE) return cell_file_data;
	
	if(check_free_cell_inode(file_index) == TRUE) return cell_file_data;

	cell_file_data = CELL_STORAGE + CELL_FILE_STRUCT_OFFSET
					+ file_index * CELL_FILE_STRUCT_SIZE + CELL_FILE_METADATA_LEN;					                          

	return cell_file_data;
}

/*This function set the file data given the file name*/

static void set_cell_file_data(uchar *file_name, uchar *file_data){
	uchar *cell_file_data = NULL;
	uchar file_index = CELL_INVALID_FILE_INODE;
	
	file_index = get_cell_file_index(file_name);
	//printf("inner function set_cell_file_data: file_index is %x\n", file_index);
	if(file_index == CELL_INVALID_FILE_INODE) return;
	/*if(check_free_cell_inode(file_index) == TRUE){
		printf("inner function, commit the file not allocated\n");
		return;
	}*/ 
	//printf("inner function set_cell_file_data: file_data is %s\n", file_data);
	cell_file_data = CELL_STORAGE + CELL_FILE_STRUCT_OFFSET
					+ file_index * CELL_FILE_STRUCT_SIZE + CELL_FILE_METADATA_LEN;
	memcpy(cell_file_data, file_data, CELL_FILE_DATA_LEN);
	
}


