TARGET = futurerestore_tool
INSTALLTARGET = futurerestore
CXX = g++
CC = gcc
INCLUDE += -I/usr/local/include -I futurerestore -I /opt/local/include -I external/idevicerestore/src -I external/tsschecker/tsschecker -I external/img4tool/img4tool
CFLAGS += $(INCLUDE) -Wall -std=c11 -D HAVE_CONFIG_H=1 
CXXFLAGS += $(INCLUDE) -Wall -std=c++11 

LDFLAGS += -L/opt/local/lib -L/usr/local/lib -lirecovery -limobiledevice -lcrypto -lcurl -lplist -lc -lc++ -lzip -lfragmentzip -lz

OBJECTS += futurerestore.o
SRC_DIR = futurerestore
OBJECTS_COMBINED = $(OBJECTS:%.o=$(SRC_DIR)/%.o)

DEPOBJECTS_TSSCHECKER += tsschecker.o jsmn.o download.o tss.o
DEPOBJECTS_IMG4TOOL += img4tool.o img4.o
DEPOBJECTS_IDEVICERESTORE += common.o img3.o img4.o download.o asr.o fdr.o fls.o dfu.o ipsw.o limera1n.o locking.o mbn.o normal.o recovery.o restore.o socket.o thread.o

PATH_TSSCHECKER = external/tsschecker/tsschecker
PATH_IMG4TOOL = external/img4tool/img4tool
PATH_IDEVICERESTORE = external/idevicerestore/src

DEPOBJECTS_TSSCHECKER_COMBINED = $(DEPOBJECTS_TSSCHECKER:%.o=$(PATH_TSSCHECKER)/%.o)
DEPOBJECTS_IMG4TOOL_COMBINED = $(DEPOBJECTS_IMG4TOOL:%.o=$(PATH_IMG4TOOL)/%.o)
DEPOBJECTS_IDEVICERESTORE_COMBINED = $(DEPOBJECTS_IDEVICERESTORE:%.o=$(PATH_IDEVICERESTORE)/%.o)


all : $(TARGET)


ALLDEPS = $(DEPOBJECTS_TSSCHECKER_COMBINED) $(DEPOBJECTS_IMG4TOOL_COMBINED) $(DEPOBJECTS_IDEVICERESTORE_COMBINED) $(PATH_IDEVICERESTORE)/idevicerestore.o $(OBJECTS_COMBINED)
$(TARGET) : $(ALLDEPS)
		$(CXX) $(CXXFLAGS) $(LDFLAGS) $(ALLDEPS) $(SRC_DIR)/main.cpp -o $(TARGET)
		@echo "Successfully built $(TARGET)"

$(OBJECTS_COMBINED) : $(OBJECTS_COMBINED:%.o=%.cpp)
		$(CXX) $(CXXFLAGS)  $< -c -o $@

$(DEPOBJECTS_TSSCHECKER_COMBINED) :
		CFLAGS='-D HAVE_CONFIG_H=1 -I ../../futurerestore' make -C external/tsschecker $(DEPOBJECTS_TSSCHECKER:%.o=tsschecker/%.o)

$(DEPOBJECTS_IDEVICERESTORE_COMBINED) : $(DEPOBJECTS_IDEVICERESTORE_COMBINED:%.o=%.c)
		$(CC) $(CFLAGS)  $(@:%.o=%.c) -c -o $@

$(PATH_IDEVICERESTORE)/idevicerestore.o : $(PATH_IDEVICERESTORE)/idevicerestore.c
		$(CC) $(CFLAGS) -c $(PATH_IDEVICERESTORE)/idevicerestore.c -o $(PATH_IDEVICERESTORE)/idevicerestore.o

$(PATH_IMG4TOOL)/img4tool.o :
		CFLAGS='-D HAVE_CONFIG_H=1 -I ../../futurerestore' make -C external/img4tool objects

install :
		cp $(TARGET) /usr/local/bin/$(INSTALLTARGET)
		@echo "Installed $(INSTALLTARGET)"
clean :
		make -C external/tsschecker clean
		make -C external/img4tool clean
		rm -rf external/idevicerestore/src/*.o
		rm -rf futurerestore/*.o $(TARGET)
