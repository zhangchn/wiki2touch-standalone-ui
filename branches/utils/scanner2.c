/**************
 * Scanner2.c, derived from bzip2recover program of bzip2 libs.
 *   A scanner-indexer for media-wiki xml.bz2 dump.
 *   This is, briefly, BSD licensed.
 *                        -zhangchnxp # 9mai1 d07 c0m
 **************/

/**************
 * index record format:
 * .article_start_block_bit_offset: off_t
 * .article_end_block_bit_offset: off_t
 * .article_uncompressed_stream_position: int32
 * .article_uncompressed_stream_length: int32
 * .article_name_length: int32
 * .article_name: char*.article_name_length
 * ************/

/*-----------------------------------------------------------*/
/*--- Block recoverer program for bzip2                   ---*/
/*---                                      bzip2recover.c ---*/
/*-----------------------------------------------------------*/

/* ------------------------------------------------------------------
   This file is part of bzip2/libbzip2, a program and library for
   lossless, block-sorting data compression.

   bzip2/libbzip2 version 1.0.5 of 10 December 2007
   Copyright (C) 1996-2007 Julian Seward <jseward@bzip.org>

   Please read the WARNING, DISCLAIMER and PATENTS sections in the 
   README file.

   This program is released under the terms of the license contained
   in the file LICENSE.
   ------------------------------------------------------------------ */

/* This program is a complete hack and should be rewritten properly.
	 It isn't very complicated. */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <bzlib.h>

#define S_DBG 0

struct tagitem{
	char *tagName;
	struct tagitem *parent;
};
typedef struct tagitem TagItem;
typedef struct idx_record{
	//off_t block_start_bit_offset;
	//off_t block_end_bit_offset;
	unsigned long int block_num;
	unsigned int uncomp_pos; //position from '<page>'
	unsigned int uncomp_len; // length till '</page>'
	unsigned int name_len;
	char *name;
} IdxRecord;


/* This program records bit locations in the file to be recovered.
   That means that if 64-bit ints are not supported, we will not
   be able to recover .bz2 files over 512MB (2^32 bits) long.
   On GNU supported platforms, we take advantage of the 64-bit
   int support to circumvent this problem.  Ditto MSVC.

   This change occurred in version 1.0.2; all prior versions have
   the 512MB limitation.
*/
#ifdef __GNUC__
   typedef  unsigned long long int  MaybeUInt64;
#  define MaybeUInt64_FMT "%llx"
#else
#ifdef _MSC_VER
   typedef  unsigned __int64  MaybeUInt64;
#  define MaybeUInt64_FMT "%I64u"
#else
   typedef  unsigned int   MaybeUInt64;
#  define MaybeUInt64_FMT "%u"
#endif
#endif

typedef  unsigned int   UInt32;
typedef  int            Int32;
typedef  unsigned char  UChar;
typedef  char           Char;
typedef  unsigned char  Bool;
#define True    ((Bool)1)
#define False   ((Bool)0)


#define BZ_MAX_FILENAME 2000

//Char inFileName[BZ_MAX_FILENAME];
//Char outFileName[BZ_MAX_FILENAME];
char *inFileName;
char *outFileName;
Char progName[BZ_MAX_FILENAME];

MaybeUInt64 bytesOut = 0;
MaybeUInt64 bytesIn  = 0;


/*---------------------------------------------------*/
/*--- Header bytes                                ---*/
/*---------------------------------------------------*/

#define BZ_HDR_B 0x42                         /* 'B' */
#define BZ_HDR_Z 0x5a                         /* 'Z' */
#define BZ_HDR_h 0x68                         /* 'h' */
#define BZ_HDR_0 0x30                         /* '0' */
 

/*---------------------------------------------------*/
/*--- I/O errors                                  ---*/
/*---------------------------------------------------*/

/*---------------------------------------------*/
static void readError ( void )
{
   fprintf ( stderr,
             "%s: I/O error reading `%s', possible reason follows.\n",
            progName, inFileName );
   perror ( progName );
   fprintf ( stderr, "%s: warning: output file(s) may be incomplete.\n",
             progName );
   exit ( 1 );
}


/*---------------------------------------------*/
static void writeError ( void )
{
   fprintf ( stderr,
             "%s: I/O error reading `%s', possible reason follows.\n",
            progName, inFileName );
   perror ( progName );
   fprintf ( stderr, "%s: warning: output file(s) may be incomplete.\n",
             progName );
   exit ( 1 );
}


/*---------------------------------------------*/
static void mallocFail ( Int32 n )
{
   fprintf ( stderr,
             "%s: malloc failed on request for %d bytes.\n",
            progName, n );
   fprintf ( stderr, "%s: warning: output file(s) may be incomplete.\n",
             progName );
   exit ( 1 );
}


/*---------------------------------------------*/
static void tooManyBlocks ( Int32 max_handled_blocks )
{
   fprintf ( stderr,
             "%s: `%s' appears to contain more than %d blocks\n",
            progName, inFileName, max_handled_blocks );
   fprintf ( stderr,
             "%s: and cannot be handled.  To fix, increase\n",
             progName );
   fprintf ( stderr, 
             "%s: BZ_MAX_HANDLED_BLOCKS in bzip2recover.c, and recompile.\n",
             progName );
   exit ( 1 );
}



/*---------------------------------------------------*/
/*--- Bit stream I/O                              ---*/
/*---------------------------------------------------*/

typedef
   struct {
      FILE*  handle;
      Int32  buffer;
      Int32  buffLive;
      Char   mode;
   }
   BitStream;


/*---------------------------------------------*/
static BitStream* bsOpenReadStream ( FILE* stream )
{
   BitStream *bs = malloc ( sizeof(BitStream) );
   if (bs == NULL) mallocFail ( sizeof(BitStream) );
   bs->handle = stream;
   bs->buffer = 0;
   bs->buffLive = 0;
   bs->mode = 'r';
   return bs;
}


/*---------------------------------------------*/
static BitStream* bsOpenWriteStream ( FILE* stream )
{
   BitStream *bs = malloc ( sizeof(BitStream) );
   if (bs == NULL) mallocFail ( sizeof(BitStream) );
   bs->handle = stream;
   bs->buffer = 0;
   bs->buffLive = 0;
   bs->mode = 'w';
   return bs;
}
static BitStream* bsOpenWriteStream2 ( )
{
   BitStream *bs = malloc ( sizeof(BitStream) );
   if (bs == NULL) mallocFail ( sizeof(BitStream) );
   //bs->handle = NULL;
   bs->buffer = 0;
   bs->buffLive = 0;
   bs->mode = 'w';
   return bs;
}


/*---------------------------------------------*/
static void bsPutBit ( BitStream* bs, Int32 bit )
{
   if (bs->buffLive == 8) {
      Int32 retVal = putc ( (UChar) bs->buffer, bs->handle );
      if (retVal == EOF) writeError();
      bytesOut++;
      bs->buffLive = 1;
      bs->buffer = bit & 0x1;
   } else {
      bs->buffer = ( (bs->buffer << 1) | (bit & 0x1) );
      bs->buffLive++;
   };
}

char* bsPutBit2 ( BitStream* bs, char* buff, Int32 bit )
{
   if (bs->buffLive == 8) {
      *buff=(UChar)bs->buffer;
      //if (retVal == EOF) writeError();
      buff++;
      bs->buffLive = 1;
      bs->buffer = bit & 0x1;
   } else {
      bs->buffer = ( (bs->buffer << 1) | (bit & 0x1) );
      bs->buffLive++;
   };
   return buff;
}


/*---------------------------------------------*/
/*--
   Returns 0 or 1, or 2 to indicate EOF.
--*/
static Int32 bsGetBit ( BitStream* bs )
{
   if (bs->buffLive > 0) {
      bs->buffLive --;
      return ( ((bs->buffer) >> (bs->buffLive)) & 0x1 );
   } else {
      Int32 retVal = getc ( bs->handle );
      if ( retVal == EOF ) {
         if (errno != 0) readError();
         return 2;
      }
      bs->buffLive = 7;
      bs->buffer = retVal;
      return ( ((bs->buffer) >> 7) & 0x1 );
   }
}


/*---------------------------------------------*/
static void bsClose ( BitStream* bs )
{
   Int32 retVal;

   if ( bs->mode == 'w' ) {
      while ( bs->buffLive < 8 ) {
         bs->buffLive++;
         bs->buffer <<= 1;
      };
      retVal = putc ( (UChar) (bs->buffer), bs->handle );
      if (retVal == EOF) writeError();
      bytesOut++;
      retVal = fflush ( bs->handle );
      if (retVal == EOF) writeError();
   }
   retVal = fclose ( bs->handle );
   if (retVal == EOF) {
      if (bs->mode == 'w') writeError(); else readError();
   }
   free ( bs );
}

char *bsClose2(BitStream* bs, char* buff)
{
   Int32 retVal;

   if ( bs->mode == 'w' ) {
      while ( bs->buffLive < 8 ) {
         bs->buffLive++;
         bs->buffer <<= 1;
      };
      *buff= (UChar) (bs->buffer) ;
      buff++;
      bytesOut++;
   }
   //retVal = fclose ( bs->handle );
   //if (retVal == EOF) {
   //   if (bs->mode == 'w') writeError(); else readError();
   //}
   free ( bs );
   return buff;
}


/*---------------------------------------------*/
static void bsPutUChar ( BitStream* bs, UChar c )
{
   Int32 i;
   for (i = 7; i >= 0; i--)
      bsPutBit ( bs, (((UInt32) c) >> i) & 0x1 );
}

char *bsPutUChar2(BitStream* bs, char *buff, UChar c)
{
   Int32 i;
   for (i = 7; i>=0; i--)
      buff=bsPutBit2 (bs, buff, (((UInt32) c) >>i) & 0x1);
   return buff;
}

/*---------------------------------------------*/
static void bsPutUInt32 ( BitStream* bs, UInt32 c )
{
   Int32 i;

   for (i = 31; i >= 0; i--)
      bsPutBit ( bs, (c >> i) & 0x1 );
}
char* bsPutUInt32_2 ( BitStream* bs, char *buff , UInt32 c )
{
   Int32 i;

   for (i = 31; i >= 0; i--)
      buff=bsPutBit2 ( bs, buff, (c >> i) & 0x1 );
   return buff;
}

/*---------------------------------------------*/
static Bool endsInBz2 ( Char* name )
{
   Int32 n = strlen ( name );
   if (n <= 4) return False;
   return
      (name[n-4] == '.' &&
       name[n-3] == 'b' &&
       name[n-2] == 'z' &&
       name[n-1] == '2');
}


void writeIndexRecord(IdxRecord idxRecord, FILE *stream){
	fwrite(&idxRecord.block_num, sizeof(unsigned long int), 1, stream);
	fwrite(&idxRecord.uncomp_pos, sizeof(unsigned int), 1, stream);
	fwrite(&idxRecord.uncomp_len, sizeof(unsigned int), 1, stream);
	fwrite(&idxRecord.name_len, sizeof(unsigned int), 1, stream);
	fwrite(idxRecord.name, sizeof(char), idxRecord.name_len, stream);
	putc('\0',stream);
}
void appendBlockOffset(off_t start_offset, off_t end_offset, FILE *stream){
	fwrite(&start_offset, sizeof(off_t), 1, stream);
	fwrite(&end_offset, sizeof(off_t), 1, stream);
}

/*---------------------------------------------------*/
/*---                                             ---*/
/*---------------------------------------------------*/

/* This logic isn't really right when it comes to Cygwin. */
#ifdef _WIN32
#  define  BZ_SPLIT_SYM  '\\'  /* path splitter on Windows platform */
#else
#  define  BZ_SPLIT_SYM  '/'   /* path splitter on Unix platform */
#endif

#define BLOCK_HEADER_HI  0x00003141UL
#define BLOCK_HEADER_LO  0x59265359UL

#define BLOCK_ENDMARK_HI 0x00001772UL
#define BLOCK_ENDMARK_LO 0x45385090UL

/* Increase if necessary.  However, a .bz2 file with > 50000 blocks
   would have an uncompressed size of at least 40GB, so the chances
   are low you'll need to up this.
*/
// #define BZ_MAX_HANDLED_BLOCKS 50000
#define BZ_MAX_HANDLED_BLOCKS 50000

/*
MaybeUInt64 bStart [BZ_MAX_HANDLED_BLOCKS];
MaybeUInt64 bEnd   [BZ_MAX_HANDLED_BLOCKS];
MaybeUInt64 rbStart[BZ_MAX_HANDLED_BLOCKS];
MaybeUInt64 rbEnd  [BZ_MAX_HANDLED_BLOCKS];
*/
MaybeUInt64 *bStart;
MaybeUInt64 *bEnd;
MaybeUInt64 *rbStart;
MaybeUInt64 *rbEnd;

Int32 main ( Int32 argc, Char** argv )
{
   bStart=(MaybeUInt64 *)malloc(BZ_MAX_HANDLED_BLOCKS * sizeof(MaybeUInt64));
   bEnd=(MaybeUInt64 *)malloc(BZ_MAX_HANDLED_BLOCKS * sizeof(MaybeUInt64));
   rbStart=(MaybeUInt64 *)malloc(BZ_MAX_HANDLED_BLOCKS * sizeof(MaybeUInt64));
   rbEnd=(MaybeUInt64 *)malloc(BZ_MAX_HANDLED_BLOCKS * sizeof(MaybeUInt64));
   FILE*       inFile;
   FILE*       outFile;
   BitStream*  bsIn, *bsWr;
   Int32       b, wrBlock, currBlock, rbCtr;
   MaybeUInt64 bitsRead;

   UInt32      buffHi, buffLo, blockCRC;
   Char*       p;

   strcpy ( progName, argv[0] );
   inFileName=(char*)malloc(1024);
   inFileName[0]=0;

   char *blockBuff=(char *)malloc(500000*sizeof(char));
   char *pbBuff=blockBuff;
   //inFileName[0] = outFileName[0] = 0;

   fprintf ( stderr, 
             "bzip2recover 1.0.5: extracts blocks from damaged .bz2 files.\n" );

   if (argc != 2) {
      fprintf ( stderr, "%s: usage is `%s damaged_file_name'.\n",
                        progName, progName );
      switch (sizeof(MaybeUInt64)) {
         case 8:
            fprintf(stderr, 
                    "\trestrictions on size of recovered file: None\n");
            break;
         case 4:
            fprintf(stderr, 
                    "\trestrictions on size of recovered file: 512 MB\n");
            fprintf(stderr, 
                    "\tto circumvent, recompile with MaybeUInt64 as an\n"
                    "\tunsigned 64-bit int.\n");
            break;
         default:
            fprintf(stderr, 
                    "\tsizeof(MaybeUInt64) is not 4 or 8 -- "
                    "configuration error.\n");
            break;
      }
      exit(1);
   }

   if (strlen(argv[1]) >= BZ_MAX_FILENAME-20) {
      fprintf ( stderr, 
                "%s: supplied filename is suspiciously (>= %d chars) long.  Bye!\n",
                progName, (int)strlen(argv[1]) );
      exit(1);
   }

   strcpy ( inFileName, argv[1] );

   inFile = fopen ( inFileName, "rb" );
   if (inFile == NULL) {
      fprintf ( stderr, "%s: can't read `%s'\n", progName, inFileName );
      exit(1);
   }

   bsIn = bsOpenReadStream ( inFile );
   fprintf ( stderr, "%s: searching for block boundaries ...\n", progName );

   bitsRead = 0;
   buffHi = buffLo = 0;
   currBlock = 0;
   bStart[currBlock] = 0;

   rbCtr = 0;
   FILE *fpBlockOffset=fopen("blockoffset","w");
   while (True) {
      b = bsGetBit ( bsIn );
      bitsRead++;
      if (b == 2) {
         if (bitsRead >= bStart[currBlock] &&
            (bitsRead - bStart[currBlock]) >= 40) {
            bEnd[currBlock] = bitsRead-1;
            if (currBlock > 0)
               fprintf ( stderr, "   block %d runs from " MaybeUInt64_FMT 
                                 " to :" MaybeUInt64_FMT " (incomplete)\n",
                         currBlock,  bStart[currBlock], bEnd[currBlock] );
         } else
            currBlock--;
         break;
      }
      buffHi = (buffHi << 1) | (buffLo >> 31);
      buffLo = (buffLo << 1) | (b & 1);
      if ( ( (buffHi & 0x0000ffff) == BLOCK_HEADER_HI 
             && buffLo == BLOCK_HEADER_LO)
           || 
           ( (buffHi & 0x0000ffff) == BLOCK_ENDMARK_HI 
             && buffLo == BLOCK_ENDMARK_LO)
         ) {
         if (bitsRead > 49) {
            bEnd[currBlock] = bitsRead-49;
         } else {
            bEnd[currBlock] = 0;
         }
         if (currBlock > 0 &&
	     (bEnd[currBlock] - bStart[currBlock]) >= 130) {
            fprintf ( stderr, "   block %d runs from " MaybeUInt64_FMT 
                              " to " MaybeUInt64_FMT "\n",
                      rbCtr+1,  bStart[currBlock], bEnd[currBlock] );
            rbStart[rbCtr] = bStart[currBlock];
            rbEnd[rbCtr] = bEnd[currBlock];
            rbCtr++;
         }
         if (currBlock >= BZ_MAX_HANDLED_BLOCKS)
            //tooManyBlocks(BZ_MAX_HANDLED_BLOCKS);
	    break;
	 if(currBlock>0)
		 appendBlockOffset(bStart[currBlock],bEnd[currBlock],fpBlockOffset);
         currBlock++;

         bStart[currBlock] = bitsRead;
      }
   }
   fclose(fpBlockOffset);
   bsClose ( bsIn );

   /*-- identified blocks run from 1 to rbCtr inclusive. --*/

   if (rbCtr < 1) {
      fprintf ( stderr,
                "%s: sorry, I couldn't find any block boundaries.\n",
                progName );
      exit(1);
   };

   fprintf ( stderr, "%s: splitting into blocks\n", progName );

   inFile = fopen ( inFileName, "rb" );
   if (inFile == NULL) {
      fprintf ( stderr, "%s: can't open `%s'\n", progName, inFileName );
      exit(1);
   }
   bsIn = bsOpenReadStream ( inFile );

   /*-- placate gcc's dataflow analyser --*/
   blockCRC = 0; bsWr = 0;

   bitsRead = 0;
   outFile = NULL;
   wrBlock = 0;
   int parserState=0;
   char *tagName=(char*)malloc(128);
   char *contentBuff=(char*)malloc(512);
   char *pTagName=tagName;
   char *pContentBuff=contentBuff;
   Bool tagNameReady=False;
   // Manifest file: setttings, pahts and namespaces etc.
   FILE *fpManifest=fopen("manifest","w+");
   char *compressedXMLName=strrchr(argv[1],'/')+1;
   fprintf(fpManifest, "CompressedXML=%s\n",compressedXMLName);

   char *keyName=(char *)malloc(128);
   char *pKeyName=keyName;
   IdxRecord indexRecord;
   int currLen=0;
   FILE *fpIndexRecord=fopen("indexrecord","w");
   while (True) {
      b = bsGetBit(bsIn);
      if (b == 2) break;
      buffHi = (buffHi << 1) | (buffLo >> 31);
      buffLo = (buffLo << 1) | (b & 1);
      if (bitsRead == 47+rbStart[wrBlock]) 
         blockCRC = (buffHi << 16) | (buffLo >> 16);

      if (outFile != NULL && bitsRead >= rbStart[wrBlock]
                          && bitsRead <= rbEnd[wrBlock]) {
         //bsPutBit ( bsWr, b );
	 pbBuff=bsPutBit2(bsWr, pbBuff, b);
      }

      bitsRead++;

      if (bitsRead == rbEnd[wrBlock]+1) {
         if (outFile != NULL) {
            //bsPutUChar ( bsWr, 0x17 ); bsPutUChar ( bsWr, 0x72 );
            pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x17 ); 
	    pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x72 );
            //bsPutUChar ( bsWr, 0x45 ); bsPutUChar ( bsWr, 0x38 );
            pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x45 ); 
	    pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x38 );
            //bsPutUChar ( bsWr, 0x50 ); bsPutUChar ( bsWr, 0x90 );
            pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x50 );
	    pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x90 );
            pbBuff=bsPutUInt32_2 ( bsWr, pbBuff, blockCRC );
            pbBuff=bsClose2 ( bsWr ,pbBuff);
	    char *decompBuff=(char *)malloc(1024000);
	    int sSize=pbBuff-blockBuff;
	    int dSize=1023999;
	    int r=BZ2_bzBuffToBuffDecompress(decompBuff,
						&dSize,
						blockBuff,
						sSize,
						0,0);
	    int j=0,i;
	    if(S_DBG) // S_DBG: defined at beginning of this file.
		    for(;j<dSize;j++)
			    putc((UChar)(decompBuff[j]),stdout);
	     // parserState: 0=ready, 1=tag, 2=text
	    
	    for(;j<dSize;j++){
		    char c=decompBuff[j];
		    switch (parserState)
		    {
			    case 0:
				if(c=='<')
				{
					if(pContentBuff>contentBuff)
					{
						*pContentBuff='\0';
					}
					parserState=1;
				    	pTagName=tagName;
					tagNameReady=False;
				}
				else if(strcmp(tagName,"namespace")==0)
				{
					*pContentBuff=c;
					pContentBuff++;
				}
				else if(strcmp(tagName,"title")==0)
				{
					*pContentBuff=c;
					pContentBuff++;
				}
				else if(strcmp(tagName,"text")==0)
				{
					currLen++;
				}
				break;
			    case 1:
				
				if(c==' '||c=='>')
				{
					tagNameReady=True;
					*pTagName='\0';
					if(c==' ')
					{
						if(strcmp(tagName,"namespace")==0)
						{
							if(S_DBG)
							fprintf(stderr, "namespace:");
							pKeyName=keyName;
							parserState=3;
							break;
						}
					}
					else if(c=='>')
					{
						if(tagName[0]=='/')
						{
							//end of tag
							if(strcmp(tagName,"/namespace")==0)
							{
								if(S_DBG)
								fprintf(stderr,"%s\n",contentBuff);
								
								fprintf(fpManifest,"%s\n",contentBuff);
								pContentBuff=contentBuff;
							}else if(strcmp(tagName,"/title")==0)
							{
								if(S_DBG)
								fprintf(stderr, "title: %s\n",contentBuff);
								indexRecord.name=strdup(contentBuff);
								indexRecord.name_len=strlen(contentBuff);
								// article title
							}else if(strcmp(tagName,"/text")==0)
							{
								if(S_DBG)
								fprintf(stderr, "end of text\n");
								indexRecord.uncomp_len=currLen;
								// article length
							}
							else if(strcmp(tagName,"/page")==0)
							{
								if(S_DBG)
								fprintf(stderr, "page end\n");
								writeIndexRecord(indexRecord, fpIndexRecord);
								// write article record
							}
						}
						else if(strcmp(tagName,"text")==0)
						{
							// article position
							if(S_DBG){
								fprintf(stderr, "article start:");
								for(i=j;i<j+20 && i<dSize;i++)
									putc( decompBuff[i], stderr);
								putc('\n',stderr);
							}
							indexRecord.uncomp_pos=j+1;
							indexRecord.block_num=wrBlock;
							if(S_DBG)
								fprintf(stderr,"block_num:%d\n",wrBlock);
							//indexRecord.block_start_bit_offset=rbStart[wrBlock];
							//indexRecord.block_end_bit_offset=rbEnd[wrBlock];
						}
						else if(strcmp(tagName,"page")==0)
						{
							// new index record
							if(S_DBG)
							fprintf(stderr, "page start\n");
						}
						pContentBuff=contentBuff;
						parserState=0;
						break;
					}
				}
				else if(!tagNameReady)
				{
					*pTagName=c;
					pTagName++;
				}else
				{
					//tagNameReady is true

				}

				break;
			    case 2:
				break;

			    case 3://namespace prop.

				if(pKeyName-keyName>=5 && c=='"')
				{
					//key end;
					*pKeyName='"';
					pKeyName++;
					break;
				}
				if(c=='/'){
					parserState=1;
					pKeyName=keyName;
					break;
				}
				else if(c=='>'){
					*pKeyName=0;
					if(strcmp(keyName,"key=\"-2\"")==0)
					{
						fprintf(fpManifest,"Media=");
					}else if(strcmp(keyName,"key=\"-1\"")==0)
					{
						fprintf(fpManifest,"Special=");
					}else if(strcmp(keyName,"key=\"0\"")==0)
					{
						fprintf(fpManifest,"Main=");
					}else if(strcmp(keyName,"key=\"1\"")==0)
					{
						fprintf(fpManifest,"Talk=");
					}else if(strcmp(keyName,"key=\"2\"")==0)
					{
						fprintf(fpManifest,"User=");
					}else if(strcmp(keyName,"key=\"3\"")==0)
					{
						fprintf(fpManifest,"User talk=");
					}else if(strcmp(keyName,"key=\"4\"")==0)
					{
						fprintf(fpManifest,"Project=");
					}else if(strcmp(keyName,"key=\"5\"")==0)
					{
						fprintf(fpManifest,"Project talk=");
					}else if(strcmp(keyName,"key=\"6\"")==0)
					{
						fprintf(fpManifest,"File=");
					}else if(strcmp(keyName,"key=\"7\"")==0)
					{
						fprintf(fpManifest,"File talk=");
					}else if(strcmp(keyName,"key=\"8\"")==0)
					{
						fprintf(fpManifest,"MediaWiki=");
					}else if(strcmp(keyName,"key=\"9\"")==0)
					{
						fprintf(fpManifest,"MediaWiki talk=");
					}else if(strcmp(keyName,"key=\"10\"")==0)
					{
						fprintf(fpManifest,"Template=");
					}else if(strcmp(keyName,"key=\"11\"")==0)
					{
						fprintf(fpManifest,"Template talk=");
					}else if(strcmp(keyName,"key=\"12\"")==0)
					{
						fprintf(fpManifest,"Help=");
					}else if(strcmp(keyName,"key=\"13\"")==0)
					{
						fprintf(fpManifest,"Help talk=");
					}else if(strcmp(keyName,"key=\"14\"")==0)
					{
						fprintf(fpManifest,"Category=");
					}else if(strcmp(keyName,"key=\"15\"")==0)
					{
						fprintf(fpManifest,"Category talk=");
					}else if(strncmp(keyName, "key=\"",5)==0)
					{
						
						char *namespaceName=malloc(strlen(keyName)-5);
						memcpy(namespaceName,keyName+5, strlen(keyName)-5);
						fprintf(fpManifest,"Namespace:%s=",namespaceName);
						free(namespaceName);
					}

					//parserState=1;
					pKeyName=keyName;
					//tagNameReady=False;
					//pTagName=tagName;
					j--;
					parserState=1;
				}
				else
				{
					*pKeyName=c;
					pKeyName++;
				}
				break;
						
		    }
	    }
		    
	    fprintf(stderr,"status:%d, dSize:%d \n",r,dSize);
	    fprintf(stderr,"decompBuff[dSize-1]=%x",decompBuff[dSize-1]);
	    
	    free(decompBuff);

	    fprintf(stderr,"^^^^^^block %d end $$$$$$$\n",wrBlock);
	    pbBuff=blockBuff;
         }
         if (wrBlock >= rbCtr) break;
	 //if (wrBlock >=)break;
         wrBlock++;
      } else
      if (bitsRead == rbStart[wrBlock]) {
         /* Create the output file name, correctly handling leading paths. 
            (31.10.2001 by Sergey E. Kusikov) */
         Char* split;
         Int32 ofs, k;
	 /*
         for (k = 0; k < BZ_MAX_FILENAME; k++) 
            outFileName[k] = 0;
         strcpy (outFileName, inFileName);
         split = strrchr (outFileName, BZ_SPLIT_SYM);
         if (split == NULL) {
            split = outFileName;
         } else {
            ++split;
	 }*/
	 /* Now split points to the start of the basename. */
	 /*
         ofs  = split - outFileName;
         sprintf (split, "rec%5d", wrBlock+1);
         for (p = split; *p != 0; p++) if (*p == ' ') *p = '0';
         strcat (outFileName, inFileName + ofs);
         if ( !endsInBz2(outFileName)) strcat ( outFileName, ".bz2" );
	*/
         if(outFile!=NULL)
         {
	    fclose(outFile);
            outFile=NULL;
         }
	 outFileName="tmp.bz2";
         fprintf ( stderr, "   writing block %d to `%s' ...\n",
                           wrBlock+1, outFileName );

         outFile = fopen ( outFileName, "wb" );
         if (outFile == NULL) {
            fprintf ( stderr, "%s: can't write `%s'\n",
                      progName, outFileName );
            exit(1);
         }
         bsWr = bsOpenWriteStream2 ();
         //bsPutUChar ( bsWr, BZ_HDR_B );    
         pbBuff=bsPutUChar2 ( bsWr, pbBuff, BZ_HDR_B );    
         //bsPutUChar ( bsWr, BZ_HDR_Z );    
         pbBuff=bsPutUChar2 ( bsWr, pbBuff, BZ_HDR_Z );    
         //bsPutUChar ( bsWr, BZ_HDR_h );    
         pbBuff=bsPutUChar2 ( bsWr, pbBuff, BZ_HDR_h );    
         //bsPutUChar ( bsWr, BZ_HDR_0 + 9 );
         pbBuff=bsPutUChar2 ( bsWr, pbBuff, BZ_HDR_0 + 9 );

         pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x31 ); 
	 pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x41 );
         pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x59 ); 
	 pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x26 );
         pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x53 ); 
	 pbBuff=bsPutUChar2 ( bsWr, pbBuff, 0x59 );
      }
   }
   fclose(fpManifest);
   fclose(fpIndexRecord);
   fprintf ( stderr, "%s: finished\n", progName );
   return 0;
}



/*-----------------------------------------------------------*/
/*--- end                                  bzip2recover.c ---*/
/*-----------------------------------------------------------*/

