#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
typedef struct idx_record{
	//unsigned long int block_num;
	//unsigned int uncomp_pos; //position from '<page>'
	//unsigned int uncomp_len; // length till '</page>'
	//unsigned int name_len;
	size_t offset;
	char *name;
} IdxRecord;

int compare(const void *a, const void *b)
{
	IdxRecord *ra=(IdxRecord *)a;
	IdxRecord *rb=(IdxRecord *)b;
	//char *name_a=a->name;
	//char *name_b=b->name;
	return strcmp(ra->name, rb->name);
}

int main(int argc, char *argv[]){
	if(argc!=2)
	{
		fprintf(stderr, "Usage: %s filename_to_be_sorted\n",argv[0]);
		exit(1);
	}
	FILE *fpIn=fopen(argv[1] ,"r");
	if(fpIn==NULL)
	{
		fprintf(stderr, "error open %s\n",argv[1]);
		exit(1);
	}
	if(fseeko(fpIn, 0, SEEK_END))
	{
		fprintf(stderr, "error seeking file %s\n",argv[1]);
		exit(1);
	}
	off_t flength=ftell(fpIn);
	if(flength > UINT_MAX){
		fprintf(stderr, "file %s too large\n",argv[1]);
		exit(1);
	}
	fseeko(fpIn, 0 , SEEK_SET);
	IdxRecord *r=malloc(50000000*8);
	int i=0,l;
	int name_len;
	while(!feof(fpIn))
	{
		//Unnecessary to use off_t here.
		r[i].offset=(size_t)ftell(fpIn);
		if(r[i].offset>=flength)
			break;
		fseeko(fpIn, sizeof(long int)+sizeof(int)*2, SEEK_CUR);
		int m=fread(&name_len,sizeof(unsigned int),1 ,fpIn);
		if(m==1)
		{
			//fprintf(stderr,"name_len:%d\n",name_len);
			r[i].name=malloc(name_len+1);
			int n=fread(r[i].name,name_len+1,1,fpIn);
			if(n!=1)
			{
				fprintf(stderr, "fread error1\nfread=%d,name_len+1=%d",n,name_len+1);

				exit(1);
			}
			//fseeko(fpIn,1,SEEK_CUR);
			//printf("%s\n",r[i].name);
			i++;
		}
		else
		{
			l=ftell(fpIn);
			if(l>flength)
				break;
			fprintf(stderr, "fread error0\nl=%d\n",l);
			exit(1);
		}
	}
	//
	qsort(r,i, sizeof(IdxRecord), compare);
	int j;
	FILE *fpOut=fopen("indexsort","w");
	for(j=0;j<i;j++){
		size_t o=r[j].offset;
		fwrite(&o, sizeof(size_t), 1, fpOut);
		//free(r[i].name);
		//free(r[i]);
	}
	fclose(fpIn);
	fclose(fpOut);
	return 0;
}
