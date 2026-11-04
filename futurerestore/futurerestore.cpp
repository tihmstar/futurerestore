//
//  futurerestore.cpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <libgeneral/macros.h>
#include <libgeneral/Utils.hpp>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <zlib.h>
#include <fcntl.h>
#include <memory>
#include "futurerestore.hpp"

#include <img4tool/img4tool.hpp>
#include <img3tool/img3tool.hpp>
#include <img3tool/24kpwn.hpp>

#include <ra1nsn0w/ra1nsn0w_patch.hpp>
#include <libfwkeyfetch/libfwkeyfetch.hpp>

#include <tsschecker/tsschecker.hpp>
#include <tsschecker/TssRequest.hpp>

extern "C"{
#include <libirecovery.h>
#include <libimobiledevice/libimobiledevice.h>
#include <idevicerestore/idevicerestore.h>
#include <libimobiledevice-glue/thread.h>
#include <libimobiledevice-glue/utils.h>

#include <idevicerestore/common.h>
#include <idevicerestore/dfu.h>
#include <idevicerestore/recovery.h>
#include <idevicerestore/restore.h>
#include <idevicerestore/normal.h>
#include <idevicerestore/ipsw.h>
#include <idevicerestore/locking.h>
}

//(re)define __mkdir
#ifdef __mkdir
#undef __mkdir
#endif
#ifdef WIN32
#include <windows.h>
#define __mkdir(path, mode) mkdir(path)
#else
#include <sys/stat.h>
#define __mkdir(path, mode) mkdir(path, mode)
#endif

#ifdef WIN32
#define FUTURERESTORE_TMP_PATH "download"
#else
#define TMP_PATH "/tmp"
#define FUTURERESTORE_TMP_PATH TMP_PATH"/futurerestore"
#endif

#define BASEBAND_TMP_PATH FUTURERESTORE_TMP_PATH"/baseband.bbfw"

#define USEC_PER_SEC 1000000

#ifdef __APPLE__
#   include <CommonCrypto/CommonDigest.h>
#   define SHA1(d, n, md) CC_SHA1(d, n, md)
#   define SHA384(d, n, md) CC_SHA384(d, n, md)
#   define SHA_DIGEST_LENGTH CC_SHA1_DIGEST_LENGTH
#else
#   include <openssl/sha.h>
#endif // __APPLE__

using namespace tihmstar;

#pragma mark helpers
extern "C"{
    void irecv_event_cb(const irecv_device_event_t* event, void *userdata);
    void idevice_event_cb(const idevice_event_t *event, void *userdata);
};

tihmstar::Mem getIPSWComponent(struct idevicerestore_client_t* client, plist_t build_identity, std::string component){
    char *path = NULL;
    void* component_data = NULL;
    cleanup([&]{
        safeFree(component_data);
        safeFree(path);
    });
    size_t component_size = 0;

    retassure(!build_identity_get_component_path(build_identity, component.c_str(), &path),"ERROR: Unable to get path for component '%s'", component.c_str());
    retassure(!extract_component(client->ipsw, (char*)path, &component_data, &component_size),"ERROR: Unable to extract component: %s", component.c_str());
    {
        Mem ret{component_data,component_size}; component_data = NULL; component_size = 0;
        return ret;
    }
}

void get_custom_component_decrypted(struct idevicerestore_client_t* client, plist_t build_identity, const char* component, unsigned char** data, size_t *size) noexcept{
#ifndef HAVE_LIBIPATCHER
    reterror("compiled without libipatcher");
#else
    try {
        auto comp = getIPSWComponent(client, build_identity, component);
        tihmstar::Mem comp_dec;
        {
            auto keys = libfwkeyfetch::getFirmwareKeyForComponent(client->device->product_type, client->build, component);
            if (client->image4supported) {
                auto dec = ra1nsn0w::patchIMG4(comp.data(), comp.size(), keys.iv, keys.key, "Apple", [](void*, size_t, void*)->int{return 0;}, NULL);
                comp_dec = {(const void*)dec.buf(), dec.size()};
            }else{
                comp_dec = ra1nsn0w::patchIMG3(comp.data(), comp.size(), keys.iv, keys.key, "Apple", [](void*, size_t, void*)->int{return 0;}, NULL);
            }
        }
        *data = (unsigned char*)comp_dec.data();
        *size = (unsigned int)comp_dec.size();
        comp_dec.disown();
    } catch (tihmstar::exception &e) {
        reterror("ERROR: libipatcher failed with reason %d (%s)",e.code(),e.what());
    }
#endif
}

#pragma mark futurerestore
futurerestore::futurerestore(uint64_t ecid, bool isUpdateInstall, bool isPwnDfu)
: _client(NULL)
, _isUpdateInstall(isUpdateInstall), _isPwnDfu(isPwnDfu), _no_ticket_needed(false), _restorePwnage(false), _restore24kpwn(false), _needsDecryptedRamdisk(false)
, _p_latestManifestCache(NULL)
, _persist_cfg{}
{
    bool didInit = false;
    cleanup([&]{
        if (!didInit) {
            this->~futurerestore();
        }
    })
    irecv_error_t err_irecv = {};
    idevice_error_t err_idev = {};
    retassure(_client = idevicerestore_client_new(), "Failed to create idevicerestore client");
    _client->ecid = ecid;
    retassure(!(err_irecv = irecv_device_event_subscribe(&_client->irecv_e_ctx, irecv_event_cb, _client)),"Failed to irecv_device_event_subscribe with err=%d",err_irecv);
    retassure(!(err_idev = idevice_event_subscribe(idevice_event_cb, _client)),"Failed to idevice_event_subscribe with err=%d",err_irecv);
    _client->idevice_e_ctx = (void*)idevice_event_cb;
    
    struct stat st{0};
    if (stat(FUTURERESTORE_TMP_PATH, &st) == -1) __mkdir(FUTURERESTORE_TMP_PATH, 0755);
    
    auto mode = getDeviceMode();
    info("Found device in %s mode", mode->string);
    
    _client->image4supported = is_image4_supported(_client);
    
    didInit = true;
}

futurerestore::~futurerestore(){
    safeFreeCustom(_p_latestManifestCache, plist_free);
    while (_aptickets.size()) {
        auto t = _aptickets.back();
        safeFreeCustom(t, plist_free);
        _aptickets.pop_back();
    }
    safeFreeCustom(_client, idevicerestore_client_free);
}

#pragma mark private
const plist_t futurerestore::loadBuilmanifestForLatestFirmware(){
    if (!_p_latestManifestCache) {
        auto latestFirmwareInfo = tsschecker::getLatestFirmwareForDevice(getDeviceCPID(), getDeviceBDID());
        _latestFirmwareUrl = latestFirmwareInfo.url;
        _p_latestManifestCache = tsschecker::getBuildManifestFromUrl(_latestFirmwareUrl.c_str());
    }
    return _p_latestManifestCache;
}

tihmstar::Mem futurerestore::getComponentFromIPSW(const char *ipswPath, const char *component){
    plist_t p_buildmanifest = NULL;
    char *componentPath = NULL;
    void *componentBuf = NULL;
    ipsw_archive_t ipswArchive = NULL;
    cleanup([&]{
        safeFreeCustom(ipswArchive, ipsw_close);
        safeFree(componentBuf);
        safeFree(componentPath);
        safeFreeCustom(p_buildmanifest, plist_free);
    });
    size_t componentBufSize = 0;
    plist_t p_buildidentity = NULL;
    
    retassure(ipswArchive = ipsw_open(ipswPath), "Failed to open IPSW '%s'",ipswPath);
    {
        int unused = 0;
        if (ipsw_extract_build_manifest(ipswArchive, &p_buildmanifest, &unused)) {
            error("ERROR: Unable to extract BuildManifest from %s. Firmware file might be corrupt.", ipswPath);
            retassure(!ipsw_extract_restore_plist(ipswArchive, &p_buildmanifest),"ERROR: Unable to extract Restore.plist from %s. Firmware file might be corrupt.", ipswPath);
            tsschecker::buildIdentityFromRestorePlist(p_buildmanifest);
        }
    }
    
    retassure(p_buildidentity = build_manifest_get_build_identity_for_model_with_variant(p_buildmanifest, getDeviceBoardNoCopy(), ""/*any*/, 0),"ERROR: Unable to find any build identities for IPSW");
    
    retassure(!build_identity_get_component_path(p_buildidentity, component, &componentPath), "ERROR: Unable to get path for '%s' component",component);
    
    retassure(!extract_component(ipswArchive, componentPath, &componentBuf, &componentBufSize),"Failed to extract component");
    {
        tihmstar::Mem ret{componentBuf,componentBufSize}; componentBuf = NULL; //transfer ownership
        return ret;
    }
}

#pragma mark public

void futurerestore::setPersistCfg(tihmstar::ra1nsn0w::launchConfig cfg){
    _persist_cfg = cfg;
}

void futurerestore::setRestorePwnage(bool restorePwnage){
    _restorePwnage = restorePwnage;
}

void futurerestore::setRestore24kpwn(bool restore24kpwn){
    _restore24kpwn = restore24kpwn;
    if (restore24kpwn) _restorePwnage = restore24kpwn;
}

void futurerestore::setNeedsDecryptedRamdisk(bool needsDecryptedRamdisk){
    _needsDecryptedRamdisk = needsDecryptedRamdisk;
}

void futurerestore::setNoTicketNeeded(bool ticketIsNotNeeded){
    _no_ticket_needed = ticketIsNotNeeded;
}

void futurerestore::loadAPTickets(const std::vector<const char *> &apticketPaths){
    for (auto apticketPath : apticketPaths){
        int fd = -1;
        plist_t apticket = NULL;
        char *buf = NULL;
        char *im4m = NULL;
        cleanup([&]{
            safeFree(im4m);
            safeFree(buf);
            safeFreeCustom(apticket, plist_free);
            safeClose(fd);
        });
        size_t bufSize = 0;
        struct stat fst;
        
        retassure((fd = open(apticketPath, O_RDONLY)) != -1, "failed to load APTicket at %s",apticketPath);
        retassure(!fstat(fd, &fst), "Failed to stat");
        retassure(buf = (char*)malloc(bufSize = fst.st_size), "Failed to malloc mem");
        retassure(read(fd, buf, bufSize) == bufSize, "Failed to read ticket");
        plist_from_memory(buf, (uint32_t)bufSize, &apticket, NULL);
        
        if (_isUpdateInstall) {
            if(plist_t update =  plist_dict_get_item(apticket, "updateInstall")){
                plist_t cpy = NULL;
                cleanup([&]{
                    safeFreeCustom(cpy, plist_free);
                });
                assure(cpy = plist_copy(update));
                std::swap(cpy, apticket);
            }
        }
        
        plist_t ticket = plist_dict_get_item(apticket, (_client->image4supported) ? "ApImg4Ticket" : "APTicket");
        uint64_t im4msize=0;
        plist_get_data_val(ticket, &im4m, &im4msize);
        
        if (!im4msize){
            warning("SHSH file does NOT contain APTicket!");
        }else{
            _im4ms.push_back({im4m,im4msize}); im4m = NULL; im4msize = 0;
        }
        _aptickets.push_back(apticket); apticket = NULL;
        info("reading signing ticket %s is done",apticketPath);
    }
}

idevicerestore_mode_t *futurerestore::getDeviceMode(){
    mutex_lock(&_client->device_event_mutex);
    cleanup([&]{
        mutex_unlock(&_client->device_event_mutex);
    });
    if (_client->mode == MODE_UNKNOWN) {
        info("Waiting for device to enter restore mode...");
        cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 120000);
        if (_client->mode == MODE_UNKNOWN) {
            reterror("ERROR: Unable to discover device mode. Please make sure a device is attached.");
        }
    }
    return _client->mode;
}

bool futurerestore::isInRestoreMode(){
    return getDeviceMode() == MODE_RESTORE;
}

void futurerestore::putDeviceIntoRecovery(){
    auto mode = getDeviceMode();
    
    info("Found device in %s mode", mode->string);
    if (mode == MODE_NORMAL){
#ifdef HAVE_LIBIPATCHER
        retassure(!_isPwnDfu, "isPwnDfu enabled, but device was found in normal mode");
#endif
        info("Entering recovery mode...");
        retassure(!normal_enter_recovery(_client),"Unable to place device into recovery mode from %s mode", _client->mode->string);
    }else if (mode == MODE_RECOVERY){
        info("Device already in recovery mode");
    }else if (_client->mode == MODE_DFU && _isPwnDfu &&
#ifdef HAVE_LIBIPATCHER
              true
#else
              false
#endif
              ){
        info("requesting to get into pwnRecovery later");
    }else if (!_client->image4supported){
        info("32-bit device in DFU mode found, assuming user wants to use iOS 9.x re-restore bug. Not failing here");
    }else{
        reterror("unsupported device mode, please put device in recovery or normal mode");
    }

    //these get also freed by destructor
    dfu_client_free(_client);
    recovery_client_free(_client);
}

tihmstar::Mem futurerestore::getRamdiskForDevice(const char *ipswpath){
    plist_t buildmanifest = NULL;
    char* rdiskPath = NULL;
    void *ramdisk = NULL;
    ipsw_archive_t ipsw = NULL;
    cleanup([&]{
        safeFreeCustom(ipsw, ipsw_close);
        safeFree(ramdisk);
        safeFree(rdiskPath);
        safeFreeCustom(buildmanifest, plist_free);
    });
    size_t ramdiskSize = 0;
    plist_t build_identity = NULL;
    
    if (getDeviceMode() == MODE_WTF ) {
        info("Booting WTF into pwnDFU mode!");
        tihmstar::ra1nsn0w::iOSDevice ra1nDevice;

        tihmstar::ra1nsn0w::launchConfig cfg{
            .is32Bit = true,
            .justDFU = true
        };

        dfu_client_free(_client);
        std::string file = "file://";
        file += ipswpath;
        ra1nsn0w::launchDevice(ra1nDevice, file, cfg);
    }
    
    info("Extracting BuildManifest from IPSW");
    assure(ipsw = ipsw_open(ipswpath));
    {
        int unused;
        if (ipsw_extract_build_manifest(ipsw, &buildmanifest, &unused)) {
            error("ERROR: Unable to extract BuildManifest from %s. Firmware file might be corrupt.", ipswpath);
            retassure(!ipsw_extract_restore_plist(ipsw, &buildmanifest),"ERROR: Unable to extract Restore.plist from %s. Firmware file might be corrupt.", ipswpath);
            tsschecker::buildIdentityFromRestorePlist(buildmanifest);
        }
    }
    info("Identified device as %s, %s", getDeviceBoardNoCopy(), getDeviceModelNoCopy());
    
    /* check if device type is supported by the given build manifest */
    retassure(!build_manifest_check_compatibility(buildmanifest, _client->device->product_type),"ERROR: Could not make sure this firmware is suitable for the current device. Refusing to continue.");
    
    /* print iOS information from the manifest */
    build_manifest_get_version_information(buildmanifest, _client);
    info("Product version: %s", _client->version);
    info("Product build: %s Major: %d", _client->build, _client->build_major);
    
    retassure(build_identity = build_manifest_get_build_identity_for_model_with_variant(buildmanifest, _client->device->hardware_model, _isUpdateInstall ? RESTORE_VARIANT_UPGRADE_INSTALL : RESTORE_VARIANT_ERASE_INSTALL, 0),"ERROR: Unable to find any build identities for iPSW");
    
    retassure(!build_identity_get_component_path(build_identity, "RestoreRamDisk", &rdiskPath), "ERROR: Unable to get path for RestoreRamDisk component");
    
    retassure(!extract_component(ipsw, rdiskPath, &ramdisk, &ramdiskSize),"Failed to extract ramdisk");
    {
        Mem ret{ramdisk,ramdiskSize}; ramdisk = NULL; ramdiskSize = 0;
        return ret;
    }
}


void futurerestore::doRestore(const char *ipswpath){
    plist_t build_identity = NULL;
    bool build_identity_needs_free = false;
    cleanup([&]{
        if (build_identity_needs_free) {
            safeFreeCustom(build_identity, plist_free);
        }
    });

    struct idevicerestore_client_t* client = _client;
    int isRestorePlist = 0;

    if (!_client->tss && !_no_ticket_needed) {
        try {
            _client->tss = _aptickets.at(0);
        } catch (...) {
            reterror("Failed to load apticket");
        }
        info("Loaded APticket at index 0");
    }
    
    safeFreeCustom(client->ipsw, ipsw_close);
    retassure(client->ipsw = ipsw_open(ipswpath),"ERROR: Failed to open ipsw %s does not exist.",ipswpath);
    
    if (!_isUpdateInstall) client->flags |= FLAG_ERASE;

    {
        auto mode = getDeviceMode();
        info("Found device in %s mode", mode->string);
        retassure(mode->index == _MODE_RESTORE, "Device not in restore mode");
    }

    info("Identified device as %s, %s", getDeviceBoardNoCopy(), getDeviceModelNoCopy());
    info("Extracting BuildManifest from iPSW");
    {
        int unused;
        if (ipsw_extract_build_manifest(client->ipsw, &client->build_manifest, &unused) < 0) {
            error("ERROR: Unable to extract BuildManifest from %s. Firmware file might be corrupt.", client->ipsw->path);
            if (ipsw_extract_restore_plist(client->ipsw, &client->build_manifest) < 0) {
                reterror("ERROR: Unable to extract Restore.plist from %s. Firmware file might be corrupt.", client->ipsw->path);
            }
            isRestorePlist = 1;
        }
    }
    
    if (isRestorePlist) {
        build_identity = plist_new_dict();
        build_identity_needs_free = true;

        if (client->mode == MODE_WTF){
            /*
                If we're still in WTF mode (i.e. when restoring iOS 1) we can't correctly identify the device yet.
                We'll just assume that this firmware is correct for the device and identify the device base on the firmware.
                Not ideal, but there is no other way to distinguish iPod1,1 from iPhone1,1 restoring iOS 1
            */
            plist_t p_ProductType = plist_dict_get_item(client->build_manifest, "ProductType");
            if (!p_ProductType){
                error("ERROR: Unable to get ProductType from Restore.plist");
            }else{
                char *firmwareProductType = NULL;
                plist_get_string_val(p_ProductType, &firmwareProductType);
                if (!firmwareProductType){
                    error("ERROR: ProductType from Restore.plist is not of type PLIST_STRING");
                }else{
                    irecv_error_t ierr = IRECV_E_SUCCESS;
                    ierr = irecv_devices_get_device_by_product_type(firmwareProductType, &client->device);
                    if (ierr){
                        error("ERROR: Failed to identify device from ProductType");
                    }
                    free(firmwareProductType);
                }
            }
        }

        {
            plist_t node;
            plist_t comp;
            plist_t inf;
            plist_t manifest;

            char tmpstr[256];
            char p_all_flash[128];
            char lcmodel[8];
            strcpy(lcmodel, client->device->hardware_model);
            int x = 0;
            while (lcmodel[x]) {
                lcmodel[x] = tolower(lcmodel[x]);
                x++;
            }

            sprintf(p_all_flash, "Firmware/all_flash/all_flash.%s.%s", lcmodel, "production");
            strcpy(tmpstr, p_all_flash);
            strcat(tmpstr, "/manifest");

            // get all_flash file manifest
            char *files[16];
            char *fmanifest = NULL;
            size_t msize = 0;
            if (ipsw_extract_to_memory(client->ipsw, tmpstr, (void**)&fmanifest, &msize) < 0) {
                reterror("ERROR: could not extract %s from IPSW", tmpstr);
            }

            char *tok = strtok(fmanifest, "\r\n");
            int fc = 0;
            while (tok) {
                files[fc++] = strdup(tok);
                if (fc >= 16) {
                    break;
                }
                tok = strtok(NULL, "\r\n");
            }
            free(fmanifest);

            manifest = plist_new_dict();

            for (x = 0; x < fc; x++) {
                inf = plist_new_dict();
                strcpy(tmpstr, p_all_flash);
                strcat(tmpstr, "/");
                strcat(tmpstr, files[x]);
                plist_dict_set_item(inf, "Path", plist_new_string(tmpstr));
                comp = plist_new_dict();
                plist_dict_set_item(comp, "Info", inf);
                const char* compname = get_component_name(files[x]);
                if (compname) {
                    plist_dict_set_item(manifest, compname, comp);
                    if (!strncmp(files[x], "DeviceTree", 10)) {
                        plist_dict_set_item(manifest, "RestoreDeviceTree", plist_copy(comp));
                    }
                } else {
                    error("WARNING: unhandled component %s", files[x]);
                    plist_free(comp);
                }
                free(files[x]);
                files[x] = NULL;
            }

            // add iBSS
            sprintf(tmpstr, "Firmware/dfu/iBSS.%s.%s.dfu", lcmodel, "RELEASE");
            inf = plist_new_dict();
            plist_dict_set_item(inf, "Path", plist_new_string(tmpstr));
            comp = plist_new_dict();
            plist_dict_set_item(comp, "Info", inf);
            plist_dict_set_item(manifest, "iBSS", comp);

            // add iBEC
            sprintf(tmpstr, "Firmware/dfu/iBEC.%s.%s.dfu", lcmodel, "RELEASE");
            inf = plist_new_dict();
            if (ipsw_file_exists(client->ipsw, tmpstr)){
                /*
                    iOS 1.0 doesn't actually have iBEC
                */
                plist_dict_set_item(inf, "Path", plist_new_string(tmpstr));
                comp = plist_new_dict();
                plist_dict_set_item(comp, "Info", inf);
                plist_dict_set_item(manifest, "iBEC", comp);
            }

            //add logo
            {
                const char *logopath = NULL;
                snprintf(tmpstr,sizeof(tmpstr), "Firmware/all_flash/all_flash.%s.production/applelogo.img2",lcmodel);
                if (ipsw_file_exists(client->ipsw,tmpstr)){
                    logopath = tmpstr;
                }else{
                    snprintf(tmpstr,sizeof(tmpstr), "Firmware/all_flash/all_flash.%s.production/applelogo.s5l8900x.img3",lcmodel);
                    if (ipsw_file_exists(client->ipsw,tmpstr)){
                        logopath = tmpstr;
                    }
                }
                if (logopath){
                    inf = plist_new_dict();
                    plist_dict_set_item(inf, "Path", plist_new_string(logopath));
                    comp = plist_new_dict();
                    plist_dict_set_item(comp, "Info", inf);
                    plist_dict_set_item(manifest, "RestoreLogo", comp);
                }
            }

            //add logo
            {
                const char *logopath = NULL;
                snprintf(tmpstr,sizeof(tmpstr), "Firmware/all_flash/all_flash.%s.production/applelogo.img2",lcmodel);
                if (ipsw_file_exists(client->ipsw,tmpstr)){
                    logopath = tmpstr;
                }else{
                    snprintf(tmpstr,sizeof(tmpstr), "Firmware/all_flash/all_flash.%s.production/applelogo.s5l8900x.img3",lcmodel);
                    if (ipsw_file_exists(client->ipsw,tmpstr)){
                        logopath = tmpstr;
                    }
                }
                if (logopath){
                    inf = plist_new_dict();
                    plist_dict_set_item(inf, "Path", plist_new_string(logopath));
                    comp = plist_new_dict();
                    plist_dict_set_item(comp, "Info", inf);
                    plist_dict_set_item(manifest, "RestoreLogo", comp);
                }
            }

            // add kernel cache
            plist_t kdict = NULL;

            node = plist_dict_get_item(client->build_manifest, "KernelCachesByTarget");
            if (node && (plist_get_node_type(node) == PLIST_DICT)) {
                char tt[4];
                strncpy(tt, lcmodel, 3);
                tt[3] = 0;
                kdict = plist_dict_get_item(node, tt);
            } else {
                // Populated in older iOS IPSWs
                kdict = plist_dict_get_item(client->build_manifest, "RestoreKernelCaches");
            }
            if (kdict && (plist_get_node_type(kdict) == PLIST_DICT)) {
                plist_t kc = plist_dict_get_item(kdict, "Release");
                if (kc && (plist_get_node_type(kc) == PLIST_STRING)) {
                    inf = plist_new_dict();
                    plist_dict_set_item(inf, "Path", plist_copy(kc));
                    comp = plist_new_dict();
                    plist_dict_set_item(comp, "Info", inf);
                    plist_dict_set_item(manifest, "KernelCache", comp);
                    plist_dict_set_item(manifest, "RestoreKernelCache", plist_copy(comp));
                }
            }

            // add ramdisk
            node = plist_dict_get_item(client->build_manifest, "RestoreRamDisks");
            if (node && (plist_get_node_type(node) == PLIST_DICT)) {
                plist_t rd = plist_dict_get_item(node, (client->flags & FLAG_ERASE) ? "User" : "Update");
                // if no "Update" ram disk entry is found try "User" ram disk instead
                if (!rd && !(client->flags & FLAG_ERASE)) {
                    rd = plist_dict_get_item(node, "User");
                    // also, set the ERASE flag since we actually change the restore variant
                    client->flags |= FLAG_ERASE;
                }
                if (rd && (plist_get_node_type(rd) == PLIST_STRING)) {
                    inf = plist_new_dict();
                    plist_dict_set_item(inf, "Path", plist_copy(rd));
                    comp = plist_new_dict();
                    plist_dict_set_item(comp, "Info", inf);
                    plist_dict_set_item(manifest, "RestoreRamDisk", comp);
                }
            }

            // add OS filesystem
            node = plist_dict_get_item(client->build_manifest, "SystemRestoreImages");
            if (!node) {
                error("ERROR: missing SystemRestoreImages in Restore.plist");
            }
            plist_t os = plist_dict_get_item(node, "User");
            if (!os) {
                error("ERROR: missing filesystem in Restore.plist");
            } else {
                inf = plist_new_dict();
                plist_dict_set_item(inf, "Path", plist_copy(os));
                comp = plist_new_dict();
                plist_dict_set_item(comp, "Info", inf);
                plist_dict_set_item(manifest, "OS", comp);
            }

            // add info
            inf = plist_new_dict();
            plist_dict_set_item(inf, "RestoreBehavior", plist_new_string((client->flags & FLAG_ERASE) ? "Erase" : "Update"));
            plist_dict_set_item(inf, "Variant", plist_new_string((client->flags & FLAG_ERASE) ? "Customer " RESTORE_VARIANT_ERASE_INSTALL : "Customer " RESTORE_VARIANT_UPGRADE_INSTALL));
            plist_dict_set_item(build_identity, "Info", inf);

            // finally add manifest
            plist_dict_set_item(build_identity, "Manifest", manifest);
        }
    }


    
    /* check if device type is supported by the given build manifest */
    retassure(!build_manifest_check_compatibility(client->build_manifest, client->device->product_type),"ERROR: Could not make sure this firmware is suitable for the current device. Refusing to continue.");

    /* print iOS information from the manifest */
    build_manifest_get_version_information(client->build_manifest, client);
    info("Product version: %s", client->version);
    info("Product build: %s Major: %d", client->build, client->build_major);
    client->image4supported = is_image4_supported(client);
    info("Device supports Image4: %s", (client->image4supported) ? "true" : "false");

    if (client->tss) {
        plist_dict_remove_item(client->tss, "BBTicket");
        plist_dict_remove_item(client->tss, "BasebandFirmware");
    }

    if (!build_identity) retassure(build_identity = build_manifest_get_build_identity_for_model_with_variant(client->build_manifest, client->device->hardware_model, _isUpdateInstall ? RESTORE_VARIANT_UPGRADE_INSTALL : RESTORE_VARIANT_ERASE_INSTALL, 0),"ERROR: Unable to find any build identities for iPSW");

    build_identity_print_information(build_identity); // print information about current build identity

    // Get filesystem name from build identity
    char* os_path = NULL;
    retassure(!build_identity_get_component_path(build_identity, "OS", &os_path), "ERROR: Unable to get path for filesystem component");

    {
        /* check if IPSW has OS component 'stored' in ZIP archive, otherwise we need to extract it */
        int needs_os_extraction = 0;
        if (client->ipsw->zip) {
            ipsw_file_handle_t zfile = ipsw_file_open(client->ipsw, os_path);
            if (zfile) {
                if (!zfile->seekable) {
                    needs_os_extraction = 1;
                }
                ipsw_file_close(zfile);
            }
        }

        if (needs_os_extraction && !(client->flags & FLAG_SHSHONLY)) {
            char* tmpf = NULL;
            cleanup([&]{
                if (tmpf){
                    unlink(tmpf);
                    safeFree(tmpf);
                }
            });
            struct stat st = {};
            if (client->cache_dir) {
                char* ipsw_basename = NULL;
                cleanup([&]{
                    safeFree(ipsw_basename);
                });
                memset(&st, '\0', sizeof(struct stat));
                if (stat(client->cache_dir, &st) < 0) {
                    mkdir_with_parents(client->cache_dir, 0755);
                }
                ipsw_basename = strdup(path_get_basename(client->ipsw->path));
                char* p = strrchr(ipsw_basename, '.');
                if (p && isalpha(*(p+1))) {
                    *p = '\0';
                }
                tmpf = string_build_path(client->cache_dir, ipsw_basename, NULL);
                mkdir_with_parents(tmpf, 0755);
                tmpf = string_build_path(client->cache_dir, ipsw_basename, os_path, NULL);
            } else {
                tmpf = get_temp_filename(NULL);
                client->delete_fs = 1;
            }

            /* check if we already have it extracted */
            uint64_t fssize = 0;
            ipsw_get_file_size(client->ipsw, os_path, &fssize);
            memset(&st, '\0', sizeof(struct stat));
            if (stat(tmpf, &st) == 0) {
                if ((fssize > 0) && ((uint64_t)st.st_size == fssize)) {
                    info("Using cached filesystem from '%s'", tmpf);
                    client->filesystem = tmpf; tmpf = NULL;
                }
            }

            if (!client->filesystem) {
                info("Extracting filesystem from IPSW: %s", os_path);
                if (ipsw_extract_to_file_with_progress(client->ipsw, os_path, tmpf, 1) < 0) {
                    error("ERROR: Unable to extract filesystem from IPSW");
                    info("Removing %s", tmpf);
                    reterror("ERROR: Unable to extract filesystem from IPSW");
                }
                client->filesystem = tmpf; tmpf = NULL;
            }
        }
    }
        
    info("About to restore device... ");
    {
        int result = 0;
        client->user_cb_data = this;
        client->restore_custom_component_function = [](struct idevicerestore_client_t* client, const char* name, void**data, size_t *dataSize)->int{
            tihmstar::Mem fcomponent;
            const tihmstar::Mem *unsigned_component = NULL;
            futurerestore *_this = (futurerestore*)(client->user_cb_data);
            debug("restore_custom_component_function called with component '%s'",name);
            
            if (_this->_restorePwnage) {
                if (_this->_persist_cfg.bootlogoIm4p.size() && !strcmp(name, "AppleLogo")){
                    unsigned_component = &_this->_persist_cfg.bootlogoIm4p;
                }else if (!strcmp(name, "KernelCache") ||
                          !strcmp(name, "LLB") ||
                          !strcmp(name, "iBoot")) {
                    fcomponent = _this->getComponentFromIPSW(client->ipsw->path,name);
                    
                    auto keys = libfwkeyfetch::getFirmwareKeyForComponent(client->device->product_type, client->build, name, client->device->chip_id);
                    ra1nsn0w::bootconfig bcfg = {&_this->_persist_cfg};
                    if (!strcmp(name, "KernelCache")){
                        fcomponent = ra1nsn0w::patchIMG3(fcomponent.data(), fcomponent.size(), keys.iv, keys.key, "Apple", (int(*)(void*,size_t,void*))ra1nsn0w::patchFuncKernel, &bcfg);
                    }else if (!strcmp(name, "LLB") ||
                              !strcmp(name, "iBoot")) {
                        fcomponent = ra1nsn0w::patchIMG3(fcomponent.data(), fcomponent.size(), keys.iv, keys.key, "Apple", (int(*)(void*,size_t,void*))ra1nsn0w::patchFunciBoot, &bcfg);
                    }
                    unsigned_component = &fcomponent;
                } else if (client->device->chip_id == 0x8900 && (!strcmp(client->version, "3.0") || !strcmp(client->version, "3.0.1"))) {
                    info("Force decrypting nor component '%s' for iOS 3.0/3.0.1 gid issue workaround",name);
                    fcomponent = _this->getComponentFromIPSW(client->ipsw->path, name);
                    auto keys = libfwkeyfetch::getFirmwareKeyForComponent(client->device->product_type, client->build, name, client->device->chip_id);
                    fcomponent = ra1nsn0w::patchIMG3(fcomponent.data(), fcomponent.size(), keys.iv, keys.key, "", [](void*, size_t, void*)->int{return 0;}, NULL);
                    unsigned_component = &fcomponent;
                }
            }else if (_this->_donorLLB.size() && strcmp(name, "LLB") == 0) {
                unsigned_component = &_this->_donorLLB;
            }else if (_this->_sep.size() && strcmp(name, "SEP") == 0){
                unsigned_component = &_this->_sep;
            }else if (_this->_donorIPSW.size()){
                if (!strcmp(name, "BatteryCharging0") ||
                    !strcmp(name, "BatteryCharging1") ||
                    !strcmp(name, "BatteryFull") ||
                    !strcmp(name, "BatteryLow0") ||
                    !strcmp(name, "BatteryLow1") ||
                    !strcmp(name, "BatteryPlugin") ||
                    !strcmp(name, "RecoveryMode") ||
                    !strcmp(name, "iBoot")) {
                    /*
                        Send valid iBoot and logos so that we can get out of battery charge trap
                     */
                    fcomponent = _this->getComponentFromIPSW(_this->_donorIPSW.c_str(),name);
                    unsigned_component = &fcomponent;
                }
            }
            
            if (unsigned_component) {
                if (client->image4supported) {
                    auto img4 = img4tool::getEmptyIMG4Container();
                    img4 = img4tool::appendIM4PToIMG4(img4, {unsigned_component->data(),unsigned_component->size()});
                    if (strcmp(name, "SEP") == 0) {
                        retassure(_this->_sepTicket.size(), "No SEP ticket found!");
                        img4 = img4tool::appendIM4MToIMG4(img4, {_this->_sepTicket.data(),_this->_sepTicket.size()});
                    } else if (!strcmp(name, "LLB") ||
                               !strcmp(name, "BatteryCharging0") ||
                               !strcmp(name, "BatteryCharging1") ||
                               !strcmp(name, "BatteryFull") ||
                               !strcmp(name, "BatteryLow0") ||
                               !strcmp(name, "BatteryLow1") ||
                               !strcmp(name, "BatteryPlugin") ||
                               !strcmp(name, "RecoveryMode") ||
                               !strcmp(name, "iBoot")
                               ) {
                        tihmstar::Mem im4mRaw;
                        try {
                            im4mRaw = _this->_im4ms.at(0);
                        } catch (...) {
                            reterror("Failed to get IM4M!");
                        }
                        img4 = img4tool::appendIM4MToIMG4(img4, {im4mRaw.data(),im4mRaw.size()});
                    }else{
                        reterror("Unexpected component '%s'",name);
                    }
                    if (dataSize) *dataSize = img4.size();
                    if (data){
                        safeFree(*data);
                        void *ldata = malloc(img4.size());
                        memcpy(ldata, img4.buf(), img4.size());
                        *data = (unsigned char*)ldata;
                    }
                    return 0;
                }else{
                    tihmstar::Mem signedComponent;
                    if (!strcmp(name, "LLB") && _this->_restore24kpwn){
                        info("Injecting 24kpwn exploit into LLB!");
                        signedComponent = img3tool::do24kpwn(unsigned_component->data(), unsigned_component->size(), client->device->chip_id);
                    } else if (_this->_no_ticket_needed) {
                        warning("Not personalizing component '%s' because we're told that no ticket is needed",name);
                        signedComponent = *unsigned_component;
                    }else{
                        retassure(_this->_client->tss, "We're asked to personalize '%s', but we don't have a ticket!!",name);
                        signedComponent = img3tool::signIMG3WithSHSH(unsigned_component->data(), unsigned_component->size(), _this->_client->tss);
                    }
                    if (dataSize) *dataSize = (unsigned int)signedComponent.size();
                    if (data) {
                        safeFree(*data);
                        void *ldata = malloc(signedComponent.size());
                        memcpy(ldata, signedComponent.data(), signedComponent.size());
                        *data = (unsigned char*)ldata;
                    }
                    return 0;
                }
            }
            return 1;
        };
        retassure(!(result = restore_device(client, build_identity)), "ERROR: Unable to restore device");
        client->user_cb_data = NULL;
        client->restore_custom_component_function = NULL;
    }
}

void futurerestore::enterPwnRestore(const char *ipswpath, bool is32Bit, const char *bootargs, ra1nsn0w::launchConfig cfg){
    bool hasSEP = false;
    {
        unsigned char* nonce = NULL;
        cleanup([&]{
            safeFree(nonce);
        });
        unsigned int nonceSize = 0;
        
        retassure(!get_sep_nonce(_client, &nonce, &nonceSize),"Failed to query sepnonce");
        hasSEP = nonce != NULL;
    }
    
    tihmstar::Mem ticket;
    try {
        _client->tss = _aptickets.at(0);
    } catch (...) {
        retassure(_no_ticket_needed,"failed to get ticket");
    }

    try {
        ticket = _im4ms.at(0);
    } catch (...) {
        error("failed to get ticket data");
    }

    
    cfg.restoreBoot = true;
    _persist_cfg.is32Bit = cfg.is32Bit = is32Bit;
    if (_sep.size()) {
        auto img4 = img4tool::getEmptyIMG4Container();
        img4 = img4tool::appendIM4PToIMG4(img4, {_sep.data(),_sep.size()});
        img4 = img4tool::appendIM4MToIMG4(img4, {_sepTicket.data(),_sepTicket.size()});
        cfg.sepIm4p = {img4.buf(),img4.size()};
    }
    if (bootargs) {
        cfg.bootargs = bootargs;
    }
    
    if (_customRamdisk.size()) {
        cfg.kpatch_codesig = tihmstar::ra1nsn0w::kPatchcfgYes;
        cfg.ramdiskIm4p = _customRamdisk;
    }else{
        cfg.ramdiskIm4p = getRamdiskForDevice(ipswpath);
    }
    
    if (getDeviceCPID() == 0x8900 && (!strcmp(_client->version, "3.0") || !strcmp(_client->version, "3.0.1"))) {
        /*
            Workaround for weird dummy gid issue which turns out to actually tie iboot hash or something to the key
            We can avoid bad key by just having everything already decrypted
         */
        info("Enabling 0x8900 iOS 3.0 / 3.0.1 workaround!");
        _needsDecryptedRamdisk = true;
        cfg.decrypt_devicetree = true;
    }
    
    if (_needsDecryptedRamdisk) {
        if (_client->image4supported) {
            reterror("TODO");
        }else{
            auto kb = img3tool::getKBAG(cfg.ramdiskIm4p.data(), cfg.ramdiskIm4p.size(), 1);
            std::string kbhex;
            kbhex.resize(kb.size()*1+2);
            size_t kbhexSize = kbhex.size();
            tihmstar::bytesToHex(kb.data(), kb.size(), (char*)kbhex.data(), &kbhexSize);
            auto keys = libfwkeyfetch::getFirmwareKeyForKBAG(kbhex);
            cfg.ramdiskIm4p = ra1nsn0w::patchIMG3(cfg.ramdiskIm4p.data(), cfg.ramdiskIm4p.size(), keys.iv, keys.key, "Apple", [](void*, size_t, void*)->int{return 0;}, NULL);
        }
    }

    if (ticket.size()) {
        if (_client->image4supported) {
            cfg.boot_manifest_hash.resize(SHA_DIGEST_LENGTH);
            SHA1((const unsigned char *)ticket.data(), (uint32_t)ticket.size(), (unsigned char*)cfg.boot_manifest_hash.data());
        }else{
            if (!_client->version || atoi(_client->version) >= 5) {
                cfg.root_ticket_hash.resize(SHA_DIGEST_LENGTH);
                SHA1((const unsigned char *)ticket.data(), (uint32_t)ticket.size(), (unsigned char*)cfg.root_ticket_hash.data());
            }
        }
    }
    dfu_client_free(_client);
    recovery_client_free(_client);
    
    std::string file = "file://";
    file += ipswpath;
    img4tool::ASN1DERElement im4mData;
    if (_im4ms.size()) {
        retassure(_im4ms.size() == 1, "TODO: selection of im4m is not implemented!");
        auto t = _im4ms.at(0);
        im4mData = {t.data(),t.size()};
    }else{
        retassure(!_client->image4supported, "img4 always needs APTicket for sigcheck bypass!");
    }
        
    {
        ra1nsn0w::iOSDevice dev(_client->ecid);
        ra1nsn0w::launchDevice(dev, file, cfg, im4mData);
    }
}

void futurerestore::enterRestore(const char *ipswpath){
    plist_t p_buildmanifest = NULL;
    cleanup([&]{
        safeFreeCustom(p_buildmanifest, plist_free);
    });
    int err = 0;
    plist_t p_buildidentity = NULL;
        
    {
        auto mode = getDeviceMode();
        info("Found device in %s mode", mode->string);
    }
    info("Identified device as %s, %s", getDeviceBoardNoCopy(), getDeviceModelNoCopy());
    
    safeFreeCustom(_client->ipsw, ipsw_close);
    retassure(_client->ipsw = ipsw_open(ipswpath), "Failed to open ipsw '%s'",ipswpath);

    {
        int unused = 0;
        retassure(!(err = ipsw_extract_build_manifest(_client->ipsw, &p_buildmanifest, &unused)), "Failed to extract BuildManifest.plist from");
    }
    
    retassure(!build_manifest_check_compatibility(p_buildmanifest, _client->device->product_type),"ERROR: Could not make sure this firmware is suitable for the current device. Refusing to continue.");
    
    /* print iOS information from the manifest */
    build_manifest_get_version_information(p_buildmanifest, _client);
    info("Product version: %s", _client->version);
    info("Product build: %s Major: %d", _client->build, _client->build_major);

    retassure(p_buildidentity = build_manifest_get_build_identity_for_model_with_variant(p_buildmanifest, getDeviceBoardNoCopy(), ""/*any*/, 0),"ERROR: Unable to find any build identities for IPSW");
    
    try {
        retassure(_client->tss = plist_copy(_aptickets.at(0)),"plist_copy tss failed");
    } catch (...) {
        reterror("failed to read apticket!");
    }
    
    if (getDeviceMode() == MODE_DFU) {
        retassure(!(err = dfu_enter_recovery(_client, p_buildidentity)),"Failed to enter recovery mode");
    }
    
    if (getDeviceMode() == MODE_RECOVERY) {
        retassure(!(err = recovery_enter_restore(_client, p_buildidentity)),"Failed to enter restore mode");
    }

    if (getDeviceMode() != MODE_RESTORE){
        reterror("Failed to enter restore mode!");
    }
}

void futurerestore::setLLBFromIPSW(const char *llbipswpath){
    plist_t p_buildmanifest = NULL;
    char *llbpath = NULL;
    void *llbbuf = NULL;
    ipsw_archive_t llbipsw = NULL;
    cleanup([&]{
        safeFreeCustom(llbipsw, ipsw_close);
        safeFree(llbbuf);
        safeFree(llbpath);
        safeFreeCustom(p_buildmanifest, plist_free);
    });
    size_t llbbufsize = 0;
    plist_t p_buildidentity = NULL;
    
    int err = 0;
    
    {
        auto mode = getDeviceMode();
        info("Found device in %s mode", mode->string);
    }
    info("Identified device as %s, %s", getDeviceBoardNoCopy(), getDeviceModelNoCopy());
    
    retassure(llbipsw = ipsw_open(llbipswpath), "Failed to open llbipsw '%s'",llbipswpath);
    
    {
        int unused = 0;
        retassure(!(err = ipsw_extract_build_manifest(llbipsw, &p_buildmanifest, &unused)), "Failed to extract BuildManifest.plist from '%s'",llbipsw);
    }
    
    retassure(p_buildidentity = build_manifest_get_build_identity_for_model_with_variant(p_buildmanifest, getDeviceBoardNoCopy(), ""/*any*/, 0),"ERROR: Unable to find any build identities for IPSW");
    
    retassure(!build_identity_get_component_path(p_buildidentity, "LLB", &llbpath), "ERROR: Unable to get path for LLB component");
    
    retassure(!extract_component(llbipsw, llbpath, &llbbuf, &llbbufsize),"Failed to extract ramdisk");
    _donorLLB = {llbbuf,llbbufsize}; llbbuf = NULL; llbbufsize = 0;
    
    {
        debug("Checking if APTicket is valid for donorLLB...");
        if (_client->image4supported) {
            tihmstar::Mem im4mRaw;
            try {
                im4mRaw = _im4ms.at(0);
            } catch (...) {
                reterror("Failed to get APTicket");
            }
            img4tool::ASN1DERElement im4m{im4mRaw.data(),im4mRaw.size()};
            retassure(img4tool::isIM4PValidForIM4M({_donorLLB.data(), _donorLLB.size()}, im4m), "APTicket is not valid for donorLLB");
            retassure(img4tool::isIM4MSignatureValid(im4m), "APTicket is not valid");
        }else{
            plist_t ticket = NULL;
            try {
                ticket = _aptickets.at(0);
            } catch (...) {
                reterror("Failed to get APTicket");
            }
            retassure(img3tool::verifySignedIMG3FileForSHSH(_donorLLB.data(), _donorLLB.size(), ticket), "APTicket is not valid for donorLLB");
        }
        info("APTicket is valid for donorLLB!");
    }
    _donorIPSW = llbipswpath;
}

const char *futurerestore::getDeviceBoardNoCopy(){
    if (!_client->device || !_client->device->product_type){
        auto mode = getDeviceMode();
        retassure(mode == MODE_NORMAL || mode == MODE_RECOVERY || mode == MODE_DFU || mode == MODE_RESTORE, "unexpected device mode=%d",mode->index);
        
        switch (mode->index) {
        case _MODE_RESTORE:
            _client->device = restore_get_irecv_device(_client);
            break;
        case _MODE_NORMAL:
            _client->device = normal_get_irecv_device(_client);
            break;
        case _MODE_RECOVERY:
            _client->device = recovery_get_irecv_device(_client);
            break;
        case _MODE_DFU:
            dfu_client_free(_client);
            _client->device = dfu_get_irecv_device(_client);
            break;
        default:
            break;
        }
    }
    retassure(_client->device,"Failed to get device board");
    return _client->device->hardware_model;
}

const char *futurerestore::getDeviceModelNoCopy(){
    if (!_client->device || !_client->device->product_type){
        auto mode = getDeviceMode();
        retassure(mode == MODE_NORMAL || mode == MODE_RECOVERY || mode == MODE_DFU, "unexpected device mode=%d",mode);
        
        switch (mode->index) {
        case _MODE_RESTORE:
            _client->device = restore_get_irecv_device(_client);
            break;
        case _MODE_NORMAL:
            _client->device = normal_get_irecv_device(_client);
            break;
        case _MODE_RECOVERY:
            _client->device = recovery_get_irecv_device(_client);
            break;
        case _MODE_DFU:
            dfu_client_free(_client);
            _client->device = dfu_get_irecv_device(_client);
            break;
        default:
            break;
        }
    }
    retassure(_client->device,"Failed to get device board");
    return _client->device->product_type;
}

plist_t futurerestore::nonceMatchesApTickets(){
    reterror("todo");
}

tihmstar::Mem futurerestore::nonceMatchesIM4Ms(){
    reterror("todo");
//    retassure(_didInit, "did not init");
//
//    retassure(getDeviceMode(true) == MODE_RECOVERY, "Device is not in recovery mode, can't check ApNonce");
//
//    unsigned char* realnonce;
//    int realNonceSize = 0;
//    recovery_get_ap_nonce(_client, &realnonce, &realNonceSize);
//
//    vector<const char*>nonces;
//
//    if (_client->image4supported) {
//        for (int i=0; i< _im4ms.size(); i++){
//            auto nonce = img4tool::getValFromIM4M({_im4ms[i].first,_im4ms[i].second}, 'BNCH');
//            if (nonce.payloadSize() == realNonceSize && memcmp(realnonce, nonce.payload(), realNonceSize) == 0) return _im4ms[i];
//        }
//    }else{
//        for (int i=0; i< _im4ms.size(); i++){
//            size_t ticketNonceSize = 0;
//            const char *nonce = NULL;
//            try {
//                //nonce might not exist, which we use in re-restoring iOS 9.x for 32-bit
//                auto n = getNonceFromSCAB(_im4ms[i].first, _im4ms[i].second);
//                ticketNonceSize = n.second;
//                nonce = n.first;
//            } catch (...) {
//                //
//            }
//            if (memcmp(realnonce, nonce, ticketNonceSize) == 0) return _im4ms[i];
//        }
//    }
//
//    return {};
}

uint64_t futurerestore::getDeviceEcid(){
    uint64_t ecid = _client->ecid;
    retassure(ecid, "Failed to get ecid");
    return ecid;
}

uint32_t futurerestore::getDeviceCPID(){
    uint32_t cpid = 0;
    auto mode = getDeviceMode();
    switch (mode->index) {
        case _MODE_WTF:
            return 0x8900; //there is no other device with WTF mode
            
        case _MODE_DFU:
            retassure(!dfu_get_cpid(_client, &cpid),"Failed to get CPID in DFU mode");
            break;
        case _MODE_RECOVERY:
            retassure(!recovery_get_cpid(_client, &cpid),"Failed to get CPID in recovery mode");
            break;
        default:
            reterror("Found device in unexpected mode '%s'",mode->string);
            break;
    }
    return cpid;
}

uint32_t futurerestore::getDeviceBDID(){
    uint32_t bdid = 0;
    auto mode = getDeviceMode();
    switch (mode->index) {
        case _MODE_DFU:
            retassure(!dfu_get_bdid(_client, &bdid),"Failed to get BDID in DFU mode");
            break;
        case _MODE_RECOVERY:
            retassure(!recovery_get_bdid(_client, &bdid),"Failed to get BDID in recovery mode");
            break;
            
        default:
            reterror("Found device in unexpected mode '%s'",mode->string);
            break;
    }
    return bdid;
}

void futurerestore::requestFreshAPTicket(plist_t p_BuildManifest, const char *variant){
    uint8_t *sepnonce = NULL;
    plist_t p_APTicket = NULL;
    cleanup([&]{
        safeFreeCustom(p_APTicket, plist_free);
        safeFree(sepnonce);
    });
    size_t sepnonceSize = 0;
    int err = 0;
    
    if (!variant) variant = RESTORE_VARIANT_ERASE_INSTALL;
    tsschecker::TssRequest req(p_BuildManifest,variant);
    req.setDeviceVals(getDeviceCPID(), getDeviceBDID());
    req.setEcid(getDeviceEcid());
    
    retassure(!(err = get_ap_nonce(_client, &_client->nonce, &_client->nonce_size)), "Failed to get APNonce with err=%d",err);
    if (_client->nonce_size) req.setAPNonce(_client->nonce,_client->nonce_size);
    
    retassure(!(err = get_sep_nonce(_client, &sepnonce, (unsigned int*)&sepnonceSize)), "Failed to get SEPNonce with err=%d",err);
    if (sepnonceSize) req.setSEPNonce(sepnonce,sepnonceSize);
    
    req.addDefaultAPTagsToRequest();
    req.addAllAPComponentsToRequest();
    
    p_APTicket = req.getTSSResponce();
    {
        char *im4m = NULL;
        cleanup([&]{
            safeFree(im4m);
        });
        uint64_t im4msize=0;
        plist_t ticket = plist_dict_get_item(p_APTicket, (_client->image4supported) ? "ApImg4Ticket" : "APTicket");
        plist_get_data_val(ticket, &im4m, &im4msize);
        if (!im4msize){
            warning("SHSH file does NOT contain APTicket!");
        }
        _im4ms.push_back({im4m,im4msize}); im4m = NULL; im4msize = 0;
    }
    _aptickets.push_back(p_APTicket); p_APTicket = NULL;
}

void futurerestore::requestFreshSEPTicket(plist_t p_BuildManifest, const char *variant){
    uint8_t *sepnonce;
    plist_t p_APTicket = NULL;
    cleanup([&]{
        safeFreeCustom(p_APTicket, plist_free);
        safeFree(sepnonce);
    });
    size_t sepnonceSize = 0;
    int err = 0;
    
    if (!variant) variant = RESTORE_VARIANT_ERASE_INSTALL;
    tsschecker::TssRequest req(p_BuildManifest,variant);
    req.setDeviceVals(getDeviceCPID(), getDeviceBDID());
    req.setEcid(getDeviceEcid());
    
    retassure(!(err = get_ap_nonce(_client, &_client->nonce, &_client->nonce_size)), "Failed to get APNonce with err=%d",err);
    if (_client->nonce_size) req.setAPNonce(_client->nonce,_client->nonce_size);
    
    retassure(!(err = get_sep_nonce(_client, &sepnonce, (unsigned int*)&sepnonceSize)), "Failed to get SEPNonce with err=%d",err);
    if (sepnonceSize) req.setSEPNonce(sepnonce,sepnonceSize);
    
    req.addDefaultAPTagsToRequest();
    req.addComponent("SEP");
    req.addComponent("RestoreSEP");

    p_APTicket = req.getTSSResponce();
    
    {
        char *im4m = NULL;
        cleanup([&]{
            safeFree(im4m);
        });
        uint64_t im4msize=0;
        plist_t ticket = plist_dict_get_item(p_APTicket, (_client->image4supported) ? "ApImg4Ticket" : "APTicket");
        plist_get_data_val(ticket, &im4m, &im4msize);
        retassure(im4msize, "SHSH file does NOT contain SEP-APTicket!");
        _sepTicket = {im4m,im4msize}; im4m = NULL; im4msize = 0;
    }
}

void futurerestore::requestFreshAPTicket(const char *ipswpath, const char *variant){
    char *buf = NULL;
    plist_t p_BuildManifest = NULL;
    ipsw_archive_t ipsw = NULL;
    cleanup([&]{
        safeFreeCustom(ipsw, ipsw_close);
        safeFreeCustom(p_BuildManifest, plist_free);
        safeFree(buf);
    });
    size_t bufSize = 0;
    int err = 0;
    plist_err_t perr = PLIST_ERR_SUCCESS;
    
    retassure(ipsw = ipsw_open(ipswpath), "Failed to open ipsw '%s'",ipswpath);
    
    retassure(!(err = ipsw_extract_to_memory(ipsw, "BuildManifest.plist", (void**)&buf, &bufSize)), "Failed to extract BuildManifest.plist with err=%d from ipsw at path=%s",err,ipsw);
    retassure(!(perr = plist_from_memory(buf, (uint32_t)bufSize, &p_BuildManifest, NULL)),"Failed to parse BuildManifest.plist");
    return requestFreshAPTicket(p_BuildManifest, variant);
}

void futurerestore::fetchLatestBaseband(){
    plist_t p_latestManifest = loadBuilmanifestForLatestFirmware();
    plist_t p_BuildIdentity = tsschecker::TssRequest::getBuildIdentityForDevice(p_latestManifest, getDeviceCPID(), getDeviceBDID());

    safeFreeCustom(_client->bbfw_buildidentity, plist_free);
    _client->bbfw_buildidentity = plist_copy(p_BuildIdentity);
    
    auto bbfwpath = tsschecker::TssRequest::getPathForComponentBuildIdentity(p_BuildIdentity, "BasebandFirmware");
    auto bbfw = tsschecker::downloadFileFromIPSW(_latestFirmwareUrl.c_str(), bbfwpath.c_str());
    tihmstar::writeFile(BASEBAND_TMP_PATH, bbfw.data(), bbfw.size());
    safeFree(_client->bbfwpath);
    _client->bbfwpath = strdup(BASEBAND_TMP_PATH);
}

void futurerestore::fetchLatestSEP(){
    plist_t p_latestManifest = loadBuilmanifestForLatestFirmware();
    plist_t p_BuildIdentity = tsschecker::TssRequest::getBuildIdentityForDevice(p_latestManifest, getDeviceCPID(), getDeviceBDID());

    safeFreeCustom(_client->bbfw_buildidentity, plist_free);
    _client->bbfw_buildidentity = plist_copy(p_BuildIdentity);
    
    auto fwpath = tsschecker::TssRequest::getPathForComponentBuildIdentity(p_BuildIdentity, "SEP");
    _sep = tsschecker::downloadFileFromIPSW(_latestFirmwareUrl.c_str(), fwpath.c_str());
    requestFreshSEPTicket(p_latestManifest);
}

void futurerestore::setCustomBasebandPath(const char *basebandPath){
    safeFree(_client->bbfwpath);
    _client->bbfwpath = strdup(basebandPath);
    retassure(!tihmstar::fileExists(_client->bbfwpath), "Faile does not exist at path '%s'",_client->bbfwpath);
}

void futurerestore::setCustomSEPPath(const char *sepPath){
    _sep = tihmstar::readFile(sepPath);
}

void futurerestore::setCustomBasebandManifest(plist_t basebandManifest){
    safeFreeCustom(_client->bbfw_buildidentity, plist_free);
    _client->bbfw_buildidentity = plist_copy(basebandManifest);
}

void futurerestore::setCustomSEPManifest(plist_t SEPManifest){
    requestFreshSEPTicket(SEPManifest);
}

void futurerestore::setCustomRamdisk(const char *ramdiskPath){
    _customRamdisk = readFile(ramdiskPath);
}

void futurerestore::verifyBasebandTicketCanBeRequested(){
    if (_client->bbfw_buildidentity) {
        info("Checking whether Baseband Ticket can be requested");
        tsschecker::TssRequest req(_client->bbfw_buildidentity, "", true);
        req.addBasebandComponentsToRequest();
        req.setDeviceVals(getDeviceCPID(), getDeviceBDID());
        req.setEcid(getDeviceEcid());
        req.setDefaultBbGoldCertId();
        req.setRandomSNUM();
        try {
            plist_t p_BBTicket = NULL;
            cleanup([&]{
                safeFreeCustom(p_BBTicket, plist_free);
            });
            p_BBTicket = req.getTSSResponce();
        }catch (tihmstar::exception &e){
            reterror("Failed to verify that requesting Baseband ticket is possible. Erro:\n%s",e.dumpStr().c_str());
        }
        info("Success checking Baseband ticket availablility!");
    }else{
        info("Custom Baseband ticket was not requested for this restore");
        retassure(!tsschecker::getGoldCertIDForDevice(tsschecker::getProductTypeFromCPIDandBDID(getDeviceCPID(), getDeviceBDID())), "Custom Baseband ticket was not requested, but device has a baseband!");
    }
}

#pragma mark static
tihmstar::Mem futurerestore::getRamdiskHashFromSCAB(const void* scab, size_t scabSize){
    retassure(scab, "Got empty SCAB");

    img4tool::ASN1DERElement bacs(scab,scabSize);

    try {
        bacs[3];
    } catch (...) {
        reterror("unexpected number of Elements in SCAB sequence (expects 4)");
    }

    img4tool::ASN1DERElement mainSet = bacs[1];

    for (auto &elem : mainSet) {
        if (*(uint8_t*)elem.buf() == 0x9A) {
            return {(const uint8_t*)elem.payload(),elem.payloadSize()};
        }
    }
    reterror("failed to get nonce from SCAB");
    return {};
}

uint64_t futurerestore::getEcidFromSCAB(const void* scab, size_t scabSize){
    retassure(scab, "Got empty SCAB");
    
    img4tool::ASN1DERElement bacs(scab,scabSize);
    
    try {
        bacs[3];
    } catch (...) {
        reterror("unexpected number of Elements in SCAB sequence (expects 4)");
    }
    
    img4tool::ASN1DERElement mainSet = bacs[1];

    for (auto &elem : mainSet) {
        if (*(uint8_t*)elem.buf() == 0x81) {
            uint64_t ret = 0;
            for (int i=(int)(elem.payloadSize()-1); i>=0; i--) {
                ret <<=8;
                ret |= ((uint8_t*)elem.payload())[i];
            }
            return ret;
        }
    }

    reterror("failed to get ECID from SCAB");
    return 0;
}

