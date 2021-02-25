#ifndef IMG3_H
#define IMG3_H

#include <stdint.h>
#include "common.h"
#include <abstractfile.h>
#include <openssl/aes.h>

#define IMG3_MAGIC 0x496d6733
#define IMG3_DATA_MAGIC 0x44415441
#define IMG3_VERS_MAGIC 0x56455253
#define IMG3_SEPO_MAGIC 0x5345504f
#define IMG3_SCEP_MAGIC 0x53434550
#define IMG3_BORD_MAGIC 0x424f5244
#define IMG3_BDID_MAGIC 0x42444944
#define IMG3_SHSH_MAGIC 0x53485348
#define IMG3_CERT_MAGIC 0x43455254
#define IMG3_KBAG_MAGIC 0x4B424147
#define IMG3_TYPE_MAGIC 0x54595045
#define IMG3_ECID_MAGIC 0x45434944

#define IMG3_SIGNATURE IMG3_MAGIC

typedef struct Img3Element Img3Element;
typedef struct Img3Info Img3Info;

typedef void (*WriteImg3)(AbstractFile* file, Img3Element* element, Img3Info* info);
typedef void (*FreeImg3)(Img3Element* element);

typedef struct AppleImg3Header {
	uint32_t magic;
	uint32_t size;
	uint32_t dataSize;
}__attribute__((__packed__)) AppleImg3Header;

typedef struct AppleImg3RootExtra {
	uint32_t shshOffset;
	uint32_t name;
}__attribute__((__packed__)) AppleImg3RootExtra;

typedef struct AppleImg3RootHeader {
	AppleImg3Header base;
	AppleImg3RootExtra extra;
}__attribute__((__packed__)) AppleImg3RootHeader;

typedef struct AppleImg3KBAGHeader {
 
  uint32_t key_modifier;		// key modifier, can be 0 or 1 	
  uint32_t key_bits;			// number of bits in the key, can be 128, 192 or 256 (it seems only 128 is supported in current iBoot)
} AppleImg3KBAGHeader;

struct Img3Element
{
	AppleImg3Header* header;
	WriteImg3 write;
	FreeImg3 free;
	void* data;
	struct Img3Element* next;
};

struct Img3Info {
	AbstractFile* file;
	Img3Element* root;
	Img3Element* data;
	Img3Element* cert;
	Img3Element* kbag;
	Img3Element* type;
	int encrypted;
	AES_KEY encryptKey;
	AES_KEY decryptKey;
	uint8_t iv[16];
	size_t offset;
	uint32_t replaceDWord;
	char dirty;
	char exploit24k;
	char exploitN8824k;
	char decryptLast;
};

#ifdef __cplusplus
extern "C" {
#endif
	AbstractFile* createAbstractFileFromImg3(AbstractFile* file);
	AbstractFile* duplicateImg3File(AbstractFile* file, AbstractFile* backing);
	void replaceCertificateImg3(AbstractFile* file, AbstractFile* certificate);
	void exploit24kpwn(AbstractFile* file);
	void exploitN8824kpwn(AbstractFile* file);
#ifdef __cplusplus
}
#endif


#endif

