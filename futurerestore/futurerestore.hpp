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
#include <plist/plist.h>
#include <libgeneral/Mem.hpp>

#include <ra1nsn0w/ra1nsn0w.hpp>

extern "C"{
#include <idevicerestore/idevicerestore.h>
};

struct idevicerestore_mode_t;

class futurerestore {
    idevicerestore_client_t *_client;

    std::vector<plist_t> _aptickets;
    std::vector<tihmstar::Mem>_im4ms;
    tihmstar::Mem _donorLLB;
    std::string _donorIPSW;
    tihmstar::Mem _sep;
    std::string _latestFirmwareUrl;
    
    tihmstar::Mem _customRamdisk;
    tihmstar::Mem _sepTicket;
    plist_t _p_latestManifestCache;
    tihmstar::ra1nsn0w::launchConfig _persist_cfg;

    bool _isUpdateInstall;
    bool _isPwnDfu;
    bool _no_ticket_needed;
    bool _restorePwnage;
    bool _restore24kpwn;
    bool _needsDecryptedRamdisk;
    
private:
    const plist_t loadBuilmanifestForLatestFirmware();
    tihmstar::Mem getComponentFromIPSW(const char *ipswPath, const char *component);

public:
    futurerestore(uint64_t ecid = 0, bool isUpdateInstall = false, bool isPwnDfu = true);
    ~futurerestore();
    
    void setPersistCfg(tihmstar::ra1nsn0w::launchConfig cfg);
    void setRestorePwnage(bool restorePwnage);
    void setRestore24kpwn(bool restore24kpwn);
    void setNeedsDecryptedRamdisk(bool needsDecryptedRamdisk);
    void setNoTicketNeeded(bool ticketIsNotNeeded);
    void loadAPTickets(const std::vector<const char*> &APTicketPaths);
    
    idevicerestore_mode_t *getDeviceMode();
    bool isInRestoreMode();
    void putDeviceIntoRecovery();
    
    tihmstar::Mem getRamdiskForDevice(const char *ipswpath);
    
    void doRestore(const char *ipswpath);
    void enterPwnRestore(const char *ipswpath, bool is32bit, const char *bootargs = NULL, tihmstar::ra1nsn0w::launchConfig = {});
    void enterRestore(const char *ipswpath);

    void setLLBFromIPSW(const char *llbipswpath);

    const char *getDeviceBoardNoCopy();
    const char *getDeviceModelNoCopy();
    
    plist_t nonceMatchesApTickets();
    tihmstar::Mem nonceMatchesIM4Ms();
    uint64_t getDeviceEcid();
    uint32_t getDeviceCPID();
    uint32_t getDeviceBDID();

    void requestFreshAPTicket(plist_t buildManifest, const char *variant = NULL, bool skipRamdisk = false);
    void requestFreshAPTicket(const char *ipsw, const char *variant = NULL, bool skipRamdisk = false);
    void requestFreshSEPTicket(plist_t buildManifest, const char *variant = NULL);
    void fetchLatestBaseband();
    void fetchLatestSEP();

    void setCustomBasebandPath(const char *basebandPath);
    void setCustomSEPPath(const char *SEPPath);
    void setCustomBasebandManifest(plist_t basebandManifest);
    void setCustomSEPManifest(plist_t SEPManifest);

    void setCustomRamdisk(const char *ramdiskPath);

    void verifyBasebandTicketCanBeRequested();
    
#pragma mark static
    static tihmstar::Mem getRamdiskHashFromSCAB(const void* scab, size_t scabSize);
//    static std::pair<const char *,size_t> getNonceFromSCAB(const char* scab, size_t scabSize);
    static uint64_t getEcidFromSCAB(const void* scab, size_t scabSize);
//    static plist_t loadPlistFromFile(const char *path);
//    static void saveStringToFile(const char *str, const char *path);
//    static char *getPathOfElementInManifest(const char *element, const char *manifeststr, const char *model, int isUpdateInstall);
//    static std::string getGeneratorFromSHSH2(const plist_t shsh2);
};

#endif /* futurerestore_hpp */
