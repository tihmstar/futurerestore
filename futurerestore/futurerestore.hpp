//
//  futurerestore.hpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#ifndef futurerestore_hpp
#define futurerestore_hpp

#include "config.h"
#include <stdio.h>
#include <functional>
#include <vector>
#include "idevicerestore.h"
#include "jsmn.h"

using namespace std;

template <typename T>
class ptr_smart {
    std::function<void(T)> _ptr_free = NULL;
public:
    T _p;
    ptr_smart(T p, function<void(T)> ptr_free){static_assert(is_pointer<T>(), "error: this is for pointers only\n"); _p = p;_ptr_free = ptr_free;}
    ptr_smart(T p){_p = p;}
    ptr_smart(){_p = NULL;}
    T operator=(T p){return _p = p;}
    T *operator&(){return &_p;}
    ~ptr_smart(){if (_p) (_ptr_free) ? _ptr_free(_p) : free((void*)_p);}
};

class futurerestore {
    struct idevicerestore_client_t* _client;
    bool _didInit = false;
    vector<plist_t> _aptickets;
    vector<char *>_im4ms;
    int _foundnonce = -1;
    
    char *_firmwareJson = NULL;
    jsmntok_t *_firmwareTokens = NULL;;
    char *__latestManifest = NULL;
    char *__latestFirmwareUrl = NULL;
    
    const char *_sepManifestPath = NULL;
    const char *_basebandManifestPath = NULL;
    const char *_sepPath = NULL;
    const char *_basebandPath = NULL;
    
    
public:
    futurerestore();
    bool init();
    int getDeviceMode(bool reRequest);
    uint64_t getDeviceEcid();
    void putDeviceIntoRecovery();
    void setAutoboot(bool val);
    void waitForNonce();
    void waitForNonce(vector<const char *>nonces, size_t nonceSize);
    void loadAPTickets(const vector<const char *> &apticketPaths);
    
    plist_t nonceMatchesApTickets();

    void loadFirmwareTokens();
    const char *getConnectedDeviceModel();
    char *getLatestManifest();
    char *getLatestFirmwareUrl();
    void loadLatestBaseband();
    void loadLatestSep();
    
    void setSepManifestPath(const char *sepManifestPath){_sepManifestPath = sepManifestPath;};
    void setBasebandManifestPath(const char *basebandManifestPath){_basebandManifestPath = basebandManifestPath;};
    void setSepPath(const char *sepPath){_sepPath = sepPath;};
    void setBasebandPath(const char *basebandPath){_basebandPath = basebandPath;};
    
    
    const char *sepManifestPath(){return _sepManifestPath;};
    const char *basebandManifestPath(){return _basebandManifestPath;};
    const char *sepPath(){return _sepPath;};
    const char *basebandPath(){return _basebandPath;};
    
    uint64_t getBasebandGoldCertIDFromDevice();
    const char *getDeviceModelNoCopy();
    
    int doRestore(const char *ipsw, bool noerase);
    
    ~futurerestore();

    static char *getNonceFromIM4M(const char* im4m, size_t *nonceSize);
    static char *getNonceFromAPTicket(const char* apticketPath);
    static plist_t loadPlistFromFile(const char *path);
    static void saveStringToFile(const char *str, const char *path);
    static char *getPathOfElementInManifest(const char *element, const char *manifeststr, const char *model, int isUpdateInstall);

};




#endif /* futurerestore_hpp */
