#include <inttypes.h>
#include <curl/curl.h>

typedef struct EndOfCD {
	uint32_t signature;
	uint16_t diskNo;
	uint16_t CDDiskNo;
	uint16_t CDDiskEntries;
	uint16_t CDEntries;
	uint32_t CDSize;
	uint32_t CDOffset;
	uint16_t lenComment;
} __attribute__ ((packed)) EndOfCD;

typedef struct CDFile {
	uint32_t signature;
	uint16_t version;
	uint16_t versionExtract;
	uint16_t flags;
	uint16_t method;
	uint16_t modTime;
	uint16_t modDate;
	uint32_t crc32;
	uint32_t compressedSize;
	uint32_t size;
	uint16_t lenFileName;
	uint16_t lenExtra;
	uint16_t lenComment;
	uint16_t diskStart;
	uint16_t internalAttr;
	uint32_t externalAttr;
	uint32_t offset;
} __attribute__ ((packed)) CDFile;

typedef struct LocalFile {
	uint32_t signature;
	uint16_t versionExtract;
	uint16_t flags;
	uint16_t method;
	uint16_t modTime;
	uint16_t modDate;
	uint32_t crc32;
	uint32_t compressedSize;
	uint32_t size;
	uint16_t lenFileName;
	uint16_t lenExtra;
} __attribute__ ((packed)) LocalFile;

typedef struct ZipInfo ZipInfo;

typedef void (*PartialZipProgressCallback)(ZipInfo* info, CDFile* file, size_t progress);

struct ZipInfo {
	char* url;
	uint64_t length;
	CURL* hIPSW;
	char* centralDirectory;
	size_t centralDirectoryRecvd;
	EndOfCD* centralDirectoryDesc;
	char centralDirectoryEnd[0xffff + sizeof(EndOfCD)];
	size_t centralDirectoryEndRecvd;
	PartialZipProgressCallback progressCallback;
};

#ifdef __cplusplus
extern "C" {
#endif

	ZipInfo* PartialZipInit(const char* url);

	CDFile* PartialZipFindFile(ZipInfo* info, const char* fileName);

	CDFile* PartialZipListFiles(ZipInfo* info);

	unsigned char* PartialZipGetFile(ZipInfo* info, CDFile* file);

	void PartialZipRelease(ZipInfo* info);
	
	void PartialZipSetProgressCallback(ZipInfo* info, PartialZipProgressCallback progressCallback);

#ifdef __cplusplus
}
#endif

