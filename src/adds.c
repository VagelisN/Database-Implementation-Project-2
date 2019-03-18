#include "adds.h"
#include "AM.h"
#include "bf.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

/***********************Υλοποίηση Στοίβας*******************************/
int push(stack_node** stack , int num)
{
	stack_node* temp =  malloc(sizeof(stack_node));
	temp->data = num;
	temp->next = *(stack);
	*stack = temp;
	return 0;
}

int pop(stack_node** stack)
{
	stack_node* temp;
	temp = (*stack)->next;
	int retval = (*stack)->data;
	free(*stack);
	*stack = temp;
	return retval;
}

void delete_stack(stack_node* stack)
{
	stack_node* temp;
	while (stack!=NULL)
	{
		temp = stack;
		stack = stack->next;
		free(temp);
	}
}
/***********************************************************************/
 
int create_block(BF_Block **block,int b_r,open_file_info_s* file_ptr)
{
	char* data;
	int zero=0,i,max,one=1;

	BF_Block_Init(block);
	safety(BF_AllocateBlock(file_ptr->fd,*block));
	data = BF_Block_GetData(*block);
	memcpy(data,&b_r,sizeof(int));
	memcpy(data+sizeof(int),&one,sizeof(int));

	data+=2*sizeof(int);

	if (b_r == 0)										//if this is an index block initialize all the pointers to 0
	{
		for ( i = 0; i < file_ptr->max_keys; i++)
		{
			memcpy(data,&zero,sizeof(int));
			data+= file_ptr->attributes.attrLength1;
		}
	}
	else memcpy(data,&zero,sizeof(int));
	BF_Block_SetDirty(*block);

	return AME_OK;
}

void compute_max_entries(open_file_info_s* file_ptr)
{
	file_ptr->max_keys    = ( BF_BLOCK_SIZE - 3*sizeof(int) )/ (file_ptr->attributes.attrLength1+sizeof(int));
	file_ptr->max_entries = ( BF_BLOCK_SIZE - 3*sizeof(int) )/ (file_ptr->attributes.attrLength1+file_ptr->attributes.attrLength2);

	if ((file_ptr->max_keys==0)||(file_ptr->max_entries==0)) exit(2);
}
 
int key_compare(void* key1,void* key2 , char key_type)
{
	if(key_type=='c') return strcmp((char*)key1,(char*)key2);
	else if (key_type=='i')
	{
		if(*(int*)key1 > *(int*)key2) return 1;
		else if (*(int*)key1 == *(int*)key2) return 0;
		else return -1;
	}
	else 
	{
		if(*(float*)key1 > *(float*)key2) return 1;
		else if (*(float*)key1 == *(float*)key2) return 0;
		else return -1;
	}
}
 
int find_block(void* key, stack_node** stack,open_file_info_s* file_ptr)
{
	int keys_num=0,block_type=0,b_root = file_ptr->b_root;
	int index_block_num = b_root, temp_ptr,flag=0;
	int original_keys_num;
	BF_Block *block;
	char* data;
	void* temp_key;
	
	temp_key = malloc(file_ptr->attributes.attrLength1);

	BF_Block_Init(&block);
	safety(BF_GetBlock(file_ptr->fd,b_root,block));

	data = BF_Block_GetData(block); 

	while(block_type==0) 														//While we are at an index block
	{
		push(stack,index_block_num);
		memcpy(&original_keys_num,data+sizeof(int),sizeof(int));
		keys_num = original_keys_num;
		data+=3*sizeof(int);
		while(keys_num)														   //Check its keys one by one
		{

			memcpy(temp_key,data,file_ptr->attributes.attrLength1);
			if(key_compare(temp_key,key,file_ptr->attributes.attrType1) > 0)   //if we found a key that is bigger the block we need is at its left pointer
			{
				
				memcpy(&temp_ptr,data-sizeof(int),sizeof(int));				   //get the left pointer of the key
				safety(BF_UnpinBlock(block));
				
				if (temp_ptr == 0)											   //if there is no leaf block here error
				{
					BF_Block_Destroy(&block);
					return -1;													
				}
				safety(BF_GetBlock(file_ptr->fd,temp_ptr,block));				//get the new block
				index_block_num = temp_ptr;
				data = BF_Block_GetData(block);
				memcpy(&block_type,data,sizeof(int));							//get its block type

				break;															//since we finished with this block, stop this loop

			}
			data+=file_ptr->attributes.attrLength1;
			keys_num--;
			if (keys_num==0)													//if we reached the end of the index block
			{
				memcpy(&temp_ptr,data,sizeof(int));

				safety(BF_UnpinBlock(block));										
						
				safety(BF_GetBlock(file_ptr->fd,temp_ptr,block));				//get the new block
				index_block_num = temp_ptr;
				data = BF_Block_GetData(block);
				memcpy(&block_type,data,sizeof(int));							//get its block type

				break;
			}
			data+=sizeof(int);
		}
	}
	safety(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
	free(temp_key);
	return temp_ptr;
}
 
void sort_block(char* data1,void* key,void* value,open_file_info_s* file_ptr)
{
	char* data2;
	int count =0,gen_num,final_gen_num,blocks_num;
	int gen_size = file_ptr->attributes.attrLength1;
	int type;

	memcpy(&type,data1,sizeof(int));
	if (type) gen_size+=file_ptr->attributes.attrLength2;		//if this is a data block key+value
	else gen_size+=sizeof(int);									//else key+pointer

	count = find_place_in_block(data1,key,file_ptr);            //find where in the block the entry should be inserted
	
	memcpy(&gen_num,data1+sizeof(int),sizeof(int));				//get how many entries are in the block
	final_gen_num = gen_num+1;
	memcpy(data1+sizeof(int),&final_gen_num,sizeof(int));		//update the num of entries to the previous num of entries plus one

	if((gen_num-count)!=0)										//if there are entries on the right of the place that we should insert the entry
	{
		data2 = malloc((gen_num-count)*gen_size);				//allocate memory to move them								
		memcpy(data2,data1+3*sizeof(int)+(count*(gen_size))		//copy them
		,(gen_num-count)*(gen_size));
	}

	data1+=3*sizeof(int)+count*gen_size;						//insert the new record in the right place
	memcpy(data1,key,file_ptr->attributes.attrLength1);
	data1+=file_ptr->attributes.attrLength1;
	if (type)
	{
		memcpy(data1,value,file_ptr->attributes.attrLength2);
		data1+=file_ptr->attributes.attrLength2;
	}
	else 
	{
		memcpy(data1,value,sizeof(int));
		data1+=sizeof(int);
	}
	if((gen_num-count)!=0)										//if there are entries on the right of the place that we should insert the entry
	{
		memcpy(data1,data2,(gen_num-count)*gen_size); 			//paste them after the new entry
		free(data2);
	}
}

int find_place_in_block(char* data1,void* key,open_file_info_s* file_ptr)
{
	int type,move,times,count = 0;
	void* curr_key;

	curr_key = malloc(file_ptr->attributes.attrLength1);
	
	memcpy(&times,data1+sizeof(int),sizeof(int));						//How many entries are there in the block											
	memcpy(&type,data1,sizeof(int));
	if (!type)	move = file_ptr->attributes.attrLength1 + sizeof(int);	//How much do we need to move the pointer to get the next entry
	else move = file_ptr->attributes.attrLength1 + file_ptr->attributes.attrLength2;
	data1+=3*sizeof(int);
	memcpy(curr_key,data1,file_ptr->attributes.attrLength1);
	while((key_compare(key,curr_key,file_ptr->attributes.attrType1) > 0 ) && (count < times))
	{
		data1+= move;
		memcpy(curr_key,data1,file_ptr->attributes.attrLength1);
		count++;
	}	
	free(curr_key);
	return count;
}
 
int calculate_same_keys(char* data,int break_point,open_file_info_s* file_ptr)
{
	void* key1,*key2;
	char* data1 = data;
	int type,recs_num,counter=break_point-1;
	int temp_break_point=break_point;
	memcpy(&recs_num,data+sizeof(int),sizeof(int));
	key1 = malloc(file_ptr->attributes.attrLength1);
	key2 = malloc(file_ptr->attributes.attrLength1);
	data+=3*sizeof(int);
	data+=break_point * (file_ptr->attributes.attrLength1+file_ptr->attributes.attrLength2);
	memcpy(key1,data,file_ptr->attributes.attrLength1);
	data1 = data - (file_ptr->attributes.attrLength1 + file_ptr->attributes.attrLength2);
	memcpy(key2,data1,file_ptr->attributes.attrLength1);
	while(key_compare(key1,key2,file_ptr->attributes.attrType1)==0 && (break_point <recs_num))             //check to the right 
	{
		break_point++;
		data+=file_ptr->attributes.attrLength1+file_ptr->attributes.attrLength2;
		memcpy(key2,data,file_ptr->attributes.attrLength1);
	}
	memcpy(key2,data1,file_ptr->attributes.attrLength1);
	while((break_point==recs_num)&&(counter>0)&&(key_compare(key1,key2,file_ptr->attributes.attrType1)==0))//check to the left
	{
		counter--;
		data1-=file_ptr->attributes.attrLength1+file_ptr->attributes.attrLength2;
		memcpy(key2,data1,file_ptr->attributes.attrLength1);
	}
	free(key1);
	free(key2);
	if (break_point==recs_num) return counter;                                                             //the block is full of the same value and 0 is returned
	else return break_point;																			   //the new break point is returned 
}
 
int break_block(char* data1,void* key,void* value, void** ktgu,open_file_info_s* file_ptr)
{
	int blocks_num, temp_brother,insertion_place,recs_num,break_point;
	int gen_to_keep, gen_to_copy,type;
	void* key_to_go_up;
	char* data2 = NULL,*data3=NULL;
	BF_Block *block2;
	int attrLength1 = file_ptr->attributes.attrLength1;
	int attrLength2 = file_ptr->attributes.attrLength2;
	int gen_size = attrLength1, max_gen;

	memcpy(&type,data1,sizeof(int));
	if(type) 
	{
		gen_size+=attrLength2;													//for the data blocks the size of a pair key-entry
		max_gen = file_ptr->max_entries;
	}
	else
	{ 
		gen_size+=sizeof(int);													//for the index blocks the size of a pair key-pointer
		max_gen = file_ptr->max_keys;
	}

	safety(BF_GetBlockCounter(file_ptr->fd,&blocks_num));
	safety(create_block(&block2,type,file_ptr));
	data2 = BF_Block_GetData(block2);
	
	if (type)																	//if the block is a data block set the pointers to the next block  
	{
		memcpy(&temp_brother,data1+2*sizeof(int),sizeof(int));
		memcpy(data1+2*sizeof(int),&blocks_num,sizeof(int));					//the next of the old block is the new block
		memcpy(data2+2*sizeof(int),&temp_brother,sizeof(int));					//the next of the new block is the old next of the old block
	}
	
	insertion_place = find_place_in_block( data1, key, file_ptr);				//find where in the leaf block the new record should be
	
	if(max_gen / 2 < insertion_place)											//the new entry goes in the new block
	{
		gen_to_keep = (max_gen/2) + max_gen%2;									//how many recs stay in the first block
		if(type==1)																//if we are at a data block move  gen_to_keep accordingly
		{
			gen_to_keep = calculate_same_keys( data1,gen_to_keep,file_ptr);
			if(gen_to_keep==0)													//if the whole block has the same values 
			{
				if(key_compare(key,data1+3*sizeof(int),file_ptr->attributes.attrType1)<0) //if the key to be inserted is less than the keys move all the keys to the new block
					gen_to_keep=0;
				else gen_to_keep=max_gen;										//else keep all the keys and move the key alone to the new block
			}
		}
		memcpy(data1+sizeof(int),&gen_to_keep,sizeof(int));						//change the number of recs in the data
		gen_to_copy=(max_gen-gen_to_keep);										//how many recs move
		memcpy(data2+sizeof(int),&gen_to_copy,sizeof(int));
		memcpy(data2+3*sizeof(int),data1+gen_to_keep*gen_size+3*sizeof(int),gen_size*gen_to_copy);
		
		sort_block(data2,key,value,file_ptr);									//finally sort the newly created block
	}
	
	else if (max_gen / 2 > insertion_place)										//same story 
	{
		gen_to_keep = (max_gen/2);
		if(max_gen % 2 == 0)gen_to_keep--;
		if(type==1)
		{
			gen_to_keep = calculate_same_keys( data1,gen_to_keep,file_ptr);
			if(gen_to_keep==0)
			{
				
				if(key_compare(key,data1+3*sizeof(int),file_ptr->attributes.attrType1)<0)gen_to_keep=0;
				else gen_to_keep=max_gen;
			}
		}
		memcpy(data1+sizeof(int),&gen_to_keep,sizeof(int));					//change the number of recs in the data
		gen_to_copy=(max_gen-gen_to_keep);									//how many recs move
		memcpy(data2+sizeof(int),&gen_to_copy,sizeof(int));
		memcpy(data2+3*sizeof(int),data1+gen_to_keep*gen_size+3*sizeof(int),gen_size*gen_to_copy);
		
		if(type==1 && key_compare(data2+3*sizeof(int),key,file_ptr->attributes.attrType1)==0) //if the new key is equal to the keys on the right we must insert it to the right 
			sort_block(data2,key,value,file_ptr);
		else sort_block(data1,key,value,file_ptr);							//else it can go to the left to keep the balance
	}
	
	else                                                                   //on equalty in order to keep the balance check the modulo of the division
	{
		gen_to_keep = (max_gen/2);											//how many recs stay in the first block
		if(type==1)
		{
			gen_to_keep = calculate_same_keys( data1,gen_to_keep,file_ptr);
			if(gen_to_keep==0)
			{
				if(key_compare(key,data1+3*sizeof(int),file_ptr->attributes.attrType1)<0)gen_to_keep=0;
				else gen_to_keep=max_gen;
			}
		}
		memcpy(data1+sizeof(int),&gen_to_keep,sizeof(int));					//change the number of recs in the data
		gen_to_copy=(max_gen-gen_to_keep);					
		memcpy(data2+sizeof(int),&gen_to_copy,sizeof(int));
		memcpy(data2+3*sizeof(int),data1+gen_to_keep*gen_size+3*sizeof(int),gen_size*gen_to_copy);
		if(max_gen % 2 ==0)
		{
			if(key_compare(key,data1+3*sizeof(int),file_ptr->attributes.attrType1)!= 0)sort_block(data2,key,value,file_ptr);
			else sort_block(data1,key,value,file_ptr);
		}
		else
		{
			if(key_compare(key,data2+3*sizeof(int),file_ptr->attributes.attrType1)!= 0)sort_block(data1,key,value,file_ptr);
			else sort_block(data2,key,value,file_ptr);
		}
	}


	key_to_go_up = malloc(attrLength1);
	memcpy(key_to_go_up,data2+3*sizeof(int),attrLength1);				//the key that needs to go up is the leftmost key of the newly created block
	if(!type)															//if we are at a black block we have to delete the first key because it went up
	{
		memcpy(&recs_num,data2+sizeof(int),sizeof(int));
		recs_num--;
		memcpy(data2+sizeof(int),&recs_num,sizeof(int));
		data3 =data2+3*sizeof(int)+attrLength1;
		memcpy(data2+2*sizeof(int),data3,BF_BLOCK_SIZE-(3*sizeof(int)+attrLength1));
	}
	BF_Block_SetDirty(block2);
	safety(BF_UnpinBlock(block2));
	BF_Block_Destroy(&block2);

	memcpy((*ktgu),key_to_go_up,attrLength1);
	free (key_to_go_up);
}

int first_dblock(open_file_info_s* open_file)
{
    int first_child,block_type=0;
    char* data;
    BF_Block *block;
    if(open_file->b_root==-1)
    return 1;
    BF_Block_Init(&block);
    safety(BF_GetBlock(open_file->fd,open_file->b_root,block));
    data=BF_Block_GetData(block);
    safety(BF_UnpinBlock(block));                              
    while(block_type==0)
    {
        data+=2*sizeof(int);
        memcpy(&first_child,data,sizeof(int));
        safety(BF_GetBlock(open_file->fd,first_child,block));
        data=BF_Block_GetData(block);
        memcpy(&block_type,data,sizeof(int));
        safety(BF_UnpinBlock(block));
    }
    BF_Block_Destroy(&block);
    return first_child;
}

int os_compare(void *key,void *value,int op,char type)
{
    int result;
    result=key_compare(key,value,type);
    if(op==EQUAL)
    {
        if(result==0) return 1;
        else return 0;
    }
    if(op==NOT_EQUAL)
    {
        if(result!=0) return 1;
        else return 0;
    }
    if(op==GREATER_THAN)
    {
        if(result==1) return 1;
        else return 0;
    }
    if(op==GREATER_THAN_OR_EQUAL)
    {
        if(result==1 || result==0) return 1;
        else return 0;
    }
    if(op==LESS_THAN)
    {
        if(result==-1) return 1;
        else return 0;  
    }
    if(op==LESS_THAN_OR_EQUAL)
    {
        if(result==-1 || result==0) return 1;
        else return 0;
    }
}

/**************************PRINTS************************************/
void print_red_block(char* data,open_file_info_s* file_ptr)
{
	int recs_num;
	memcpy(&recs_num,data+sizeof(int),sizeof(int));
		printf("TYPE : %d\n",*(int*)data);
	data+=3*sizeof(int);

	while(recs_num)
	{
		if(file_ptr->attributes.attrType1=='i')printf("key %d ",*(int*)data);
		else if(file_ptr->attributes.attrType1=='c')printf("key %s ",data);
		else printf("key %.2f ",*(float*)data);
		
		
		if(file_ptr->attributes.attrType2=='i') printf("value %d\n",*(int*)(data+file_ptr->attributes.attrLength1));
		else if(file_ptr->attributes.attrType2=='c')printf("value %s\n",(data+file_ptr->attributes.attrLength1));
		else printf("value %.2f\n",*(float*)(data+file_ptr->attributes.attrLength1));
		data+=file_ptr->attributes.attrLength1+file_ptr->attributes.attrLength2;
		recs_num--;
	}
}

void print_black_block(char* data,open_file_info_s* file_ptr)
{
	int recs_num;
	memcpy(&recs_num,data+sizeof(int),sizeof(int));
	printf("pointer %d ",*(int*)(data+2*sizeof(int)));
	data+=3*sizeof(int);
	while(recs_num)
	{
		if(file_ptr->attributes.attrType1=='i')printf("key %d  pointer %d\n",*(int*)data,*(int*)(data+sizeof(int)));
		else if(file_ptr->attributes.attrType1=='c'){printf("key %s pointer %d\n",data,*(int*)(data+file_ptr->attributes.attrLength1));}
		else printf("key %.2f pointer %d\n",*(float*)data,*(int*)(data+file_ptr->attributes.attrLength1));
		data+=file_ptr->attributes.attrLength1+sizeof(int);
		recs_num--;
	}
}

/**************************PRINTS************************************/
