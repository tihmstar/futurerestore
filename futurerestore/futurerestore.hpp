//
//  futurerestore.hpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#ifndef futurerestore_hpp
#define futurerestore_hpp

//make sure WIN32 is defined if compiling for windows
#if defined _WIN32 || defined __CYGWIN__
#ifndef WIN32
#define WIN32
#endif
#endif

#include <stdio.h>
#include <functional>
#include <vector>
#include "idevicerestore.h"
#include <jssy.h>
#include <plist/plist.h>

using namespace std;

template <typename T>
class ptr_smart {
    std::function<void(T)> _ptr_free = NULL;
public:
    T _p;
    ptr_smart(T p, function<void(T)> ptr_free){static_assert(is_pointer<T>(), "error: this is for pointers only\n"); _p = p;_ptr_free = ptr_free;}
    ptr_smart(T p){_p = p;}
    ptr_smart(){_p = NULL;}
    ptr_smart(ptr_smart &&p){ _p = p._p; _ptr_free = p._ptr_free; p._p = NULL; p._ptr_free = NULL;}
    ptr_smart& operator =(ptr_smart &&p){_p = p._p; _ptr_free = p._ptr_free; p._p = NULL; p._ptr_free = NULL; return *this;}
    T operator =(T p){ _p = p; return _p;}
    T operator =(T &p){_p = p; p = NULL; return _p;}
    T *operator&(){return &_p;}
    explicit operator const T() const {return _p;}
    operator const void*() const {return _p;}
    ~ptr_smart(){if (_p) (_ptr_free) ? _ptr_free(_p) : free((void*)_p);}
};

class futurerestore {
    struct idevicerestore_client_t* _client;
    char *_ibootBuild = NULL;
    bool _didInit = false;
    vector<plist_t> _aptickets;
    vector<pair<char *, size_t>>_im4ms;
    int _foundnonce = -1;
    bool _isUpdateInstall = false;
    bool _isPwnDfu = false;
    
    char *_firmwareJson = NULL;
    jssytok_t *_firmwareTokens = NULL;;
    char *__latestManifest = NULL;
    char *__latestFirmwareUrl = NULL;
    
    plist_t _sepbuildmanifest = NULL;
    plist_t _basebandbuildmanifest = NULL;
    
    const char *_basebandPath = NULL;;
    const char *_sepbuildmanifestPath = NULL;
    const char *_basebandbuildmanifestPath = NULL;
    
    bool _enterPwnRecoveryRequested = false;
    bool _rerestoreiOS9 = false;
    //methods
    void enterPwnRecovery(plist_t build_identity, std::string bootargs = "");
    
public:
    futurerestore(bool isUpdateInstall = false, bool isPwnDfu = false);
    bool init();
    int getDeviceMode(bool reRequest);
    uint64_t getDeviceEcid();
    void putDeviceIntoRecovery();
    void setAutoboot(bool val);
    void exitRecovery();
    void waitForNonce();
    void waitForNonce(vector<const char *>nonces, size_t nonceSize);
    void loadAPTickets(const vector<const char *> &apticketPaths);
    char *getiBootBuild();
    
    plist_t nonceMatchesApTickets();
    std::pair<const char *,size_t> nonceMatchesIM4Ms();

    void loadFirmwareTokens();
    const char *getDeviceModelNoCopy();
    const char *getDeviceBoardNoCopy();
    char *getLatestManifest();
    char *getLatestFirmwareUrl();
    void loadLatestBaseband();
    void loadLatestSep();
    
    void setSepManifestPath(const char *sepManifestPath);
    void setBasebandManifestPath(const char *basebandManifestPath);
    void loadSep(const char *sepPath);
    void setBasebandPath(const char *basebandPath);
    bool isUpdateInstall(){return _isUpdateInstall;};
    
    plist_t sepManifest(){return _sepbuildmanifest;};
    plist_t basebandManifest(){return _basebandbuildmanifest;};
    const char *sepManifestPath(){return _sepbuildmanifestPath;};
    const char *basebandManifestPath(){return _basebandbuildmanifestPath;};
    bool is32bit(){return !is_image4_supported(_client);};
    
    uint64_t getBasebandGoldCertIDFromDevice();
    
    void doRestore(const char *ipsw);
    int doJustBoot(const char *ipsw, std::string bootargs = "");
    
    ~futurerestore();
    
    static std::pair<const char *,size_t> getRamdiskHashFromSCAB(const char* scab, size_t scabSize);
    static std::pair<const char *,size_t> getNonceFromSCAB(const char* scab, size_t scabSize);
    static uint64_t getEcidFromSCAB(const char* scab, size_t scabSize);
    static plist_t loadPlistFromFile(const char *path);
    static void saveStringToFile(const char *str, const char *path);
    static char *getPathOfElementInManifest(const char *element, const char *manifeststr, const char *model, int isUpdateInstall);
    static std::string getGeneratorFromSHSH2(const plist_t shsh2);
};

#endif /* futurerestore_hpp */
