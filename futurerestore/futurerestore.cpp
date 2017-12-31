//
//  futurerestore.cpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#if defined _WIN32 || defined __CYGWIN__
#ifndef WIN32
//make sure WIN32 is defined if compiling for windows
#define WIN32
#endif
#endif

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <zlib.h>
#include "futurerestore.hpp"
#ifdef HAVE_LIBIPATCHER
#include <libipatcher/libipatcher.hpp>
#endif
extern "C"{
#include "common.h"
#include "../external/img4tool/img4tool/img4.h"
#include "img4tool.h"
#include "normal.h"
#include "recovery.h"
#include "dfu.h"
#include "ipsw.h"
#include "locking.h"
#include "restore.h"
#include "tsschecker.h"
#include "all_tsschecker.h"
#include <libirecovery.h>
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

#define USEC_PER_SEC 1000000

#define TMP_PATH "/tmp"
#define FUTURERESTORE_TMP_PATH TMP_PATH"/futurerestore"

#define BASEBAND_TMP_PATH FUTURERESTORE_TMP_PATH"/baseband.bbfw"
#define BASEBAND_MANIFEST_TMP_PATH FUTURERESTORE_TMP_PATH"/basebandManifest.plist"
#define SEP_TMP_PATH FUTURERESTORE_TMP_PATH"/sep.im4p"
#define SEP_MANIFEST_TMP_PATH FUTURERESTORE_TMP_PATH"/sepManifest.plist"

#ifdef __APPLE__
#   include <CommonCrypto/CommonDigest.h>
#   define SHA1(d, n, md) CC_SHA1(d, n, md)
#   define SHA384(d, n, md) CC_SHA384(d, n, md)
#else
#   include <openssl/sha.h>
#endif // __APPLE__

#ifndef HAVE_LIBIPATCHER
#define _enterPwnRecoveryRequested false
#endif

#define reterror(code,msg ...) error(msg),throw int(code)
#define safeFree(buf) if (buf) free(buf), buf = NULL
#define safePlistFree(buf) if (buf) plist_free(buf), buf = NULL

futurerestore::futurerestore(bool isUpdateInstall, bool isPwnDfu) : _isUpdateInstall(isUpdateInstall), _isPwnDfu(isPwnDfu){
    _client = idevicerestore_client_new();
    if (_client == NULL) throw std::string("could not create idevicerestore client\n");
    
    struct stat st{0};
    if (stat(FUTURERESTORE_TMP_PATH, &st) == -1) __mkdir(FUTURERESTORE_TMP_PATH, 0755);
    
    //tsschecker nocache
    nocache = 1;
    _foundnonce = -1;
}

bool futurerestore::init(){
    if (_didInit) return _didInit;
    _didInit = (check_mode(_client) != MODE_UNKNOWN);
    if (!(_client->image4supported = is_image4_supported(_client))){
        info("[INFO] 32bit device detected\n");
    }else{
        info("[INFO] 64bit device detected\n");
        if (_isPwnDfu) reterror(-90, "isPwnDfu is only allowed for 32bit devices\n");
    }
    return _didInit;
}

uint64_t futurerestore::getDeviceEcid(){
    if (!_didInit) reterror(-1, "did not init\n");
    uint64_t ecid;
    
    get_ecid(_client, &ecid);
    
    return ecid;
}

int futurerestore::getDeviceMode(bool reRequest){
    if (!_didInit) reterror(-1, "did not init\n");
    if (!reRequest && _client->mode && _client->mode->index != MODE_UNKNOWN) {
        return _client->mode->index;
    }else{
        normal_client_free(_client);
        dfu_client_free(_client);
        recovery_client_free(_client);
        return check_mode(_client);
    }
}

void futurerestore::putDeviceIntoRecovery(){
    if (!_didInit) reterror(-1, "did not init\n");
    
#ifdef HAVE_LIBIPATCHER
    _enterPwnRecoveryRequested = _isPwnDfu;
#endif
    
    getDeviceMode(false);
    info("Found device in %s mode\n", _client->mode->string);
    if (_client->mode->index == MODE_NORMAL){
#ifdef HAVE_LIBIPATCHER
        if (_isPwnDfu)
            reterror(-501, "isPwnDfu enabled, but device was found in normal mode\n");
#endif
        info("Entering recovery mode...\n");
        if (normal_enter_recovery(_client) < 0) {
            reterror(-2,"Unable to place device into recovery mode from %s mode\n", _client->mode->string);
        }
    }else if (_client->mode->index == MODE_RECOVERY){
        info("Device already in Recovery mode\n");
    }else if (_client->mode->index == MODE_DFU && _isPwnDfu &&
#ifdef HAVE_LIBIPATCHER
              true
#else
              false
#endif
              ){
        info("requesting to get into pwnRecovery later\n");
    }else if (!_client->image4supported){
        info("32bit device in DFU mode found, assuming user wants to use iOS9 re-restore bug. Not failing here\n");
    }else{
        reterror(-3, "unsupported devicemode, please put device in recovery mode or normal mode\n");
    }
    
    //only needs to be freed manually when function did't throw exception
    safeFree(_client->udid);
    
    //these get also freed by destructor
    normal_client_free(_client);
    dfu_client_free(_client);
    recovery_client_free(_client);
}

void futurerestore::setAutoboot(bool val){
    if (!_didInit) reterror(-1, "did not init\n");
    
    if (getDeviceMode(false) != MODE_RECOVERY){
        reterror(-2, "can't set autoboot, when device isn't in recovery mode\n");
    }
    
    if (_client->recovery || recovery_client_new(_client) == 0) {
        if (recovery_set_autoboot(_client, val)){
            reterror(-3,"Setting auto-boot failed?!\n");
        }
    } else {
        reterror(-4,"Could not connect to device in recovery mode.\n");
    }
}

plist_t futurerestore::nonceMatchesApTickets(){
    if (!_didInit) reterror(-1, "did not init\n");
    if (getDeviceMode(true) != MODE_RECOVERY){
        if (getDeviceMode(false) != MODE_DFU || *_client->version != '9')
            reterror(-10, "Device not in recovery mode, can't check apnonce\n");
        else
            _rerestoreiOS9 = (info("Detected iOS 9 re-restore, proceeding in DFU mode\n"),true);
    }
    
    
    unsigned char* realnonce;
    int realNonceSize = 0;
    if (_rerestoreiOS9) {
        info("Skipping APNonce check\n");
    }else{
        recovery_get_ap_nonce(_client, &realnonce, &realNonceSize);
        
        info("Got APNonce from device: ");
        int i = 0;
        for (i = 0; i < realNonceSize; i++) {
            info("%02x ", ((unsigned char *)realnonce)[i]);
        }
        info("\n");
    }
    
    vector<const char*>nonces;
    
    if (_client->image4supported){
        for (int i=0; i< _im4ms.size(); i++){
            if (memcmp(realnonce, (unsigned const char*)getBNCHFromIM4M(_im4ms[i],NULL), realNonceSize) == 0) return _aptickets[i];
        }
    }else{
        for (int i=0; i< _im4ms.size(); i++){
            size_t ticketNonceSize = 0;
            if (memcmp(realnonce, (unsigned const char*)getNonceFromSCAB(_im4ms[i], &ticketNonceSize), ticketNonceSize) == 0 &&
                 (  (ticketNonceSize == realNonceSize && realNonceSize+ticketNonceSize > 0) ||
                        (!ticketNonceSize && *_client->version == '9' &&
                            (getDeviceMode(false) == MODE_DFU ||
                                ( getDeviceMode(false) == MODE_RECOVERY && !strncmp(getiBootBuild(), "iBoot-2817", strlen("iBoot-2817")) )
                            )
                         )
                 )
               )
                //either nonce needs to match or using re-restore bug in iOS 9
                return _aptickets[i];
        }
    }
    
    
    return NULL;
}

const char *futurerestore::nonceMatchesIM4Ms(){
    if (!_didInit) reterror(-1, "did not init\n");
    if (getDeviceMode(true) != MODE_RECOVERY) reterror(-10, "Device not in recovery mode, can't check apnonce\n");
    
    unsigned char* realnonce;
    int realNonceSize = 0;
    recovery_get_ap_nonce(_client, &realnonce, &realNonceSize);
    
    vector<const char*>nonces;
    
    if (_client->image4supported) {
        for (int i=0; i< _im4ms.size(); i++){
            if (memcmp(realnonce, (unsigned const char*)getBNCHFromIM4M(_im4ms[i],NULL), realNonceSize) == 0) return _im4ms[i];
        }
    }else{
        for (int i=0; i< _im4ms.size(); i++){
            if (memcmp(realnonce, (unsigned const char*)getNonceFromSCAB(_im4ms[i],(size_t*)&realNonceSize), realNonceSize) == 0) return _im4ms[i];
        }
    }
    
    
    return NULL;
}



void futurerestore::waitForNonce(vector<const char *>nonces, size_t nonceSize){
    if (!_didInit) reterror(-1, "did not init\n");
    setAutoboot(false);
    
    unsigned char* realnonce;
    int realNonceSize = 0;
    
    for (auto nonce : nonces){
        info("waiting for nonce: ");
        int i = 0;
        for (i = 0; i < nonceSize; i++) {
            info("%02x ", ((unsigned char *)nonce)[i]);
        }
        info("\n");
    }
    
    do {
        if (realNonceSize){
            recovery_send_reset(_client);
            recovery_client_free(_client);
            usleep(1*USEC_PER_SEC);
        }
        while (getDeviceMode(true) != MODE_RECOVERY) usleep(USEC_PER_SEC*0.5);
        if (recovery_client_new(_client)) reterror(-4,"Could not connect to device in recovery mode.\n");
        
        recovery_get_ap_nonce(_client, &realnonce, &realNonceSize);
        info("Got ApNonce from device: ");
        int i = 0;
        for (i = 0; i < realNonceSize; i++) {
            info("%02x ", realnonce[i]);
        }
        info("\n");
        for (int i=0; i<nonces.size(); i++){
            if (memcmp(realnonce, (unsigned const char*)nonces[i], realNonceSize) == 0) _foundnonce = i;
        }
    } while (_foundnonce == -1);
    info("Device has requested ApNonce now\n");
    
    setAutoboot(true);
}
void futurerestore::waitForNonce(){
    if (!_im4ms.size()) reterror(-1, "No IM4M loaded\n");
    size_t nonceSize = 0;
    vector<const char*>nonces;
    
    if (!_client->image4supported)
        reterror(-77, "Error: waitForNonce is not supported on 32bit devices\n");
    
    for (auto im4m : _im4ms){
        nonces.push_back(getBNCHFromIM4M(im4m,&nonceSize));
    }
    
    waitForNonce(nonces,nonceSize);
}

void futurerestore::loadAPTickets(const vector<const char *> &apticketPaths){
    for (auto apticketPath : apticketPaths){
        plist_t apticket = NULL;
        char *im4m = NULL;
        
        struct stat fst;
        if (stat(apticketPath, &fst))
            reterror(-9, "failed to load apticket at %s\n",apticketPath);
        
        gzFile zf = gzopen(apticketPath, "rb");
        if (zf) {
            int blen = 0;
            int readsize = 16384; //0x4000
            int bufsize = readsize;
            char* bin = (char*)malloc(bufsize);
            char* p = bin;
            do {
                int bytes_read = gzread(zf, p, readsize);
                if (bytes_read < 0)
                    reterror(-39, "Error reading gz compressed data\n");
                blen += bytes_read;
                if (bytes_read < readsize) {
                    if (gzeof(zf)) {
                        bufsize += bytes_read;
                        break;
                    }
                }
                bufsize += readsize;
                bin = (char*)realloc(bin, bufsize);
                p = bin + blen;
            } while (!gzeof(zf));
            gzclose(zf);
            if (blen > 0) {
                if (memcmp(bin, "bplist00", 8) == 0)
                    plist_from_bin(bin, blen, &apticket);
                else
                    plist_from_xml(bin, blen, &apticket);
            }
            free(bin);
        }
        
        if (_isUpdateInstall) {
            if(plist_t update =  plist_dict_get_item(apticket, "updateInstall")){
                plist_t cpy = plist_copy(update);
                plist_free(apticket);
                apticket = cpy;
            }
        }
        
        plist_t ticket = plist_dict_get_item(apticket, (_client->image4supported) ? "ApImg4Ticket" : "APTicket");
        uint64_t im4msize=0;
        plist_get_data_val(ticket, &im4m, &im4msize);
        
        if (!im4msize)
            reterror(-38, "Error: failed to load shsh file %s\n",apticketPath);
        
        _im4ms.push_back(im4m);
        _aptickets.push_back(apticket);
        printf("reading ticket %s done\n",apticketPath);
    }
}

uint64_t futurerestore::getBasebandGoldCertIDFromDevice(){
    if (!_client->preflight_info){
        if (normal_get_preflight_info(_client, &_client->preflight_info) == -1){
            printf("[WARNING] failed to read BasebandGoldCertID from device! Is it already in recovery?\n");
            return 0;
        }
    }
    plist_t node;
    node = plist_dict_get_item(_client->preflight_info, "CertID");
    if (!node || plist_get_node_type(node) != PLIST_UINT) {
        error("Unable to find required BbGoldCertId in parameters\n");
        return 0;
    }
    uint64_t val = 0;
    plist_get_uint_val(node, &val);
    return val;
}

char *futurerestore::getiBootBuild(){
    if (!_ibootBuild){
        if (_client->recovery == NULL) {
            if (recovery_client_new(_client) < 0) {
                reterror(-77, "Error: can't create new recovery client");
            }
        }
        irecv_getenv(_client->recovery->client, "build-version", &_ibootBuild);
        if (!_ibootBuild)
            reterror(-78, "Error: can't get build-version");
    }
    return _ibootBuild;
}


pair<ptr_smart<char*>, size_t> getIPSWComponent(struct idevicerestore_client_t* client, plist_t build_identity, string component){
    ptr_smart<char *> path;
    unsigned char* component_data = NULL;
    unsigned int component_size = 0;

    if (!(char*)path) {
        if (build_identity_get_component_path(build_identity, component.c_str(), &path) < 0) {
            reterror(-95,"ERROR: Unable to get path for component '%s'\n", component.c_str());
        }
    }
    
    if (extract_component(client->ipsw, (char*)path, &component_data, &component_size) < 0) {
        reterror(-95,"ERROR: Unable to extract component: %s\n", component.c_str());
    }
    
    return {(char*)component_data,component_size};
}


void futurerestore::enterPwnRecovery(plist_t build_identity, string bootargs){
#ifndef HAVE_LIBIPATCHER
    reterror(-404, "compiled without libipatcher");
#else
    int mode = 0;
    libipatcher::fw_key iBSSKeys;
    libipatcher::fw_key iBECKeys;
    
    if (dfu_client_new(_client) < 0)
        reterror(-91,"Unable to connect to DFU device\n");
    irecv_get_mode(_client->dfu->client, &mode);
    
    
    try {
        iBSSKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBSS");
        iBECKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBEC");
    } catch (libipatcher::exception e) {
        string err = "failed getting keys with error: " + to_string(e.code()) + "( " + e.what() + " ) ";
        reterror(e.code(), "getting keys failed. Are keys publicly available?");
    }
    
    
    auto iBSS = getIPSWComponent(_client, build_identity, "iBSS");
    iBSS = move(libipatcher::patchiBSS((char*)iBSS.first, iBSS.second, iBSSKeys));
    
    
    auto iBEC = getIPSWComponent(_client, build_identity, "iBEC");
    iBEC = move(libipatcher::patchiBEC((char*)iBEC.first, iBEC.second, iBECKeys, bootargs));
    
    bool modeIsRecovery = false;
    if (mode != IRECV_K_DFU_MODE) {
        info("NOTE: device is not in DFU mode, assuming pwn recovery mode.\n");
        for (int i=IRECV_K_RECOVERY_MODE_1; i<=IRECV_K_RECOVERY_MODE_4; i++) {
            if (mode == i)
                modeIsRecovery = true;
        }
        if (!modeIsRecovery)
            reterror(-505, "device not in recovery mode\n");
    }else{
        info("Sending %s (%lu bytes)...\n", "iBSS", iBSS.second);
        // FIXME: Did I do this right????
        irecv_error_t err = irecv_send_buffer(_client->dfu->client, (unsigned char*)(char*)iBSS.first, (unsigned long)iBSS.second, 1);
        if (err != IRECV_E_SUCCESS) {
            reterror(-92,"ERROR: Unable to send %s component: %s\n", "iBSS", irecv_strerror(err));
        }
    }
    
    if (_client->build_major > 8) {
        /* reconnect */
        dfu_client_free(_client);
        sleep(3);
        dfu_client_new(_client);
        
        if (irecv_usb_set_configuration(_client->dfu->client, 1) < 0) {
           reterror(-92,"ERROR: set configuration failed\n");
        }
        
        /* send iBEC */
        info("Sending %s (%lu bytes)...\n", "iBEC", iBEC.second);
        // FIXME: Did I do this right????
        irecv_error_t err = irecv_send_buffer(_client->dfu->client, (unsigned char*)(char*)iBEC.first, (unsigned long)iBEC.second, 1);
        if (err != IRECV_E_SUCCESS) {
            reterror(-92,"ERROR: Unable to send %s component: %s\n", "iBSS", irecv_strerror(err));
        }
        if (modeIsRecovery)
            irecv_send_command(_client->dfu->client, "go");
    }
    
    dfu_client_free(_client);
    
    sleep(7);
    
    // Reconnect to device, but this time make sure we're not still in DFU mode
    if (recovery_client_new(_client) < 0) {
        if (_client->recovery->client) {
            irecv_close(_client->recovery->client);
            _client->recovery->client = NULL;
        }
        reterror(-93,"ERROR: Unable to connect to recovery device\n");
    }
    
    irecv_get_mode(_client->recovery->client, &mode);
    
    if (mode == IRECV_K_DFU_MODE) {
        if (_client->recovery->client) {
            irecv_close(_client->recovery->client);
            _client->recovery->client = NULL;
        }
        reterror(-94,"ERROR: Unable to connect to recovery device\n");
    }
#endif
}

void get_custom_component(struct idevicerestore_client_t* client, plist_t build_identity, const char* component, unsigned char** data, unsigned int *size){
#ifndef HAVE_LIBIPATCHER
    reterror(-404, "compiled without libipatcher");
#else
    try {
        auto comp = getIPSWComponent(client, build_identity, component);
        comp = move(libipatcher::decryptFile3((char*)comp.first, comp.second, libipatcher::getFirmwareKey(client->device->product_type, client->build, component)));
        *data = (unsigned char*)(char*)comp.first;
        *size = comp.second;
        comp.first = NULL; //don't free on destruction
    } catch (libipatcher::exception &e) {
        reterror(e.code(),"ERROR: libipatcher failed with reason %s\n",e.what());
    }
    
#endif
}


int futurerestore::doRestore(const char *ipsw){
    int err = 0;
    //some memory might not get freed if this function throws an exception, but you probably don't want to catch that anyway.
    
    struct idevicerestore_client_t* client = _client;
    int unused;
    int result = 0;
    int delete_fs = 0;
    char* filesystem = NULL;
    plist_t buildmanifest = NULL;
    plist_t build_identity = NULL;
    plist_t sep_build_identity = NULL;
    
    client->ipsw = strdup(ipsw);
    if (!_isUpdateInstall) client->flags |= FLAG_ERASE;
    else if (_enterPwnRecoveryRequested){
        reterror(577, "ERROR: Update install is not supported in pwnDFU mode\n");
    }
    
    getDeviceMode(true);
    info("Found device in %s mode\n", client->mode->string);

    if (client->mode->index != MODE_RECOVERY && client->mode->index != MODE_DFU && !_enterPwnRecoveryRequested)
        reterror(-6, "device not in recovery mode\n");
    // discover the device type
    if (check_hardware_model(client) == NULL || client->device == NULL) {
        reterror(-2,"ERROR: Unable to discover device model\n");
    }
    info("Identified device as %s, %s\n", client->device->hardware_model, client->device->product_type);
    
    // verify if ipsw file exists
    if (access(client->ipsw, F_OK) < 0) {
        error("ERROR: Firmware file %s does not exist.\n", client->ipsw);
        return -1;
    }
    info("Extracting BuildManifest from IPSW\n");
    if (ipsw_extract_build_manifest(client->ipsw, &buildmanifest, &unused) < 0) {
        reterror(-3,"ERROR: Unable to extract BuildManifest from %s. Firmware file might be corrupt.\n", client->ipsw);
    }
    /* check if device type is supported by the given build manifest */
    if (build_manifest_check_compatibility(buildmanifest, client->device->product_type) < 0) {
        reterror(-4,"ERROR: Could not make sure this firmware is suitable for the current device. Refusing to continue.\n");
    }
    /* print iOS information from the manifest */
    build_manifest_get_version_information(buildmanifest, client);
    
    info("Product Version: %s\n", client->version);
    info("Product Build: %s Major: %d\n", client->build, client->build_major);
    
    client->image4supported = is_image4_supported(client);
    info("Device supports Image4: %s\n", (client->image4supported) ? "true" : "false");
    
    if (_enterPwnRecoveryRequested) //we are in pwnDFU, so we don't need to check nonces
        client->tss = _aptickets.at(0);
    else if (!(client->tss = nonceMatchesApTickets()))
        reterror(-20, "Devicenonce does not match APTicket nonce\n");
    
    plist_dict_remove_item(client->tss, "BBTicket");
    plist_dict_remove_item(client->tss, "BasebandFirmware");
    
    if (!(build_identity = getBuildidentityWithBoardconfig(buildmanifest, client->device->hardware_model, _isUpdateInstall)))
        reterror(-5,"ERROR: Unable to find any build identities for IPSW\n");

    if (_client->image4supported && !(sep_build_identity = getBuildidentityWithBoardconfig(_sepbuildmanifest, client->device->hardware_model, _isUpdateInstall)))
        reterror(-5,"ERROR: Unable to find any build identities for SEP\n");

    //this is the buildidentity used for restore
    plist_t manifest = plist_dict_get_item(build_identity, "Manifest");

    printf("checking APTicket to be valid for this restore...\n");
    //if we are in pwnDFU, just use first apticket. no need to check nonces
    const char * im4m = (_enterPwnRecoveryRequested || _rerestoreiOS9) ? _im4ms.at(0) : nonceMatchesIM4Ms();
    
    uint64_t deviceEcid = getDeviceEcid();
    uint64_t im4mEcid = (_client->image4supported) ? getEcidFromIM4M(im4m) : getEcidFromSCAB(im4m);
    if (!im4mEcid)
        reterror(-46, "Failed to read ECID from APTicket\n");
    
    if (im4mEcid != deviceEcid) {
        error("ECID inside APTicket does not match device ECID\n");
        printf("APTicket is valid for %16llu (dec) but device is %16llu (dec)\n",im4mEcid,deviceEcid);
        reterror(-45, "APTicket can't be used for restoring this device\n");
    }else
        printf("Verified ECID in APTicket matches device ECID\n");
    
    if (_client->image4supported) {
        printf("checking APTicket to be valid for this restore...\n");
        const char * im4m = nonceMatchesIM4Ms();
        
        uint64_t deviceEcid = getDeviceEcid();
        uint64_t im4mEcid = (_client->image4supported) ? getEcidFromIM4M(im4m) : getEcidFromSCAB(im4m);
        if (!im4mEcid)
            reterror(-46, "Failed to read ECID from APTicket\n");
        
        if (im4mEcid != deviceEcid) {
            error("ECID inside APTicket does not match device ECID\n");
            printf("APTicket is valid for %16llu (dec) but device is %16llu (dec)\n",im4mEcid,deviceEcid);
            reterror(-45, "APTicket can't be used for restoring this device\n");
        }else
            printf("Verified ECID in APTicket matches device ECID\n");
        
        plist_t ticketIdentity = getBuildIdentityForIM4M(im4m, buildmanifest);
        //TODO: make this nicer!
        //for now a simple pointercompare should be fine, because both plist_t should point into the same buildidentity inside the buildmanifest
        if (ticketIdentity != build_identity ){
            error("BuildIdentity selected for restore does not match APTicket\n\n");
            printf("BuildIdentity selected for restore:\n");
            printGeneralBuildIdentityInformation(build_identity);
            printf("\nBuildIdentiy valid for the APTicket:\n");
            
            if (ticketIdentity) printGeneralBuildIdentityInformation(ticketIdentity),putchar('\n');
            else{
                printf("IM4M is not valid for any restore within the Buildmanifest\n");
                printf("This APTicket can't be used for restoring this firmware\n");
            }
            reterror(-44, "APTicket can't be used for this restore\n");
        }else{
            if (verifyIM4MSignature(im4m)){
                printf("IM4M signature is not valid!\n");
                reterror(-44, "APTicket can't be used for this restore\n");
            }
            printf("Verified APTicket to be valid for this restore\n");
        }
    }else if (_enterPwnRecoveryRequested){
        info("[WARNING] skipping ramdisk hash check, since device is in pwnDFU according to user\n");
        
    }else{
        info("[WARNING] full buildidentity check is not implemented, only comparing ramdisk hash.\n");
        size_t tickethashSize = 0;
        const char *tickethash = getRamdiskHashFromSCAB(im4m, &tickethashSize);
        uint64_t manifestDigestSize = 0;
        char *manifestDigest = NULL;
        
        plist_t restoreRamdisk = plist_dict_get_item(manifest, "RestoreRamDisk");
        plist_t digest = plist_dict_get_item(restoreRamdisk, "Digest");
        
        plist_get_data_val(digest, &manifestDigest, &manifestDigestSize);
        
        
        if (tickethashSize == manifestDigestSize && memcmp(tickethash, manifestDigest, tickethashSize) == 0){
            printf("Verified APTicket to be valid for this restore\n");
            free(manifestDigest);
        }else{
            free(manifestDigest);
            printf("APTicket ramdisk hash does not match the ramdisk we are trying to boot. Are you using correct install type (Update/Erase)?\n");
            reterror(-44, "APTicket can't be used for this restore\n");
        }
    }
    
    
    if (_basebandbuildmanifest){
        if (!(client->basebandBuildIdentity = getBuildidentityWithBoardconfig(_basebandbuildmanifest, client->device->hardware_model, _isUpdateInstall))){
            if (!(client->basebandBuildIdentity = getBuildidentityWithBoardconfig(_basebandbuildmanifest, client->device->hardware_model, !_isUpdateInstall)))
                reterror(-5,"ERROR: Unable to find any build identities for Baseband\n");
            else
                info("[WARNING] Unable to find Baseband buildidentities for restore type %s, using fallback %s\n", (_isUpdateInstall) ? "Update" : "Erase",(!_isUpdateInstall) ? "Update" : "Erase");
        }
            
        client->bbfwtmp = (char*)_basebandPath;
        
        plist_t bb_manifest = plist_dict_get_item(client->basebandBuildIdentity, "Manifest");
        plist_t bb_baseband = plist_copy(plist_dict_get_item(bb_manifest, "BasebandFirmware"));
        plist_dict_set_item(manifest, "BasebandFirmware", bb_baseband);
        
        if (!_client->basebandBuildIdentity)
            reterror(-55, "BasebandBuildIdentity not loaded, refusing to continue");
    }else{
        warning("WARNING: we don't have a basebandbuildmanifest, not flashing baseband!\n");
    }
    
    if (_client->image4supported) {
        plist_t sep_manifest = plist_dict_get_item(sep_build_identity, "Manifest");
        plist_t sep_sep = plist_copy(plist_dict_get_item(sep_manifest, "SEP"));
        plist_dict_set_item(manifest, "SEP", sep_sep);
        //check SEP
        unsigned char genHash[48]; //SHA384 digest length
        ptr_smart<unsigned char *>sephash = NULL;
        uint64_t sephashlen = 0;
        plist_t digest = plist_dict_get_item(sep_sep, "Digest");
        if (!digest || plist_get_node_type(digest) != PLIST_DATA)
            reterror(-66, "ERROR: can't find sep digest\n");
        
        plist_get_data_val(digest, reinterpret_cast<char **>(&sephash), &sephashlen);
        
        if (sephashlen == 20)
            SHA1((unsigned char*)_client->sepfwdata, (unsigned int)_client->sepfwdatasize, genHash);
        else
            SHA384((unsigned char*)_client->sepfwdata, (unsigned int)_client->sepfwdatasize, genHash);
        if (memcmp(genHash, sephash, sephashlen))
            reterror(-67, "ERROR: SEP does not match sepmanifest\n");
    }
    
    /* print information about current build identity */
    build_identity_print_information(build_identity);
    
    //check for enterpwnrecovery, because we could be in DFU mode
    if (_enterPwnRecoveryRequested){
        if (getDeviceMode(true) != MODE_DFU)
            reterror(-6, "unexpected device mode\n");
        enterPwnRecovery(build_identity);
    }
    
    
    // Get filesystem name from build identity
    char* fsname = NULL;
    if (build_identity_get_component_path(build_identity, "OS", &fsname) < 0) {
        error("ERROR: Unable get path for filesystem component\n");
        return -1;
    }
    
    // check if we already have an extracted filesystem
    struct stat st;
    memset(&st, '\0', sizeof(struct stat));
    char tmpf[1024];
    if (client->cache_dir) {
        if (stat(client->cache_dir, &st) < 0) {
            mkdir_with_parents(client->cache_dir, 0755);
        }
        strcpy(tmpf, client->cache_dir);
        strcat(tmpf, "/");
        char *ipswtmp = strdup(client->ipsw);
        strcat(tmpf, basename(ipswtmp));
        free(ipswtmp);
    } else {
        strcpy(tmpf, client->ipsw);
    }
    char* p = strrchr(tmpf, '.');
    if (p) {
        *p = '\0';
    }
    
    if (stat(tmpf, &st) < 0) {
        __mkdir(tmpf, 0755);
    }
    strcat(tmpf, "/");
    strcat(tmpf, fsname);
    
    memset(&st, '\0', sizeof(struct stat));
    if (stat(tmpf, &st) == 0) {
        off_t fssize = 0;
        ipsw_get_file_size(client->ipsw, fsname, &fssize);
        if ((fssize > 0) && (st.st_size == fssize)) {
            info("Using cached filesystem from '%s'\n", tmpf);
            filesystem = strdup(tmpf);
        }
    }
    
    if (!filesystem) {
        char extfn[1024];
        strcpy(extfn, tmpf);
        strcat(extfn, ".extract");
        char lockfn[1024];
        strcpy(lockfn, tmpf);
        strcat(lockfn, ".lock");
        lock_info_t li;
        
        lock_file(lockfn, &li);
        FILE* extf = NULL;
        if (access(extfn, F_OK) != 0) {
            extf = fopen(extfn, "w");
        }
        unlock_file(&li);
        if (!extf) {
            // use temp filename
            filesystem = tempnam(NULL, "ipsw_");
            if (!filesystem) {
                error("WARNING: Could not get temporary filename, using '%s' in current directory\n", fsname);
                filesystem = strdup(fsname);
            }
            delete_fs = 1;
        } else {
            // use <fsname>.extract as filename
            filesystem = strdup(extfn);
            fclose(extf);
        }
        remove(lockfn);
        
        // Extract filesystem from IPSW
        info("Extracting filesystem from IPSW\n");
        if (ipsw_extract_to_file_with_progress(client->ipsw, fsname, filesystem, 1) < 0) {
            reterror(-7,"ERROR: Unable to extract filesystem from IPSW\n");
        }
        
        if (strstr(filesystem, ".extract")) {
            // rename <fsname>.extract to <fsname>
            remove(tmpf);
            rename(filesystem, tmpf);
            free(filesystem);
            filesystem = strdup(tmpf);
        }
    }
    
    if (_rerestoreiOS9) {
        if (dfu_send_component(client, build_identity, "iBSS") < 0) {
            error("ERROR: Unable to send iBSS to device\n");
            irecv_close(client->dfu->client);
            client->dfu->client = NULL;
            return -1;
        }
        
        /* reconnect */
        dfu_client_free(client);
        sleep(2);
        dfu_client_new(client);
        
        /* send iBEC */
        if (dfu_send_component(client, build_identity, "iBEC") < 0) {
            error("ERROR: Unable to send iBEC to device\n");
            irecv_close(client->dfu->client);
            client->dfu->client = NULL;
            return -1;
        }
    }else{
        if ((client->build_major > 8)) {
            if (!client->image4supported) {
                /* send ApTicket */
                if (recovery_send_ticket(client) < 0) {
                    error("WARNING: Unable to send APTicket\n");
                }
            }
        }
    }
    
    
    
    if (_enterPwnRecoveryRequested){ //if pwnrecovery send all components decrypted, unless we're dealing with iOS 10
        if (strncmp(client->version, "10.", 3))
            client->recovery_custom_component_function = get_custom_component;
    }else if (!_rerestoreiOS9){
        /* now we load the iBEC */
        if (recovery_send_ibec(client, build_identity) < 0) {
            reterror(-8,"ERROR: Unable to send iBEC\n");
        }
        printf("waiting for device to reconnect... ");
        recovery_client_free(client);
        /* this must be long enough to allow the device to run the iBEC */
        /* FIXME: Probably better to detect if the device is back then */
        sleep(7);
    }
        
   
    for (int i=0;getDeviceMode(true) != MODE_RECOVERY && i<40; i++) putchar('.'),usleep(USEC_PER_SEC*0.5);
    putchar('\n');
    
    if (!check_mode(client))
        reterror(-15, "failed to reconnect to device in recovery (iBEC) mode\n");
    
    //do magic
    if (_client->image4supported) get_sep_nonce(client, &client->sepnonce, &client->sepnonce_size);
    get_ap_nonce(client, &client->nonce, &client->nonce_size);
    
    get_ecid(client, &client->ecid);
    
    if (client->mode->index == MODE_RECOVERY) {
        if (client->srnm == NULL) {
            reterror(-9,"ERROR: could not retrieve device serial number. Can't continue.\n");
        }
        if (irecv_send_command(client->recovery->client, "bgcolor 0 255 0") != IRECV_E_SUCCESS) {
            error("ERROR: Unable to set bgcolor\n");
            return -1;
        }
        info("[WARNING] Setting bgcolor to green! If you don't see a green screen, then your device didn't boot iBEC correctly\n");
        sleep(2); //show the user a green screen!
        if (recovery_enter_restore(client, build_identity) < 0) {
            reterror(-10,"ERROR: Unable to place device into restore mode\n");
        }
        
        recovery_client_free(client);
    }
    
    
    if (_client->image4supported && get_tss_response(client, sep_build_identity, &client->septss) < 0) {
        reterror(-11,"ERROR: Unable to get SHSH blobs for SEP\n");
    }
    
    
    
    if (_client->image4supported && (!_client->sepfwdatasize || !_client->sepfwdata))
        reterror(-55, "SEP not loaded, refusing to continue");
    
    
    
    if (client->mode->index == MODE_RESTORE) {
        info("About to restore device... \n");
        result = restore_device(client, build_identity, filesystem);
        if (result < 0) {
            reterror(-11,"ERROR: Unable to restore device\n");
        }
    }
    
    info("Cleaning up...\n");
    
    
error:
    safeFree(client->sepfwdata);
    safePlistFree(buildmanifest);
    if (delete_fs && filesystem) unlink(filesystem);
    if (!result && !err) info("DONE\n");
    return result ? abs(result) : err;
}

int futurerestore::doJustBoot(const char *ipsw, string bootargs){
    int err = 0;
    //some memory might not get freed if this function throws an exception, but you probably don't want to catch that anyway.
    
    struct idevicerestore_client_t* client = _client;
    int unused;
    int result = 0;
    plist_t buildmanifest = NULL;
    plist_t build_identity = NULL;
    
    client->ipsw = strdup(ipsw);
    
    getDeviceMode(true);
    info("Found device in %s mode\n", client->mode->string);
    
    if (!(client->mode->index == MODE_DFU || client->mode->index == MODE_RECOVERY) || !_enterPwnRecoveryRequested)
        reterror(-6, "device not in DFU/Recovery mode\n");
    // discover the device type
    if (check_hardware_model(client) == NULL || client->device == NULL) {
        reterror(-2,"ERROR: Unable to discover device model\n");
    }
    info("Identified device as %s, %s\n", client->device->hardware_model, client->device->product_type);
    
    // verify if ipsw file exists
    if (access(client->ipsw, F_OK) < 0) {
        error("ERROR: Firmware file %s does not exist.\n", client->ipsw);
        return -1;
    }
    info("Extracting BuildManifest from IPSW\n");
    if (ipsw_extract_build_manifest(client->ipsw, &buildmanifest, &unused) < 0) {
        reterror(-3,"ERROR: Unable to extract BuildManifest from %s. Firmware file might be corrupt.\n", client->ipsw);
    }
    /* check if device type is supported by the given build manifest */
    if (build_manifest_check_compatibility(buildmanifest, client->device->product_type) < 0) {
        reterror(-4,"ERROR: Could not make sure this firmware is suitable for the current device. Refusing to continue.\n");
    }
    /* print iOS information from the manifest */
    build_manifest_get_version_information(buildmanifest, client);
    
    info("Product Version: %s\n", client->version);
    info("Product Build: %s Major: %d\n", client->build, client->build_major);
    
    client->image4supported = is_image4_supported(client);
    info("Device supports Image4: %s\n", (client->image4supported) ? "true" : "false");
    
    if (!(build_identity = getBuildidentityWithBoardconfig(buildmanifest, client->device->hardware_model, 0)))
        reterror(-5,"ERROR: Unable to find any build identities for IPSW\n");

    
    /* print information about current build identity */
    build_identity_print_information(build_identity);
    
    
    //check for enterpwnrecovery, because we could be in DFU mode
    if (!_enterPwnRecoveryRequested)
        reterror(-6, "enterPwnRecoveryRequested is not set, but required");
        
    if (getDeviceMode(true) != MODE_DFU && getDeviceMode(false) != MODE_RECOVERY)
        reterror(-6, "unexpected device mode\n");
    enterPwnRecovery(build_identity, bootargs);

    client->recovery_custom_component_function = get_custom_component;
    
    for (int i=0;getDeviceMode(true) != MODE_RECOVERY && i<40; i++) putchar('.'),usleep(USEC_PER_SEC*0.5);
    putchar('\n');
    
    if (!check_mode(client))
        reterror(-15, "failed to reconnect to device in recovery (iBEC) mode\n");
    
    get_ecid(client, &client->ecid);
    
    client->flags |= FLAG_BOOT;
    
    if (client->mode->index == MODE_RECOVERY) {
        if (client->srnm == NULL) {
            reterror(-9,"ERROR: could not retrieve device serial number. Can't continue.\n");
        }
        if (irecv_send_command(client->recovery->client, "bgcolor 0 255 0") != IRECV_E_SUCCESS) {
            error("ERROR: Unable to set bgcolor\n");
            return -1;
        }
        info("[WARNING] Setting bgcolor to green! If you don't see a green screen, then your device didn't boot iBEC correctly\n");
        sleep(2); //show the user a green screen!
        client->image4supported = true; //dirty hack to not require apticket
        if (recovery_enter_restore(client, build_identity) < 0) {
            reterror(-10,"ERROR: Unable to place device into restore mode\n");
        }
        client->image4supported = false;
        recovery_client_free(client);
    }
    
    info("Cleaning up...\n");
    
error:
    safeFree(client->sepfwdata);
    safePlistFree(buildmanifest);
    if (!result && !err) info("DONE\n");
    return result ? abs(result) : err;
}

futurerestore::~futurerestore(){
    normal_client_free(_client);
    recovery_client_free(_client);
    idevicerestore_client_free(_client);
    for (auto im4m : _im4ms){
        safeFree(im4m);
    }
    safeFree(_ibootBuild);
    safeFree(_firmwareJson);
    safeFree(_firmwareTokens);
    safeFree(__latestManifest);
    safeFree(__latestFirmwareUrl);
    for (auto plist : _aptickets){
         safePlistFree(plist);
    }
    safePlistFree(_sepbuildmanifest);
    safePlistFree(_basebandbuildmanifest);
}

void futurerestore::loadFirmwareTokens(){
    if (!_firmwareTokens){
        if (!_firmwareJson) _firmwareJson = getFirmwareJson();
        if (!_firmwareJson) reterror(-6,"[TSSC] could not get firmware.json\n");
        int cnt = parseTokens(_firmwareJson, &_firmwareTokens);
        if (cnt < 1) reterror(-2,"[TSSC] parsing %s.json failed\n",(0) ? "ota" : "firmware");
    }
}

const char *futurerestore::getDeviceModelNoCopy(){
    if (!_client->device || !_client->device->product_type){
        
        int mode = getDeviceMode(true);
        if (mode != MODE_NORMAL && mode != MODE_RECOVERY && mode != MODE_DFU)
            reterror(-20, "unexpected device mode=%d\n",mode);
        
        if (check_hardware_model(_client) == NULL || _client->device == NULL)
            reterror(-2,"ERROR: Unable to discover device model\n");
    }
    
    return _client->device->product_type;
}

const char *futurerestore::getDeviceBoardNoCopy(){
    if (!_client->device || !_client->device->hardware_model){
        
        int mode = getDeviceMode(true);
        if (mode != MODE_NORMAL && mode != MODE_RECOVERY)
            reterror(-20, "unexpected device mode=%d\n",mode);
        
        if (check_hardware_model(_client) == NULL || _client->device == NULL)
            reterror(-2,"ERROR: Unable to discover device model\n");
    }
    
    return _client->device->hardware_model;
}


char *futurerestore::getLatestManifest(){
    if (!__latestManifest){
        loadFirmwareTokens();

        const char *device = getDeviceModelNoCopy();
        t_iosVersion versVals;
        memset(&versVals, 0, sizeof(versVals));
        
        int versionCnt = 0;
        int i = 0;
        char **versions = getListOfiOSForDevice(_firmwareTokens, device, 0, &versionCnt);
        if (!versionCnt) reterror(-8, "[TSSC] failed finding latest iOS\n");
        char *bpos = NULL;
        while((bpos = strstr((char*)(versVals.version = strdup(versions[i++])),"[B]")) != 0){
            free((char*)versVals.version);
            if (--versionCnt == 0) reterror(-9, "[TSSC] automatic iOS selection couldn't find non-beta iOS\n");
        }
        info("[TSSC] selecting latest iOS: %s\n",versVals.version);
        if (bpos) *bpos= '\0';
        if (versions) free(versions[versionCnt-1]),free(versions);
        
        //make sure it get's freed after function finishes execution by either reaching end or throwing exception
        ptr_smart<const char*>autofree(versVals.version);
        
        __latestFirmwareUrl = getFirmwareUrl(device, &versVals, _firmwareTokens);
        if (!__latestFirmwareUrl) reterror(-21, "could not find url of latest firmware\n");
        
        __latestManifest = getBuildManifest(__latestFirmwareUrl, device, versVals.version, versVals.buildID, 0);
        if (!__latestManifest) reterror(-22, "could not get buildmanifest of latest firmware\n");
    }
    
    return __latestManifest;
}

char *futurerestore::getLatestFirmwareUrl(){
    return getLatestManifest(),__latestFirmwareUrl;
}


void futurerestore::loadLatestBaseband(){
    char * manifeststr = getLatestManifest();
    char *pathStr = getPathOfElementInManifest("BasebandFirmware", manifeststr, getDeviceModelNoCopy(), 0);
    info("downloading Baseband\n\n");
    if (downloadPartialzip(getLatestFirmwareUrl(), pathStr, _basebandPath = BASEBAND_TMP_PATH))
        reterror(-32, "could not download baseband\n");
    saveStringToFile(manifeststr, BASEBAND_MANIFEST_TMP_PATH);
    setBasebandManifestPath(BASEBAND_MANIFEST_TMP_PATH);
    setBasebandPath(BASEBAND_TMP_PATH);
}

void futurerestore::loadLatestSep(){
    char * manifeststr = getLatestManifest();
    char *pathStr = getPathOfElementInManifest("SEP", manifeststr, getDeviceModelNoCopy(), 0);
    info("downloading SEP\n\n");
    if (downloadPartialzip(getLatestFirmwareUrl(), pathStr, SEP_TMP_PATH))
        reterror(-33, "could not download SEP\n");
    loadSep(SEP_TMP_PATH);
    saveStringToFile(manifeststr, SEP_MANIFEST_TMP_PATH);
    setSepManifestPath(SEP_MANIFEST_TMP_PATH);
}

void futurerestore::setSepManifestPath(const char *sepManifestPath){
    if (!(_sepbuildmanifest = loadPlistFromFile(_sepbuildmanifestPath = sepManifestPath)))
        reterror(-14, "failed to load SEPManifest");
}

void futurerestore::setBasebandManifestPath(const char *basebandManifestPath){
    if (!(_basebandbuildmanifest = loadPlistFromFile(_basebandbuildmanifestPath = basebandManifestPath)))
        reterror(-14, "failed to load BasebandManifest");
};

void futurerestore::loadSep(const char *sepPath){
    FILE *fsep = fopen(sepPath, "rb");
    if (!fsep)
        reterror(-15, "failed to read SEP\n");
    
    fseek(fsep, 0, SEEK_END);
    _client->sepfwdatasize = ftell(fsep);
    fseek(fsep, 0, SEEK_SET);
    
    if (!(_client->sepfwdata = (char*)malloc(_client->sepfwdatasize)))
        reterror(-15, "failed to malloc memory for SEP\n");
    
    size_t freadRet=0;
    if ((freadRet = fread(_client->sepfwdata, 1, _client->sepfwdatasize, fsep)) != _client->sepfwdatasize)
        reterror(-15, "failed to load SEP. size=%zu but fread returned %zu\n",_client->sepfwdatasize,freadRet);
    
    fclose(fsep);
}


void futurerestore::setBasebandPath(const char *basebandPath){
    FILE *fbb = fopen(basebandPath, "rb");
    if (!fbb)
        reterror(-15, "failed to read Baseband");
    _basebandPath = basebandPath;
    fclose(fbb);
}


#pragma mark static methods

inline void futurerestore::saveStringToFile(const char *str, const char *path){
    FILE *f = fopen(path, "w");
    if (!f) reterror(-41,"can't save file at %s\n",path);
    else{
        size_t len = strlen(str);
        size_t wlen = fwrite(str, 1, len, f);
        fclose(f);
        if (len != wlen) reterror(-42, "saving file failed, wrote=%zu actual=%zu\n",wlen,len);
    }
}

char *futurerestore::getNonceFromSCAB(const char* scab, size_t *nonceSize){
    char *ret = NULL;
    char *mainSet = NULL;
    int elems = 0;
    char *nonceOctet = NULL;
    
    if (!scab) reterror(-15, "Got empty SCAB\n");
    
    if (asn1ElementsInObject(scab)< 4){
        error("unexpected number of Elements in SCAB sequence\n");
        goto error;
    }
    if (nonceSize) *nonceSize = 0;
    
    mainSet = asn1ElementAtIndex(scab, 1);
    
    elems = asn1ElementsInObject(mainSet);
    
    for (int i=0; i<elems; i++) {
        nonceOctet = asn1ElementAtIndex(mainSet, i);
        if (*nonceOctet == (char)0x92)
            goto parsebnch;
    }
    return NULL;
    
    
parsebnch:
    nonceOctet++;
    
    ret = (char*)malloc(asn1Len(nonceOctet).dataLen);
    if (ret){
        memcpy(ret, nonceOctet + asn1Len(nonceOctet).sizeBytes, asn1Len(nonceOctet).dataLen);
        if (nonceSize) *nonceSize = asn1Len(nonceOctet).dataLen;
    }
    
    
error:
    return ret;
    
}

uint64_t futurerestore::getEcidFromSCAB(const char* scab){
    uint64_t ret = 0;
    char *mainSet = NULL;
    int elems = 0;
    char *ecidInt = NULL;
    t_asn1ElemLen len;
    
    if (!scab) reterror(-15, "Got empty SCAB\n");
    
    if (asn1ElementsInObject(scab)< 4){
        error("unexpected number of Elements in SCAB sequence\n");
        goto error;
    }
    mainSet = asn1ElementAtIndex(scab, 1);
    
    elems = asn1ElementsInObject(mainSet);
    
    for (int i=0; i<elems; i++) {
        ecidInt = asn1ElementAtIndex(mainSet, i);
        if (*ecidInt == (char)0x81)
            goto parseEcid;
    }
    reterror(-76, "Error: can't read ecid from SCAB\n");
    
    
parseEcid:
    ecidInt++;
    
    
    len = asn1Len(ecidInt);
    ecidInt += len.sizeBytes + len.dataLen;
    while (len.dataLen--) {
        ret *=0x100;
        ret += *(unsigned char*)--ecidInt;
    }
    
error:
    return ret;
}

const char *futurerestore::getRamdiskHashFromSCAB(const char* scab, size_t *hashSize){
    char *ret = NULL;
    char *mainSet = NULL;
    int elems = 0;
    char *nonceOctet = NULL;
    
    if (!scab) reterror(-15, "Got empty SCAB\n");
    
    if (asn1ElementsInObject(scab)< 4){
        error("unexpected number of Elements in SCAB sequence\n");
        goto error;
    }
    if (hashSize) *hashSize = 0;
    
    mainSet = asn1ElementAtIndex(scab, 1);
    
    elems = asn1ElementsInObject(mainSet);
    
    for (int i=0; i<elems; i++) {
        nonceOctet = asn1ElementAtIndex(mainSet, i);
        if (*nonceOctet == (char)0x9A)
            goto parsebnch;
    }
    return NULL;
    
    
parsebnch:
    nonceOctet++;
    
    ret = nonceOctet + asn1Len(nonceOctet).sizeBytes;
    if (hashSize)
        *hashSize = asn1Len(nonceOctet).dataLen;
    
    
error:
    return ret;
}

uint64_t futurerestore::getEcidFromIM4M(const char* im4m){
    uint64_t ret = 0;
    char *mainSet = NULL;
    char *manbSet = NULL;
    char *manpSet = NULL;
    char *ecidInt = NULL;
    char *ecid = NULL;
    char *manb = NULL;
    char *manp = NULL;
    t_asn1ElemLen len;
    
    if (!im4m) reterror(-15, "Got empty IM4M\n");
    
    if (asn1ElementsInObject(im4m)< 3){
        error("unexpected number of Elements in IM4M sequence\n");
        goto error;
    }
    mainSet = asn1ElementAtIndex(im4m, 2);
    
    manb = getValueForTagInSet((char*)mainSet, 0x4d414e42); //MANB priv Tag
    if (asn1ElementsInObject(manb)< 2){
        error("unexpected number of Elements in MANB sequence\n");
        goto error;
    }
    manbSet = asn1ElementAtIndex(manb, 1);
    
    manp = getValueForTagInSet((char*)manbSet, 0x4d414e50); //MANP priv Tag
    if (asn1ElementsInObject(manp)< 2){
        error("unexpected number of Elements in MANP sequence\n");
        goto error;
    }
    manpSet = asn1ElementAtIndex(manp, 1);
    
    ecid = getValueForTagInSet((char*)manpSet, *(uint32_t*)"DICE"); //ECID priv Tag
    if (asn1ElementsInObject(ecid)< 2){
        error("unexpected number of Elements in BNCH sequence\n");
        goto error;
    }
    ecidInt = (char*)asn1ElementAtIndex(ecid, 1);
    ecidInt++;
    
   
    len = asn1Len(ecidInt);
    ecidInt += len.sizeBytes;
    while (len.dataLen--) {
        ret *=0x100;
        ret += *(unsigned char*)ecidInt++;
    }
    
error:
    return ret;
}


char *futurerestore::getNonceFromAPTicket(const char* apticketPath){
    char *ret = NULL;
    if (char *im4m = im4mFormShshFile(apticketPath,NULL)){
        ret = getBNCHFromIM4M(im4m,NULL);
        free(im4m);
    }
    return ret;
}

plist_t futurerestore::loadPlistFromFile(const char *path){
    plist_t ret = NULL;
    
    FILE *f = fopen(path,"rb");
    if (!f){
        error("could not open file %s\n",path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    size_t bufSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = (char*)malloc(bufSize);
    if (!buf){
        error("failed to alloc memory\n");
        return NULL;
    }
    
    size_t freadRet = 0;
    if ((freadRet = fread(buf, 1, bufSize, f)) != bufSize){
        error("fread=%zu but bufsize=%zu",freadRet,bufSize);
        return NULL;
    }
    fclose(f);
    
    if (memcmp(buf, "bplist00", 8) == 0)
        plist_from_bin(buf, (uint32_t)bufSize, &ret);
    else
        plist_from_xml(buf, (uint32_t)bufSize, &ret);
    free(buf);
    
    return ret;
}

char *futurerestore::getPathOfElementInManifest(const char *element, const char *manifeststr, const char *model, int isUpdateInstall){
    char *pathStr = NULL;
    ptr_smart<plist_t> buildmanifest(NULL,plist_free);
    
    plist_from_xml(manifeststr, (uint32_t)strlen(manifeststr), &buildmanifest);
    
    if (plist_t identity = getBuildidentity(buildmanifest._p, model, isUpdateInstall))
        if (plist_t manifest = plist_dict_get_item(identity, "Manifest"))
            if (plist_t elem = plist_dict_get_item(manifest, element))
                if (plist_t info = plist_dict_get_item(elem, "Info"))
                    if (plist_t path = plist_dict_get_item(info, "Path"))
                        if (plist_get_string_val(path, &pathStr), pathStr)
                            goto noerror;
    
    reterror(-31, "could not get %s path\n",element);
noerror:
    return pathStr;
}

