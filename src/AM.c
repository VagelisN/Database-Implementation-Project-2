#include "AM.h"
#include "bf.h"
#include "adds.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int AM_errno = AME_OK;

open_file_info_s** open_files=NULL;
open_scan_info_s** open_scans=NULL;
int open_files_num;
int open_scans_num;  

/* Όπως η συνάρτηση που δόθηκε στο αρχείο bf_main
 * με τη διαφορά οτι γυρίζει Error Number ωστε 
 * να διαχειριστεί το λάθος η αντίστοιχη am_main*/

#define safety(call)          \
{                             \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      AM_errno = AME_BF_ERROR;\
      return AME_BF_ERROR;    \
    }                         \
}
 
#define AM_safety(call)       \
{                             \
    int code = call;		  \
    if (code < AME_OK)        \
    {                         \
      AM_errno = call;        \
      return call;            \
    }                         \
}


/* Αν δοθεί -1 βρίσκει την επόμενη NULL θέση του πίνακα open_files 
 * αλλιώς βρίσκει σε ποιά θέση είναι ο file descriptor num
 * αν γυρίσει -1 υπάρχει λάθος*/
 
int find_file(int num)
{
	int i;

	if (num == -1)
	{
		for (i = 0; i < AM_MAX_OPEN_FILES; i++)
		{
			if ( open_files[i] == NULL ) return i;
		}
		return -1;
	}
	else
	{
		for (i = 0; i < AM_MAX_OPEN_FILES; i++)
		{
			if (open_files[i] != NULL)
			{
				if (open_files[i]->fd == num) return i;
			}
		}
	}
	return -1;
}

void AM_Init() 
{
	BF_Init(MRU);
	int i;
	open_files=malloc(AM_MAX_OPEN_FILES*sizeof(open_file_info_s*));
	open_scans=malloc(AM_MAX_OPEN_SCANS*sizeof(open_scan_info_s*));
	for ( i = 0; i < 20; i++)
	{
		open_files[i]=NULL;
		open_scans[i]=NULL;
	}
	open_files_num = 0;
	open_scans_num = 0;
	return;
}


int AM_CreateIndex(char *fileName, char attrType1, int attrLength1, char attrType2, int attrLength2)
{
	int fd, am_int = 206,b_root = -1;
	am_attr_types_s attributes;
	BF_Block *block;
	char* data;
	
	if(access(fileName, F_OK) != -1)									//if the file already exists
	{
		AM_errno= AME_FILE_ALREADY_EXISTS;
		return AME_FILE_ALREADY_EXISTS;
	}
	
	if((attrType1=='i' || attrType1=='f')&&(attrLength1!=4))			//if the type and the length do not match
    {
        AM_errno = AME_TYPE_LENGTH_MISMATCH;
        return AM_errno;
    }
    else if (attrType1=='c' &&(( attrLength1>255)||(attrLength1<1)))
    {
        AM_errno = AME_TYPE_LENGTH_MISMATCH;
        return AM_errno;
    }
    if((attrType2=='i' || attrType2=='f')&&(attrLength2!=4))
    {
        AM_errno = AME_TYPE_LENGTH_MISMATCH;
        return AM_errno;
    }
    else if (attrType2=='c' &&(( attrLength2>255)||(attrLength2<1)))
    {
        AM_errno = AME_TYPE_LENGTH_MISMATCH;
        return AM_errno;
    }   
																	
	BF_Block_Init(&block);												//Everything is fine -> Create the file
	safety(BF_CreateFile(fileName));
	
	safety(BF_OpenFile(fileName, &fd));									//Store the necessary info to the first block of the file
	safety(BF_AllocateBlock(fd, block));
	data = BF_Block_GetData(block);

	memcpy(data,&am_int,sizeof(int));
	attributes.attrType1 = attrType1;
	attributes.attrType2 = attrType2;
	attributes.attrLength1 = attrLength1;
	attributes.attrLength2 = attrLength2;
	memcpy(data+sizeof(int),&attributes,sizeof(am_attr_types_s));
	memcpy(data+sizeof(int)+sizeof(am_attr_types_s),&b_root,sizeof(int));

	BF_Block_SetDirty(block);
	safety(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
	safety(BF_CloseFile(fd));
	
	return AME_OK;
}

int AM_DestroyIndex(char *fileName) 
{
	int i,flag=0;
	for (i = 0; i < AM_MAX_OPEN_FILES; i++)					//check if it is open
	{
		if(open_files[i]!=NULL)
		{
			if(strcmp(fileName,open_files[i]->filename)==0)
			{
				AM_errno = AME_FILE_IS_OPEN;
				flag=1;
				break;
			}
				
		}
	}
	for (i = 0; i < AM_MAX_OPEN_SCANS; i++)					//check if a scan is open for this file
	{
		if(open_scans[i]!=NULL)
		{
			if(strcmp(fileName,open_scans[i]->filename)==0)
			{
				AM_errno = AME_FILE_IS_OPEN;
				flag=1;
				break;
			}
				
		}
		
	}
	
	if(!flag)remove(fileName);								//if not open and no scans open for this file remove it
	return AME_OK;
}

int AM_OpenIndex (char *fileName)
{
	BF_Block *block;
	int block_nums, recognition;
	char* data;
	open_file_info_s* file_ptr;
	
	
	if (open_files_num == AM_MAX_OPEN_FILES)					 //if there is no empty space, error
	{
		AM_errno = AME_NO_FILE_SPACE;
		return AME_NO_FILE_SPACE;			
	}
	
	int next_null = find_file(-1);								 //search for empty space in array
	open_files_num++;
	open_files[next_null] = malloc(sizeof(open_file_info_s));	 //create the struct that holfd the file info
	file_ptr = open_files[next_null];							
	
	safety(BF_OpenFile(fileName, &file_ptr->fd));				
	safety(BF_GetBlockCounter(file_ptr->fd,&block_nums));		
	if (block_nums == 0) 										 //if there are no blocks, this is not an access method file, or there's something wrong
	{
		AM_errno = AME_WRONG_FILE_TYPE;
		return AME_WRONG_FILE_TYPE;						
	}
	
	BF_Block_Init(&block);							
	safety(BF_GetBlock(file_ptr->fd,0,block));					 //retrieve the first block
	data = BF_Block_GetData(block);	
	if (*(int*)data != 206) return AME_ERROR;					 //206 is the recognition number for the AM_FILE and is 
																 //always the first int writen in the first block
	
	
	file_ptr->filename=malloc(sizeof(char)*(strlen(fileName)+1));//start copying the info inside the first block to the file's place in the open_files array
	strcpy(file_ptr->filename,fileName);						  //the file name
	am_attr_types_s* attrs = (am_attr_types_s*)(data+sizeof(int));
	file_ptr->attributes.attrType1 = attrs->attrType1;			 //the type of the key
	file_ptr->attributes.attrType2 = attrs->attrType2;			 //the type of the value
	file_ptr->attributes.attrLength1 = attrs->attrLength1;		 //the length of the key
	file_ptr->attributes.attrLength2 = attrs->attrLength2;		 //the length of the value
	memcpy(&(file_ptr->b_root), data+sizeof(int)+				 //the root
		sizeof(am_attr_types_s), sizeof(int)); 
	
	compute_max_entries(file_ptr);								 //computes the number of keys that an index block can hold 
																 //and the number of key-entry pairs a data block can hold

	safety(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);						

 	return next_null;											 //it returns the file's place in the open_files array 
}

int AM_CloseIndex (int fileDesc) 
{
	int i;
	char* data;
	BF_Block* block;

	for (i = 0; i < AM_MAX_OPEN_SCANS ; i++)					//if there is no open scan for this file
	{
		if(open_scans[i]!=NULL)
		{
			if(strcmp(open_scans[i]->filename,open_files[fileDesc]->filename)==0)
			{
				AM_errno = AME_OPEN_SCAN_FOR_THIS_FILE;
				return AME_OPEN_SCAN_FOR_THIS_FILE;
			}
		}
	}
	
	BF_Block_Init(&block);									
	safety(BF_GetBlock(open_files[fileDesc]->fd,0,block));
	data = BF_Block_GetData(block);
	
	data+=sizeof(int)+sizeof(am_attr_types_s);
	memcpy(data,&(open_files[fileDesc]->b_root),sizeof(int));  //update the root of the b+ tree inside the metadata block 0
	
	BF_Block_SetDirty(block);
	safety(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
	
	BF_CloseFile(open_files[fileDesc]->fd);					  //and close the file 
	free(open_files[fileDesc]->filename);					  //free any allocated memory
	free(open_files[fileDesc]);	
	open_files[fileDesc] = NULL;
	open_files_num--;
	
	return AME_OK;
}

int AM_InsertEntry(int fileDesc, void *value1, void *value2) 
{
	int tree_rt, one = 1,two = 2, attrLength1,attrLength2;
	int insertion_block, recs_num, insertion_place, record_size, blocks_num, temp_blocks_num; 
	int temp_brother, father, flag, keys_num;
	void* key_to_go_up = NULL, *temp_key_to_go_up = NULL;
	open_file_info_s* file_ptr;
	BF_Block *black,*red;
	char* data1,*data2;
	stack_node* stack = NULL;

	if (open_files[fileDesc]==NULL) 										//We don't have such an open file
	{
		AM_errno = AME_FILE_NOT_FOUND;
		return AME_FILE_NOT_FOUND;	
	}
	file_ptr = open_files[fileDesc];
	tree_rt = file_ptr->b_root;
	attrLength1 = file_ptr->attributes.attrLength1;
	attrLength2 = file_ptr->attributes.attrLength2;
	record_size = attrLength1+attrLength2;

	if (tree_rt==-1)														//No root created yet
	{
		safety(BF_GetBlockCounter(file_ptr->fd,&blocks_num));
		
		if(blocks_num == 1)													//no root yet and the first data block has not been allocated yet
		{
			safety(create_block(&red,1,file_ptr));							//create a data block and insert the first key-value pair
			data2 = BF_Block_GetData(red);
			memcpy(data2+3*sizeof(int),value1,attrLength1);
			memcpy(data2+3*sizeof(int)+attrLength1,value2,attrLength2);
			safety(BF_UnpinBlock(red));										
		}
		else if(blocks_num ==2)												//no root yet but the first data block has been allocated
		{
			
			BF_Block_Init(&red);
			BF_GetBlock(file_ptr->fd,1,red);
			data2 = BF_Block_GetData(red);
			memcpy(&recs_num,data2+sizeof(int),sizeof(int));				//get how many record are in the block
			if(recs_num < file_ptr->max_entries)							//there is still room in the first data block
				sort_block(data2,value1,value2,file_ptr); 					//sort the block with the new key-value pair
				
			else 															//no room to the first data block -> create a root index block
			{
				key_to_go_up = malloc (attrLength1);
				break_block(data2,value1,value2,&key_to_go_up,file_ptr);	//break the first data block in two and split the entries
				safety(create_block(&black,0,file_ptr));					//create a root block
				data1 = BF_Block_GetData(black);							
				memcpy(data1+3*sizeof(int),key_to_go_up,attrLength1); 
				memcpy(data1+2*sizeof(int),&one,sizeof(int));				//the old block is block number 1
				memcpy(data1+3*sizeof(int)+attrLength1,&two,sizeof(int));	//the newly created block is the block number 2
				
				file_ptr->b_root = 3; 										//the new root is block number 3
				
				free(key_to_go_up);
				BF_UnpinBlock(black);
				BF_Block_Destroy(&black);
			}
			BF_Block_SetDirty(red);
			BF_UnpinBlock(red);
			BF_Block_Destroy(&red);
		}
	}
	
	else 																	//A root exists
	{
		stack = NULL;

		insertion_block = find_block(value1,&stack,file_ptr);				//get the block where the key should be inserted
		if(insertion_block == -1)											//error there is no such block
		{
			AM_errno = AME_BLOCK_NOT_FOUND;
			return AME_BLOCK_NOT_FOUND;
		}

		if (insertion_block!=-1)											//If the block was found without errors
		{
			BF_Block_Init(&red);
			BF_Block_Init(&black);
			safety(BF_GetBlock(file_ptr->fd,insertion_block,red));
			data1 = BF_Block_GetData(red);
			memcpy(&recs_num,data1+sizeof(int),sizeof(int));
			if (file_ptr->max_entries > recs_num)							//If we have enough space in the data block that was found
			{
				sort_block(data1,value1,value2, file_ptr);					//find where in the data block the new record should be
				
				BF_Block_SetDirty(red);												
				safety(BF_UnpinBlock(red));											
				BF_Block_Destroy(&red);																				
			}
			else															//There is not enough space in the data block,we need to break it in two
			{
				key_to_go_up = malloc (attrLength1);
				temp_key_to_go_up = malloc (attrLength1);
				break_block(data1,value1,value2,&key_to_go_up,file_ptr);	//Break the block and get the key that should go to the index levels
				BF_Block_SetDirty(red);
				safety(BF_UnpinBlock(red));
				BF_Block_Destroy(&red);
				
				flag = 1;
				while (((stack)!=NULL)&&(flag))								//Go up the index levels until we reach the root or find an index block that has space
				{
					father = pop(&stack);									//Get the father index block
					safety(BF_GetBlock(file_ptr->fd,father,black));
					data1 = BF_Block_GetData(black);
					memcpy(&keys_num,data1+sizeof(int),sizeof(int));
					if (file_ptr->max_keys > keys_num)						//if the key fits in the index block just insert it 
					{
						safety(BF_GetBlockCounter(file_ptr->fd,&blocks_num));
						blocks_num--;
						sort_block(data1,key_to_go_up,&blocks_num,file_ptr);//Sort the index block with the new key 
						flag = 0;											//No need to continue going up the key's place was found
					}
					
					else 													//The father does not have space
					{
						safety(BF_GetBlockCounter(file_ptr->fd,&blocks_num));
						blocks_num-=1;
						break_block(data1,key_to_go_up,&blocks_num,&temp_key_to_go_up,file_ptr);//break the father and get the key that goes up
						memcpy(key_to_go_up,temp_key_to_go_up,attrLength1);

					}
					BF_Block_SetDirty(black);
					BF_UnpinBlock(black);
				}
				if(flag)													//We reached the root block
				{
					safety(BF_GetBlockCounter(file_ptr->fd,&blocks_num));
					BF_Block_Destroy(&black);
					safety(create_block(&black,0,file_ptr));				//Create a new root block
					data1 = BF_Block_GetData(black);
					data1 += 2* sizeof(int);
					memcpy(data1,&(file_ptr->b_root),sizeof(int));			//The left pointer is the previous root
					memcpy(data1+sizeof(int),key_to_go_up,attrLength1);		//Write down the key 
					temp_blocks_num = blocks_num-1;
					memcpy(data1+sizeof(int)+attrLength1, &temp_blocks_num,sizeof(int));////The right pointer is the last index block that has been created before the root
					file_ptr->b_root=blocks_num;
					BF_UnpinBlock(black);

				}
				free(key_to_go_up);
				free(temp_key_to_go_up);
			}
			BF_Block_Destroy(&black);
		}
		else 
		{
			AM_errno = AME_INSERT_ERROR;
			return AME_INSERT_ERROR;
		}
		delete_stack(stack);												//free the stack	
	}
  	return AME_OK;
}

int AM_OpenIndexScan(int fileDesc, int op, void *value) 
{
    int scanDesc=0,block_num,next_block;
    BF_Block *block;
    char* data;
    BF_Block_Init(&block);
      
    if(open_files[fileDesc]==NULL)											//The file specified is not open in the open_files array
    {
		AM_errno = AME_FILE_NOT_FOUND;
		return AME_FILE_NOT_FOUND;
	}
    if(open_scans_num==AM_MAX_OPEN_SCANS)									//There is no space in the open_scans array
    {
		AM_errno = AME_NO_SCAN_SPACE;
		return AME_NO_SCAN_SPACE;				
	}
    
    open_scans_num++;
    
    while(open_scans[scanDesc]!=NULL)										//Find the next empty place in the open_scans array
    scanDesc++;
      
    open_scans[scanDesc]=malloc(sizeof(open_scan_info_s));					//Set the info needed for the scan
    open_scans[scanDesc]->fd=fileDesc;
    open_scans[scanDesc]->filename =
		malloc(sizeof(char)*(strlen(open_files[fileDesc]->filename))+1);

    strcpy(open_scans[scanDesc]->filename,open_files[fileDesc]->filename);
    open_scans[scanDesc]->op=op;
    open_scans[scanDesc]->value=value;
    open_scans[scanDesc]->next_record=1;
    
    if(open_files[fileDesc]->b_root != -1)									//if a root exists
    {
		stack_node* stack =NULL;
		block_num=find_block(value,&stack,open_files[fileDesc]);
		if(block_num==-1)													//error : there is no such block
		{
			AM_errno = AME_BLOCK_NOT_FOUND;
			return AME_BLOCK_NOT_FOUND;
		}     
		free(stack);
	}
	else block_num = 1;														//else we only have one data block which is block 1    
	
	       
    safety(BF_GetBlock(open_files[fileDesc]->fd,block_num,block));
    data=BF_Block_GetData(block);
    memcpy(&next_block,data+2*sizeof(int),sizeof(int));						//get the next block from the first block's pointer
    safety(BF_UnpinBlock(block));
      
    open_scans[scanDesc]->key_block=block_num;								//hold the block that has the key			
    if(op==EQUAL)															//choose the start and end block depending on the operator given
    {   
        open_scans[scanDesc]->start_block=block_num;						//on equal we start looking at the key_block
        open_scans[scanDesc]->end_block=next_block;      					//and stop at the key block
    }
    else if(op==NOT_EQUAL)
    {
          
        open_scans[scanDesc]->start_block=first_dblock(open_files[fileDesc]); //on not equal we need to check every block
        open_scans[scanDesc]->end_block=0;
          
    }
    else if(op==LESS_THAN || op==LESS_THAN_OR_EQUAL)
    {
        open_scans[scanDesc]->start_block=first_dblock(open_files[fileDesc]); //on less than we start from the first block
        open_scans[scanDesc]->end_block=next_block;							  //and get the next block until we reach the end of the key block
    }
    else
    {
        open_scans[scanDesc]->start_block=block_num;						//on greater than we start from the key block and stop at the end
        open_scans[scanDesc]->end_block=0;									
    }   
  
  return scanDesc;
}

void *AM_FindNextEntry(int scanDesc) 
{
    int i,entries,length1,length2,next;
    open_scan_info_s *curr_scan;
    BF_Block *block;
    char *data,type1;
    void *x,*y;
      
    curr_scan = open_scans[scanDesc];
    if( curr_scan == NULL) 
    {
        AM_errno = AME_SCAN_NOT_FOUND;
        return NULL;
    }
    length1=open_files[curr_scan->fd]->attributes.attrLength1;
    length2=open_files[curr_scan->fd]->attributes.attrLength2;
    type1=open_files[curr_scan->fd]->attributes.attrType1;
    x=malloc(length1);
    y=malloc(length2);
    BF_Block_Init(&block);
  
    while(curr_scan->start_block!=curr_scan->end_block)                       	//search all blocks from "start block" to "end block"
    {
        if (BF_GetBlock(curr_scan->fd,curr_scan->start_block,block)!=BF_OK)     //get the info of each block
        {
            AM_errno = AME_BF_ERROR;
            return NULL;
        }
        data=BF_Block_GetData(block);
        data+=sizeof(int);
        memcpy(&entries,data,sizeof(int));
        data+=sizeof(int);
        memcpy(&next,data,sizeof(int));
        data=data+sizeof(int)+(curr_scan->next_record-1)*(length1+length2);      //find the next entry
          
        while(curr_scan->next_record<=entries)                           		 //for every entry in the block
        {
            if(curr_scan->start_block==curr_scan->key_block)      				 //check if we are in the key block
            {   
                memcpy(x,data,length1);
                if(os_compare(x,curr_scan->value,curr_scan->op,type1))    		 //then check if the OP is true
                {
                    (curr_scan->next_record)++;
                    memcpy(y,data+length1,length2);                              //if it is true return the entry
                    return y;
                }
                else
                {
                    (curr_scan->next_record)++;                          		 //else just go to the next entry
                    data=data+length1+length2;
                }
            }
            else                                                            	 //if we are not in the key block return the next entry
            {
                (curr_scan->next_record)++;
                memcpy(y,data+length1,length2);
                return y;
            }
        }
          
        if(BF_UnpinBlock(block)!=BF_OK)                   						 //go to next block when there are no more entries
        {
            AM_errno = AME_BF_ERROR;
            return NULL;
        }
        curr_scan->next_record=1;
        curr_scan->start_block=next;
    }    
    AM_errno=AME_EOF;
    return NULL;
}


int AM_CloseIndexScan(int scanDesc)
{
	if(open_scans[scanDesc]==NULL)
	{
		AM_errno = AME_SCAN_NOT_FOUND;
		return AME_SCAN_NOT_FOUND;
	}
	
	free(open_scans[scanDesc]->filename);
	free(open_scans[scanDesc]);
	open_scans[scanDesc]=NULL;
	
	open_scans_num--;

	return AME_OK;
}

void AM_PrintError(char *errString) 
{
    if(errString != NULL) printf("%s\n",errString);
 
    switch (AM_errno)
    {
        case AME_EOF:
            printf("\nEnd of file has been reached\n");
            break;
 
        case AME_ERROR:
            printf("\nError\n");
            break;
 
        case AME_FILE_NOT_FOUND:
            printf("\nThe specified file was not found\n");
            break;
 
        case AME_NO_FILE_SPACE:
            printf("\nNo space to open another file\n");
            break;
 
        case AME_WRONG_FILE_TYPE:
            printf("\nFile was not an Access Method file\n");
            break;
 
        case AME_BF_ERROR:
            printf("\nError in BF level\n");
            break;
 
        case AME_SCAN_NOT_FOUND:
            printf("\nThe specified scan was not found\n");
            break;
 
        case AME_TYPE_LENGTH_MISMATCH:
            printf("\nThe type and the length given do not match\n");
            break;
            
		case AME_FILE_IS_OPEN:
			printf("\nThe specified file is open\n");
			break;
			
		case AME_INSERT_ERROR:
			printf("\nINSERTION FAULT \n");
			break;
			
		case AME_FILE_ALREADY_EXISTS:
			printf("\nCannot create file. File already exists\n");
			break;
			
		case AME_NO_SCAN_SPACE:
			printf("\nCannot open scan. No space for another scan\n");
			break;
			
		case AME_BLOCK_NOT_FOUND:
			printf("\n The value doesnt match to any block in the file.\n");
			break;
        default:
            printf("\nUnknown error type\n");
            break;

			
    }
}

void AM_Close() 
{
	int i;
	for (i = 0; i < AM_MAX_OPEN_FILES; i++)
	{
		if(open_files[i] != NULL)
		{
			free(open_files[i]);
			free(open_files[i]->filename);
		}
		if(open_scans[i] != NULL)free(open_scans[i]);
	}
	free(open_files);
	free(open_scans);
	BF_Close();
}
