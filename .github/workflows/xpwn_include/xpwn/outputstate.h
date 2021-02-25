#ifndef OUTPUTSTATE_H
#define OUTPUTSTATE_H

#include <abstractfile.h>

typedef struct OutputState {
	char* fileName;
	void* buffer;
	size_t bufferSize;
	char* tmpFileName;
	struct OutputState* next;
	struct OutputState* prev;
} OutputState;

#ifdef __cplusplus
extern "C" {
#endif
	char* createTempFile();
	void addToOutput2(OutputState** state, const char* fileName, void* buffer, const size_t bufferSize, char* tmpFileName);
	void addToOutput(OutputState** state, const char* fileName, void* buffer, const size_t bufferSize);
	void removeFileFromOutputState(OutputState** state, const char* fileName, int exact);
	AbstractFile* getFileFromOutputState(OutputState** state, const char* fileName);
	AbstractFile* getFileFromOutputStateForOverwrite(OutputState** state, const char* fileName);
	AbstractFile* getFileFromOutputStateForReplace(OutputState** state, const char* fileName);
	void writeOutput(OutputState** state, char* ipsw);
	void releaseOutput(OutputState** state);
	OutputState* loadZip2(const char* ipsw, int useMemory);
	void loadZipFile2(const char* ipsw, OutputState** output, const char* file, int useMemory);
	OutputState* loadZip(const char* ipsw);
	void loadZipFile(const char* ipsw, OutputState** output, const char* file);
#ifdef __cplusplus
}
#endif

#endif

