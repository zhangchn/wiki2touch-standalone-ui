#ifndef __INDEXER_H_INCLUDED___
#define __INDEXER_H_INCLUDED___

#define _FILE_OFFSET_BITS 64   // needed on Linux

#ifdef _WIN32
// MS defines off_t always as 32 bit, so we can't use it :-(
typedef unsigned __int64 offset_t;
#else
// on other systems (OS X, Linux), off_t should be 64 bit (which is what we need)
// and fseeko/ftello should be present
#include <sys/types.h>
typedef off_t offset_t ;
#endif

#pragma pack(1)
typedef struct
{
	char languageCode[2];				// 2 bytes
	unsigned int numberOfArticles;		// 4 bytes
	offset_t	titlesPos;				// 8 bytes
	offset_t	indexPos_0;				// 8 bytes
	offset_t	indexPos_1;				// 8 bytes; the second one has discritcs removed or traditional chineses chars are converted to simpified chineses chars
	unsigned char version;				// 1 byte
	char reserved1[1];					// 1 byte
	char imageNamespace[32];			// namespace prefix for images   (without the colon)
	char templateNamespace[32];			// namespace prefix for template (without the colon)
	char reserved2[160];				// for future use
} FILEHEADER;
#pragma pack()

struct IndexStatus
{
  int articlesWritten;
  int articlesSkipped;
  int blockCount;
  int progress;
  offset_t bytesTotal;
  offset_t bytesSkipped;
};

struct IndexOpts
{
  bool extractImagesOnly;
  bool dontExtractImages;
  bool addIndexWithDiacriticsRemoved;
  bool removedUnusedArticles;
};

int start_indexer(const char* src, const char* target, const char* imagelist, IndexStatus *pStats, IndexOpts *pOpt);


#endif
