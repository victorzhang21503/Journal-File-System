#include "string.h"
#include <stdio.h>

typedef enum{
	FALSE = 0,
	TRUE = 1
}BOOL_T;

typedef unsigned char uchar;
typedef unsigned short ushort;
uchar LOG_RECORD_STATUS[1024];
int LOG_AC_ID[1024];
uchar LOG_RUDO[1024];
uchar *LOG_DATA[1024];
uchar *LOG_FILE_NAME[1024];


/*----------------------------------------------------------*/
/*Variable Declarations*/
uchar *CELL_STORAGE;
uchar *JOURNAL_STORAGE;

/*----------------------------------------------------------*/
/* Macros for Journal storage data structure definition, which is descriped in design document.
*/


#define JOURNAL_BM_LEN 128
#define JOURNAL_FILE_NAME_LEN 8
#define JOURNAL_FILE_METADATA_LEN 1
#define JOURNAL_FILE_DATA_LEN 128
#define JOURNAL_FILE_NUM 1024


/*The file structure is defined as the name, metadata, data of the same file being viewed as a whole */

#define JOURNAL_FILE_STRUCT_SIZE  (JOURNAL_FILE_NAME_LEN + \
JOURNAL_FILE_METADATA_LEN + \
JOURNAL_FILE_DATA_LEN)
#define JOURNAL_SIZE (JOURNAL_BM_LEN + \
JOURNAL_FILE_STRUCT_SIZE * \
JOURNAL_FILE_NUM)

/*The usage of OFFSET is to facilitate the location of each fields of journal storage*/

#define JOURNAL_BM_OFFSET 0
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
#define CELL_FILE_DATA_LEN 128
#define CELL_FILE_NUM 128
#define CELL_FILE_NAME_FIELD_SIZE (CELL_FILE_NAME_LEN * CELL_FILE_NUM)
#define CELL_SIZE  (CELL_FILE_NAME_FIELD_SIZE + \
CELL_BM_LEN + \
CELL_FILE_DATA_LEN * \
CELL_FILE_NUM)

#define CELL_FILE_NAME_FIELD_OFFSET 0
#define CELL_BM_OFFSET (CELL_FILE_NAME_FIELD_OFFSET + \
CELL_FILE_NAME_FIELD_SIZE)
#define CELL_FILE_STRUCT_OFFSET (CELL_BM_OFFSET + CELL_BM_LEN)



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

/*This function returns the bitmap in journal storage*/

static uchar* get_journal_bm(){
	return (JOURNAL_STORAGE + JOURNAL_BM_OFFSET);
}

/*This function finds the first free bit in journal bitmap that is set to 0*/

static uchar find_free_journal_opt_num(){
	uchar *journal_bm = get_journal_bm();
	int i = -1;
	
	for(i = 0; i < 1024; i++){
		uchar offset = i / 8;
		uchar bitset = i % 8;
		if((journal_bm[offset] & (1 << bitset)) == 0){
			return i;
		}
	}
	return i;
}

/*This function check if the operation number is free*/
static BOOL_T check_free_journal_opt_num(int journal_opt_num){
	uchar *journal_bm = get_journal_bm();
	int offset = journal_opt_num / 8;
	int bitset = journal_opt_num % 8;
	if((journal_bm[offset] & (1 << bitset)) == 0) return TRUE;
	else return FALSE;
}


/*This function sets the journal bitmap bit from free to being used */

static void set_journal_bm_bit(int i){
	int offset = i / 8;
	int bitset = i % 8;
	uchar* journal_bm =get_journal_bm();
	journal_bm[offset] |= (1 << bitset);
}

/*This function resets the journal bitmap bit from being used to free*/

static void reset_journal_bm_bit(int i){
	uchar* journal_bm =get_journal_bm();
	int offset = i / 8;
	int bitset = i % 8;
	journal_bm[offset] &= ~(1 << bitset);
}

/*This function gets the file structure in journal storage*/

static uchar *get_journal_file_struct(int journal_opt_num){
	return (JOURNAL_STORAGE + JOURNAL_FILE_STRUCT_OFFSET + (journal_opt_num * JOURNAL_FILE_STRUCT_SIZE)); 
}

/* This function sets the file structure in journal storage */

static void set_journal_file_struct(int journal_opt_num, uchar *journal_file_struct){
	memcpy(JOURNAL_STORAGE + JOURNAL_FILE_STRUCT_OFFSET + journal_opt_num * JOURNAL_FILE_STRUCT_SIZE, journal_file_struct, JOURNAL_FILE_STRUCT_SIZE);
}

/* This function gets the file name in journal storage */


static uchar *get_journal_file_name(uchar *journal_file_struct){
	return (uchar *)(journal_file_struct);
}

/* This function sets the file name in journal storage */

static void set_journal_file_name(uchar *journal_file_struct, uchar *file_name){
	memcpy(journal_file_struct, file_name, JOURNAL_FILE_NAME_LEN);
}

static uchar get_journal_file_metadata(uchar *journal_file_struct){
	return *(journal_file_struct + JOURNAL_FILE_METADATA_OFFSET);
}

static void set_journal_file_metadata(uchar *journal_file_struct, int ac_id){
	*(journal_file_struct + JOURNAL_FILE_METADATA_OFFSET) = 0x00 + ac_id;
}

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
					+ file_index * CELL_FILE_DATA_LEN;
	return  cell_file_struct;
}

/*This function get the data of the file given the file name*/

static uchar *get_cell_file_data(uchar *file_name){
	uchar *cell_file_struct = NULL;
	uchar *cell_file_data = NULL;
	uchar file_index = CELL_INVALID_FILE_INODE;

	file_index = get_cell_file_index(file_name);
	if(file_index == CELL_INVALID_FILE_INODE) return cell_file_data;
	
	if(check_free_cell_inode(file_index) == TRUE) return cell_file_data;

	cell_file_data = CELL_STORAGE + CELL_FILE_STRUCT_OFFSET
					+ file_index * CELL_FILE_DATA_LEN;					                          

	return cell_file_data;
}

/*This function set the file data given the file name*/

static void set_cell_file_data(uchar *file_name, uchar *file_data){
	uchar *cell_file_data = NULL;
	uchar file_index = CELL_INVALID_FILE_INODE;
	file_index = get_cell_file_index(file_name);
	
	if(file_index == CELL_INVALID_FILE_INODE) return;
	cell_file_data = CELL_STORAGE + CELL_FILE_STRUCT_OFFSET
					+ file_index * CELL_FILE_DATA_LEN;
	memcpy(cell_file_data, file_data, CELL_FILE_DATA_LEN);
	
}


