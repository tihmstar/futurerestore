#ifndef PLIST_H
#define PLIST_H

enum DictTypes {
	DictionaryType = 1,
	ArrayType,
	StringType,
	DataType,
	IntegerType,
	BoolType
};

typedef struct DictValue {
	int type;
	char* key;
	struct DictValue* next;
	struct DictValue* prev;
} DictValue;

typedef struct StringValue {
	DictValue dValue;
	char* value;
} StringValue;

typedef struct DataValue {
	DictValue dValue;
	int len;
	unsigned char* value;
} DataValue;

typedef struct IntegerValue {
	DictValue dValue;
	int value;
} IntegerValue;

typedef struct BoolValue {
	DictValue dValue;
	char value;
} BoolValue;

typedef struct ArrayValue {
	DictValue dValue;
	int size;
	DictValue** values;
} ArrayValue;

typedef struct Dictionary {
	DictValue dValue;
	DictValue* values;
} Dictionary;

typedef struct Tag {
	char* name;
	char* xml;
} Tag;

#ifdef __cplusplus
extern "C" {
#endif
	void createArray(ArrayValue* myself, char* xml);
	void createDictionary(Dictionary* myself, char* xml);
	void releaseArray(ArrayValue* myself);
	void releaseDictionary(Dictionary* myself);
	char* getXmlFromArrayValue(ArrayValue* myself, int tabsCount);
	char* getXmlFromDictionary(Dictionary* myself, int tabsCount);
	Dictionary* createRoot(char* xml);
	char* getXmlFromRoot(Dictionary* root);
	DictValue* getValueByKey(Dictionary* myself, const char* key);
	void addStringToArray(ArrayValue* array, char* str);
	void removeKey(Dictionary* dict, char* key);
	void addBoolToDictionary(Dictionary* dict, const char* key, int value);
	void addIntegerToDictionary(Dictionary* dict, const char* key, int value);
	void addValueToDictionary(Dictionary* dict, const char* key, DictValue* value);
	void unlinkValueFromDictionary(Dictionary* myself, DictValue *value);
	void prependToArray(ArrayValue* array, DictValue* curValue);
	void releaseArrayEx(ArrayValue* myself, int k);
#ifdef __cplusplus
}
#endif

#endif

