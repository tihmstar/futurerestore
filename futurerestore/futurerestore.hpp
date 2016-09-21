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
#include "idevicerestore.h"
#include "jsmn.h"

using namespace std;

class futurerestore {
    struct idevicerestore_client_t* _client;
    bool _didInit = false;
    plist_t _apticket = NULL;
    char *_im4m = NULL;
    
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
    void waitForNonce(const char *nonce);
    void loadAPTicket(const char *apticketPath);
    void loadAPTicket(string apticketPath);
    
    bool nonceMatchesApTicket();

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
    
    
    int doRestore(const char *ipsw, bool noerase);
    
    ~futurerestore();
    
    static char *getNonceFromIM4M(const char* im4m);
    static char *getNonceFromAPTicket(const char* apticketPath);
    static plist_t loadPlistFromFile(const char *path);
    static void saveStringToFile(const char *str, const char *path);
    static char *getPathOfElementInManifest(const char *element, const char *manifeststr);

};




#endif /* futurerestore_hpp */
