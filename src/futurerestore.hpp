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
#include <array>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <zip.h>
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
    char *_ibootBuild = nullptr;
    bool _didInit = false;
    vector<plist_t> _aptickets;
    vector<pair<char *, size_t>>_im4ms;
    int _foundnonce = -1;
    bool _isUpdateInstall = false;
    bool _isPwnDfu = false;
    bool _noIBSS = false;
    bool _setNonce = false;
    bool _serial = false;
    bool _noRestore = false;
    
    char *_firmwareJson = nullptr;
    char *_betaFirmwareJson = nullptr;
    jssytok_t *_firmwareTokens = nullptr;;
    jssytok_t *_betaFirmwareTokens = nullptr;
    char *_latestManifest = nullptr;
    char *_latestFirmwareUrl = nullptr;
    bool _useCustomLatest = false;
    bool _useCustomLatestBuildID = false;
    bool _useCustomLatestBeta = false;
    std::string _customLatest;
    std::string _customLatestBuildID;

    plist_t _sepbuildmanifest = nullptr;
    plist_t _basebandbuildmanifest = nullptr;

    std::string _ramdiskPath;
    std::string _kernelPath;
    std::string _sepPath;
    std::string _sepManifestPath;
    std::string _basebandPath;
    std::string _basebandManifestPath;

    const char *_custom_nonce = nullptr;
    const char *_boot_args = nullptr;

    bool _noCache = false;
    bool _skipBlob = false;

    bool _enterPwnRecoveryRequested = false;
    bool _rerestoreiOS9 = false;
    //methods
    void enterPwnRecovery(plist_t build_identity, std::string bootargs);

public:
    futurerestore(bool isUpdateInstall = false, bool isPwnDfu = false, bool noIBSS = false, bool setNonce = false, bool serial = false, bool noRestore = false);
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
    std::string getSepManifestPath(){return _sepManifestPath;}
    std::string getBasebandManifestPath(){return _basebandManifestPath;}
    void downloadLatestRose();
    void downloadLatestSE();
    void downloadLatestSavage();
    void downloadLatestVeridian();
    void downloadLatestFirmwareComponents();
    void downloadLatestBaseband();
    void downloadLatestSep();
    
    void loadSepManifest(std::string sepManifestPath);
    void loadBasebandManifest(std::string basebandManifestPath);
    void loadRose(std::string rosePath);
    void loadSE(std::string sePath);
    void loadSavage(std::array<std::string, 6> savagePaths);
    void loadVeridian(std::string veridianDGMPath, std::string veridianFWMPath);
    void loadRamdisk(std::string ramdiskPath);
    void loadKernel(std::string kernelPath);
    void loadSep(std::string sepPath);
    void loadBaseband(std::string basebandPath);

    void setCustomLatest(std::string version){_customLatest = version; _useCustomLatest = true;}
    void setCustomLatestBuildID(std::string version, bool beta){_customLatestBuildID = version; _useCustomLatest = false; _useCustomLatestBuildID = true; _useCustomLatestBeta = beta;}
    void setSepPath(std::string sepPath) {_sepPath = sepPath;}
    void setSepManifestPath(std::string sepManifestPath) {_sepManifestPath = sepManifestPath;}
    void setRamdiskPath(std::string ramdiskPath) {_ramdiskPath = ramdiskPath;}
    void setKernelPath(std::string kernelPath) {_kernelPath = kernelPath;}
    void setBasebandPath(std::string basebandPath) {_basebandPath = basebandPath;}
    void setBasebandManifestPath(std::string basebandManifestPath) {_basebandManifestPath = basebandManifestPath;}
    void setNonce(const char *custom_nonce){_custom_nonce = custom_nonce;};
    void setBootArgs(const char *boot_args){_boot_args = boot_args;};
    void disableCache(){_noCache = true;};
    void skipBlobValidation(){_skipBlob = true;};

    bool is32bit(){return !is_image4_supported(_client);};
    
    uint64_t getBasebandGoldCertIDFromDevice();
    
    void doRestore(const char *ipsw);

#ifdef __APPLE__
    static int findProc(const char *procName, bool load);
    void daemonManager(bool load);
#endif

    ~futurerestore();
    
    static std::pair<const char *,size_t> getRamdiskHashFromSCAB(const char* scab, size_t scabSize);
    static std::pair<const char *,size_t> getNonceFromSCAB(const char* scab, size_t scabSize);
    static uint64_t getEcidFromSCAB(const char* scab, size_t scabSize);
    static plist_t loadPlistFromFile(const char *path);
    static void saveStringToFile(std::string str, std::string path);
    static char *getPathOfElementInManifest(const char *element, const char *manifeststr, const char *boardConfig, int isUpdateInstall);
    static bool elemExists(const char *element, const char *manifeststr, const char *boardConfig, int isUpdateInstall);
    static std::string getGeneratorFromSHSH2(plist_t shsh2);
};

#endif /* futurerestore_hpp */
