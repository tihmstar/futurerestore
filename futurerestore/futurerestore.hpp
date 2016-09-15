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

using namespace std;

class futurerestore {
    struct idevicerestore_client_t* _client;
    bool _didInit;
    plist_t _apticket;
    char *_im4m;
    
    const char *_sepManifestPath;
    const char *_basebandManifestPath;
    const char *_sepPath;
    const char *_basebandPath;
    
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

    
    void setSepManifestPath(const char *sepManifestPath){_sepManifestPath = sepManifestPath;};
    void setBasebandManifestPath(const char *basebandManifestPath){_basebandManifestPath = basebandManifestPath;};
    void setSepPath(const char *sepPath){_sepPath = sepPath;};
    void setBasebandPath(const char *basebandPath){_basebandPath = basebandPath;};
    
    int doRestore(const char *ipsw, bool noerase);
    
    ~futurerestore();
    
    static char *getNonceFromIM4M(const char* im4m);
    static char *getNonceFromAPTicket(const char* apticketPath);
    static plist_t loadPlistFromFile(const char *path);

};




#endif /* futurerestore_hpp */
