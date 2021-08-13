//
//  futurerestore.cpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <libgeneral/macros.h>
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

#include <img4tool/img4tool.hpp>

extern "C"{
#include "common.h"
#include "normal.h"
#include "recovery.h"
#include "dfu.h"
#include "ipsw.h"
#include "locking.h"
#include "restore.h"
#include "tsschecker.h"
#include <libirecovery.h>
#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define __bswap_64(x) OSSwapInt64(x)
#else
#include <byteswap.h>
#endif
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

#ifdef WIN32
#define FUTURERESTORE_TMP_PATH "download"
#else
#define TMP_PATH "/tmp"
#define FUTURERESTORE_TMP_PATH TMP_PATH"/futurerestore"
#endif

#define ROSE_TMP_PATH FUTURERESTORE_TMP_PATH"/rose.bin"
#define SE_TMP_PATH FUTURERESTORE_TMP_PATH"/se.sefw"

#define SAVAGE_B0_PP_TMP_PATH FUTURERESTORE_TMP_PATH"/savageB0PP.fw"
#define SAVAGE_B0_DP_TMP_PATH FUTURERESTORE_TMP_PATH"/savageB0DP.fw"

#define SAVAGE_B2_PP_TMP_PATH FUTURERESTORE_TMP_PATH"/savageB2PP.fw"
#define SAVAGE_B2_DP_TMP_PATH FUTURERESTORE_TMP_PATH"/savageB2DP.fw"

#define SAVAGE_BA_PP_TMP_PATH FUTURERESTORE_TMP_PATH"/savageBAPP.fw"
#define SAVAGE_BA_DP_TMP_PATH FUTURERESTORE_TMP_PATH"/savageBADP.fw"

#define VERIDIAN_DGM_TMP_PATH FUTURERESTORE_TMP_PATH"/veridianDGM.der"
#define VERIDIAN_FWM_TMP_PATH FUTURERESTORE_TMP_PATH"/veridianFWM.plist"
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

using namespace tihmstar;

#pragma mark helpers
extern "C"{
    void irecv_event_cb(const irecv_device_event_t* event, void *userdata);
    void idevice_event_cb(const idevice_event_t *event, void *userdata);
    int dfu_send_iboot_stage1_components(struct idevicerestore_client_t* client, plist_t build_identity);
    int dfu_send_command(struct idevicerestore_client_t* client, const char* command);
    int dfu_send_component_and_command(struct idevicerestore_client_t* client, plist_t build_identity, const char* component, const char* command);
    irecv_error_t irecv_send_command(irecv_client_t client, const char* command);
    int recovery_send_component_and_command(struct idevicerestore_client_t* client, plist_t build_identity, const char* component, const char* command);
    int recovery_send_component(struct idevicerestore_client_t* client, plist_t build_identity, const char* component);
};

#pragma mark futurerestore
futurerestore::futurerestore(bool isUpdateInstall, bool isPwnDfu, bool noIBSS, bool noRestore) : _isUpdateInstall(isUpdateInstall), _isPwnDfu(isPwnDfu), _noIBSS(noIBSS), _noRestore(noRestore){
    _client = idevicerestore_client_new();
    if (_client == NULL) throw std::string("could not create idevicerestore client\n");
    
    struct stat st{0};
    if (stat(FUTURERESTORE_TMP_PATH, &st) == -1) __mkdir(FUTURERESTORE_TMP_PATH, 0755);
    
    nocache = 1; //tsschecker nocache
    _foundnonce = -1;
}

bool futurerestore::init(){
    if (_didInit) return _didInit;
//    If device is in an invalid state, don't check if it supports img4
    if ((_didInit = check_mode(_client) != MODE_UNKNOWN)) {
        if (!(_client->image4supported = is_image4_supported(_client))){
            info("[INFO] 32-bit device detected\n");
        }else{
            info("[INFO] 64-bit device detected\n");
        }
    }
    return _didInit;
}

uint64_t futurerestore::getDeviceEcid(){
    retassure(_didInit, "did not init\n");
    uint64_t ecid;
    get_ecid(_client, &ecid);
    return ecid;
}

int futurerestore::getDeviceMode(bool reRequest){
    retassure(_didInit, "did not init\n");
    if (!reRequest && _client->mode && _client->mode->index != MODE_UNKNOWN) {
        return _client->mode->index;
    }else{
        dfu_client_free(_client);
        recovery_client_free(_client);
        return check_mode(_client);
    }
}

void futurerestore::putDeviceIntoRecovery(){
    retassure(_didInit, "did not init\n");

#ifdef HAVE_LIBIPATCHER
    _enterPwnRecoveryRequested = _isPwnDfu;
#endif
    
    getDeviceMode(false);
    info("Found device in %s mode\n", _client->mode->string);
    if (_client->mode->index == MODE_NORMAL){
    irecv_device_event_subscribe(&_client->irecv_e_ctx, irecv_event_cb, _client);
    idevice_event_subscribe(idevice_event_cb, _client);
#ifdef HAVE_LIBIPATCHER
        retassure(!_isPwnDfu, "isPwnDfu enabled, but device was found in normal mode\n");
#endif
        info("Entering recovery mode...\n");
        retassure(!normal_enter_recovery(_client),"Unable to place device into recovery mode from %s mode\n", _client->mode->string);
    }else if (_client->mode->index == MODE_RECOVERY){
        info("Device already in recovery mode\n");
    }else if (_client->mode->index == MODE_DFU && _isPwnDfu &&
#ifdef HAVE_LIBIPATCHER
              true
#else
              false
#endif
              ){
        info("requesting to get into pwnRecovery later\n");
    }else if (!_client->image4supported){
        info("32-bit device in DFU mode found, assuming user wants to use iOS 9.x re-restore bug. Not failing here\n");
    }else{
        reterror("unsupported device mode, please put device in recovery or normal mode\n");
    }
    safeFree(_client->udid); //only needs to be freed manually when function did't throw exception
    
    //these get also freed by destructor
    dfu_client_free(_client);
    recovery_client_free(_client);
}

void futurerestore::setAutoboot(bool val){
    retassure(_didInit, "did not init\n");

    retassure(getDeviceMode(false) == MODE_RECOVERY, "can't set auto-boot, when device isn't in recovery mode\n");
    if(!_client->recovery){
        retassure(!recovery_client_new(_client),"Could not connect to device in recovery mode.\n");
    }
    retassure(!recovery_set_autoboot(_client, val),"Setting auto-boot failed?!\n");
}

void futurerestore::exitRecovery(){
    setAutoboot(true);
    recovery_send_reset(_client);
    recovery_client_free(_client);
}

plist_t futurerestore::nonceMatchesApTickets(){
    retassure(_didInit, "did not init\n");

    if (getDeviceMode(true) != MODE_RECOVERY){
        if (getDeviceMode(false) != MODE_DFU || *_client->version != '9')
            reterror("Device is not in recovery mode, can't check ApNonce\n");
        else
            _rerestoreiOS9 = (info("Detected iOS 9.x 32-bit re-restore, proceeding in DFU mode\n"),true);
    }
    
    unsigned char* realnonce;
    int realNonceSize = 0;
    if (_rerestoreiOS9) {
        info("Skipping ApNonce check\n");
    }else{
        recovery_get_ap_nonce(_client, &realnonce, &realNonceSize);
        
        info("Got ApNonce from device: ");
        int i = 0;
        for (i = 0; i < realNonceSize; i++) {
            info("%02x ", ((unsigned char *)realnonce)[i]);
        }
        info("\n");
    }
    
    vector<const char*>nonces;
    
    if (_client->image4supported){
        for (int i=0; i< _im4ms.size(); i++){
            auto nonce = img4tool::getValFromIM4M({_im4ms[i].first,_im4ms[i].second}, 'BNCH');
            if (nonce.payloadSize() == realNonceSize && memcmp(realnonce, nonce.payload(), realNonceSize) == 0) return _aptickets[i];
        }
    }else{
        for (int i=0; i< _im4ms.size(); i++){
            size_t ticketNonceSize = 0;
            const char *nonce = NULL;
            try {
                //nonce might not exist, which we use in re-restoring iOS 9.x for 32-bit
                auto n = getNonceFromSCAB(_im4ms[i].first, _im4ms[i].second);
                ticketNonceSize = n.second;
                nonce = n.first;
            } catch (...)
            { }
            if (memcmp(realnonce, nonce, ticketNonceSize) == 0 &&
                 (  (ticketNonceSize == realNonceSize && realNonceSize+ticketNonceSize > 0) ||
                        (!ticketNonceSize && *_client->version == '9' &&
                            (getDeviceMode(false) == MODE_DFU ||
                                ( getDeviceMode(false) == MODE_RECOVERY && !strncmp(getiBootBuild(), "iBoot-2817", strlen("iBoot-2817")) )
                            )
                         )
                 )
               )
                //either nonce needs to match or using re-restore bug in iOS 9.x
                return _aptickets[i];
        }
    }
    
    return NULL;
}

std::pair<const char *,size_t> futurerestore::nonceMatchesIM4Ms(){
    retassure(_didInit, "did not init\n");

    retassure(getDeviceMode(true) == MODE_RECOVERY, "Device is not in recovery mode, can't check ApNonce\n");
    
    unsigned char* realnonce;
    int realNonceSize = 0;
    recovery_get_ap_nonce(_client, &realnonce, &realNonceSize);
    
    vector<const char*>nonces;
    
    if (_client->image4supported) {
        for (int i=0; i< _im4ms.size(); i++){
            auto nonce = img4tool::getValFromIM4M({_im4ms[i].first,_im4ms[i].second}, 'BNCH');
            if (nonce.payloadSize() == realNonceSize && memcmp(realnonce, nonce.payload(), realNonceSize) == 0) return _im4ms[i];
        }
    }else{
        for (int i=0; i< _im4ms.size(); i++){
            size_t ticketNonceSize = 0;
            const char *nonce = NULL;
            try {
                //nonce might not exist, which we use in re-restoring iOS 9.x for 32-bit
                auto n = getNonceFromSCAB(_im4ms[i].first, _im4ms[i].second);
                ticketNonceSize = n.second;
                nonce = n.first;
            } catch (...) {
                //
            }
            if (memcmp(realnonce, nonce, ticketNonceSize) == 0) return _im4ms[i];
        }
    }
    
    return {NULL,0};
}

void futurerestore::waitForNonce(vector<const char *>nonces, size_t nonceSize){
    retassure(_didInit, "did not init\n");
    setAutoboot(false);
    
    unsigned char* realnonce;
    int realNonceSize = 0;
    
    for (auto nonce : nonces){
        info("waiting for ApNonce: ");
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
        retassure(!recovery_client_new(_client), "Could not connect to device in recovery mode\n");
        
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
    retassure(_im4ms.size(), "No IM4M loaded\n");
    
    size_t nonceSize = 0;
    vector<const char*>nonces;
    
    retassure(_client->image4supported, "Error: ApNonce collision function is not supported on 32-bit devices\n");
    
    for (auto im4m : _im4ms){
        auto nonce = img4tool::getValFromIM4M({im4m.first,im4m.second}, 'BNCH');
        if (!nonceSize) {
            nonceSize = nonce.payloadSize();
        }
        retassure(nonceSize == nonce.payloadSize(), "Nonces have different lengths!");
        nonces.push_back((const char*)nonce.payload());
    }
    
    waitForNonce(nonces,nonceSize);
}

void futurerestore::loadAPTickets(const vector<const char *> &apticketPaths){
    for (auto apticketPath : apticketPaths){
        plist_t apticket = NULL;
        char *im4m = NULL;
        struct stat fst;
        
        retassure(!stat(apticketPath, &fst), "failed to load APTicket at %s\n",apticketPath);
        
        gzFile zf = gzopen(apticketPath, "rb");
        if (zf) {
            int blen = 0;
            int readsize = 16384; //0x4000
            int bufsize = readsize;
            char* bin = (char*)malloc(bufsize);
            char* p = bin;
            do {
                int bytes_read = gzread(zf, p, readsize);
                retassure(bytes_read>0, "Error reading gz compressed data\n");
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
        
        retassure(im4msize, "Error: failed to load signing ticket file %s\n",apticketPath);
        
        _im4ms.push_back({im4m,im4msize});
        _aptickets.push_back(apticket);
        printf("reading signing ticket %s is done\n",apticketPath);
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
            retassure(!recovery_client_new(_client), "Error: can't create new recovery client");
        }
        irecv_getenv(_client->recovery->client, "build-version", &_ibootBuild);
        retassure(_ibootBuild, "Error: can't get a build-version");
    }
    return _ibootBuild;
}

pair<ptr_smart<char*>, size_t> getIPSWComponent(struct idevicerestore_client_t* client, plist_t build_identity, string component){
    ptr_smart<char *> path;
    unsigned char* component_data = NULL;
    unsigned int component_size = 0;

    if (!(char*)path) {
        retassure(!build_identity_get_component_path(build_identity, component.c_str(), &path),"ERROR: Unable to get path for component '%s'\n", component.c_str());
    }
    
    retassure(!extract_component(client->ipsw, (char*)path, &component_data, &component_size),"ERROR: Unable to extract component: %s\n", component.c_str());
    
    return {(char*)component_data,component_size};
}

void futurerestore::enterPwnRecovery(plist_t build_identity, string bootargs){
#ifndef HAVE_LIBIPATCHER
    reterror("compiled without libipatcher");
#else
   bootargs = "rd=md0 -restore -v serial=3 debug=0x14e keepsyms=1 amfi=0xff amfi_unrestrict_task_for_pid=1 amfi_allow_any_signature=1 amfi_get_out_of_my_way=1";
    int mode = 0;
    libipatcher::fw_key iBSSKeys;
    libipatcher::fw_key iBECKeys;

    getDeviceMode(false);
    mutex_lock(&_client->device_event_mutex);
    cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 1000);
    retassure(((_client->mode->index == MODE_DFU) || (mutex_unlock(&_client->device_event_mutex),0)), "Device isn't in DFU mode!");
    retassure(((dfu_client_new(_client) == IRECV_E_SUCCESS) || (mutex_unlock(&_client->device_event_mutex),0)), "Failed to connect to device in DFU Mode!");
    mutex_unlock(&_client->device_event_mutex);

    try {
        const char *board = getDeviceBoardNoCopy();
        info("BOARD: %s\n", board);
        if(board == "n71ap" || board == "n71map" || board == "n69ap" || board == "n69uap") {
            iBSSKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBSS", board);
            iBECKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBEC", board);
        } else {
            iBSSKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBSS");
            iBECKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBEC");
        }
    } catch (tihmstar::exception &e) {
        reterror("getting keys failed with error: %d (%s). Are keys publicly available?",e.code(),e.what());
    }

    auto iBSS = getIPSWComponent(_client, build_identity, "iBSS");
    iBSS = move(libipatcher::patchiBSS((char*)iBSS.first, iBSS.second, iBSSKeys));

    auto iBEC = getIPSWComponent(_client, build_identity, "iBEC");
    iBEC = move(libipatcher::patchiBEC((char*)iBEC.first, iBEC.second, iBECKeys, bootargs));

    if (_client->image4supported) {
        /* if this is 64-bit, we need to back IM4P to IMG4
           also due to the nature of iBoot64Patchers sigpatches we need to stich a valid signed im4m to it (but nonce is ignored) */
        iBSS = move(libipatcher::packIM4PToIMG4(iBSS.first, iBSS.second, _im4ms[0].first, _im4ms[0].second));
        iBEC = move(libipatcher::packIM4PToIMG4(iBEC.first, iBEC.second, _im4ms[0].first, _im4ms[0].second));
    }

    /* send iBSS */
    info("Sending %s (%lu bytes)...\n", "iBSS", iBSS.second);
    mutex_lock(&_client->device_event_mutex);
    irecv_error_t err = irecv_send_buffer(_client->dfu->client, (unsigned char*)(char*)iBSS.first, (unsigned long)iBSS.second, 1);
    retassure(err == IRECV_E_SUCCESS,"ERROR: Unable to send %s component: %s\n", "iBSS", irecv_strerror(err));

    info("Booting iBSS, waiting for device to disconnect...\n");
    cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
    retassure(((_client->mode == &idevicerestore_modes[MODE_UNKNOWN]) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not disconnect. Possibly invalid iBSS. Reset device and try again");
    mutex_unlock(&_client->device_event_mutex);
    info("Booting iBSS, waiting for device to reconnect...\n");
    mutex_lock(&_client->device_event_mutex);
    cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
    retassure(((_client->mode->index == MODE_RECOVERY) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not reconnect. Possibly invalid iBSS. Reset device and try again");
    mutex_unlock(&_client->device_event_mutex);
    retassure(((dfu_client_new(_client) == IRECV_E_SUCCESS) || (mutex_unlock(&_client->device_event_mutex),0)), "Failed to connect to device in DFU Mode!");
    mutex_lock(&_client->device_event_mutex);

    if (_client->build_major > 8) {
        retassure(irecv_usb_set_configuration(_client->dfu->client, 1) >= 0, "ERROR: set configuration failed\n");
        if (_client->build_major >= 20) {
            // Without this empty policy file & its special signature, iBEC won't start.
            if (dfu_send_component_and_command(_client, build_identity, "Ap,LocalPolicy", "lpolrestore") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send Ap,LocalPolicy to device\n");
                irecv_close(_client->dfu->client);
                _client->dfu->client = NULL;
                return;
            }

            if (dfu_send_iboot_stage1_components(_client, build_identity) < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send iBoot stage 1 components to device\n");
                irecv_close(_client->dfu->client);
                _client->dfu->client = NULL;
                return;
            }

            if (dfu_send_command(_client, "setenv auto-boot false") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->dfu->client);
                _client->dfu->client = NULL;
                return;
            }

            if (dfu_send_command(_client, "saveenv") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->dfu->client);
                _client->dfu->client = NULL;
                return;
            }

            if (dfu_send_command(_client, "setenvnp boot-args rd=md0 nand-enable-reformat=1 -progress -restore -v") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->dfu->client);
                _client->dfu->client = NULL;
                return;
            }

            if (dfu_send_component(_client, build_identity, "RestoreLogo") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send RestoreDCP to device\n");
                irecv_close(_client->dfu->client);
                _client->dfu->client = NULL;
                return;
            }

            if (dfu_send_command(_client, "setpicture 4") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->dfu->client);
                _client->dfu->client = NULL;
                return;
            }

            if (dfu_send_command(_client, "bgcolor 0 0 0") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->dfu->client);
                _client->dfu->client = NULL;
                return;
            }
        }
        /* send iBEC */
        info("Sending %s (%lu bytes)...\n", "iBEC", iBEC.second);
        err = irecv_send_buffer(_client->dfu->client, (unsigned char*)(char*)iBEC.first, (unsigned long)iBEC.second, 1);
        retassure(err == IRECV_E_SUCCESS,"ERROR: Unable to send %s component: %s\n", "iBEC", irecv_strerror(err));
        info("Booting iBEC, waiting for device to disconnect...\n");
        if(_client->mode->index == MODE_RECOVERY)
            retassure(((irecv_send_command(_client->recovery->client, "go") == IRECV_E_SUCCESS) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not disconnect/reconnect. Possibly invalid iBEC. Reset device and try again\n");
        getDeviceMode(true);
        cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
        retassure(((_client->mode == &idevicerestore_modes[MODE_UNKNOWN]) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not disconnect. Possibly invalid iBEC. Reset device and try again");
        mutex_unlock(&_client->device_event_mutex);
        info("Booting iBEC, waiting for device to reconnect...\n");
    }
    mutex_lock(&_client->device_event_mutex);
    cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
    retassure(((_client->mode->index == MODE_RECOVERY) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not reconnect. Possibly invalid iBEC. Reset device and try again");
    mutex_unlock(&_client->device_event_mutex);
    retassure(((recovery_client_new(_client) == IRECV_E_SUCCESS) || (mutex_unlock(&_client->device_event_mutex),0)), "Failed to connect to device in Recovery Mode! Reset device and try again.");
    retassure((_client->mode->index == MODE_RECOVERY), "Failed to connect to device in Recovery Mode! Reset device and try again.");
    if (_client->build_major < 20) {
        irecv_usb_control_transfer(_client->recovery->client, 0x21, 1, 0, 0, 0, 0, 5000);
    }
    
    if (_client->image4supported) {
        char *deviceGen = NULL;
        cleanup([&]{
            safeFree(deviceGen);
        });
        /* IMG4 requires to have a generator set for the device to successfully boot after restore
           set generator now and make sure the nonce is the one we are trying to restore */
        
        assure(!irecv_send_command(_client->recovery->client, "bgcolor 255 0 0"));
        sleep(2); //yes, I like displaying colored screens to the user and making him wait for no reason :P
        
        auto nonceelem = img4tool::getValFromIM4M({_im4ms[0].first,_im4ms[0].second}, 'BNCH');

        printf("ApNonce pre-hax:\n");
        get_ap_nonce(_client, &_client->nonce, &_client->nonce_size);
        std::string generator = getGeneratorFromSHSH2(_client->tss);

        if (memcmp(_client->nonce, nonceelem.payload(), _client->nonce_size) != 0) {
            printf("ApNonce from device doesn't match IM4M nonce, applying hax...\n");
            
            assure(_client->tss);
            printf("Writing generator=%s to nvram!\n",generator.c_str());
            
            retassure(!irecv_setenv(_client->recovery->client, "com.apple.System.boot-nonce", generator.c_str()),"failed to write generator to nvram");
            retassure(!irecv_saveenv(_client->recovery->client), "failed to save nvram");
            
            /* send iBEC */
            info("Sending %s (%lu bytes)...\n", "iBEC", iBEC.second);
            mutex_lock(&_client->device_event_mutex);
            irecv_error_t err = irecv_send_buffer(_client->recovery->client, (unsigned char*)(char*)iBEC.first, (unsigned long)iBEC.second, 1);
            retassure(err == IRECV_E_SUCCESS,"ERROR: Unable to send %s component: %s\n", "iBEC", irecv_strerror(err));
            printf("waiting for device to reconnect...\n");
            retassure(!irecv_send_command(_client->recovery->client, "go"),"failed to re-launch iBEC after ApNonce hax");
            getDeviceMode(true);

            info("Booting 2nd iBEC, Waiting for device to disconnect...\n");
            cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
            retassure((_client->mode == &idevicerestore_modes[MODE_UNKNOWN] || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not disconnect after sending hax-iBEC in pwn-iBEC mode");
            mutex_unlock(&_client->device_event_mutex);
            
            info("Booting 2nd iBEC, Waiting for device to reconnect...\n");
            mutex_lock(&_client->device_event_mutex);
            cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
            info("mode: %s\n", (_client->mode == &idevicerestore_modes[MODE_RECOVERY]) ? "RECOVERY" : (_client->mode == &idevicerestore_modes[MODE_DFU]) ? "DFU" : (_client->mode == &idevicerestore_modes[MODE_UNKNOWN]) ? "UNKNOWN" : (_client->mode == &idevicerestore_modes[MODE_WTF]) ? "WTF" : "ERR");
            retassure((_client->mode == &idevicerestore_modes[MODE_RECOVERY] || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not reconnect after sending hax-iBEC in pwn-iBEC mode");
            mutex_unlock(&_client->device_event_mutex);

            retassure(!recovery_client_new(_client), "failed to reconnect to recovery after ApNonce hax");
            
            printf("APnonce post-hax:\n");
            get_ap_nonce(_client, &_client->nonce, &_client->nonce_size);
            assure(!irecv_send_command(_client->recovery->client, "bgcolor 255 255 0"));
            retassure(memcmp(_client->nonce, nonceelem.payload(), _client->nonce_size) == 0, "ApNonce from device doesn't match IM4M nonce after applying ApNonce hax. Aborting!");
        }else{
            printf("APNonce from device already matches IM4M nonce, no need for extra hax...\n");
        }
        retassure(!irecv_setenv(_client->recovery->client, "com.apple.System.boot-nonce", generator.c_str()),"failed to write generator to nvram");
        retassure(!irecv_saveenv(_client->recovery->client), "failed to save nvram");
        
        sleep(2); //yes, I like displaying colored screens to the user and making him wait for no reason :P
    }

#endif //HAVE_LIBIPATCHER
}
void futurerestore::enterPwnRecovery2(plist_t build_identity, string bootargs){
#ifndef HAVE_LIBIPATCHER
    reterror("compiled without libipatcher");
#else
   bootargs = "rd=md0 -restore -v serial=3 debug=0x14e keepsyms=1 amfi=0xff amfi_unrestrict_task_for_pid=1 amfi_allow_any_signature=1 amfi_get_out_of_my_way=1";
    int mode = 0;
    libipatcher::fw_key iBSSKeys;
    libipatcher::fw_key iBECKeys;

    getDeviceMode(false);
    mutex_lock(&_client->device_event_mutex);
    cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 1000);
    retassure(((_client->mode->index == MODE_DFU) || (mutex_unlock(&_client->device_event_mutex),0)), "Device isn't in DFU mode!");
    retassure(((dfu_client_new(_client) == IRECV_E_SUCCESS) || (mutex_unlock(&_client->device_event_mutex),0)), "Failed to connect to device in DFU Mode!");
    mutex_unlock(&_client->device_event_mutex);

    try {
        const char *board = getDeviceBoardNoCopy();
        info("BOARD: %s\n", board);
        if(board == "n71ap" || board == "n71map" || board == "n69ap" || board == "n69uap") {
            iBSSKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBSS", board);
            iBECKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBEC", board);
        } else {
            iBSSKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBSS");
            iBECKeys = libipatcher::getFirmwareKey(_client->device->product_type, _client->build, "iBEC");
        }
    } catch (tihmstar::exception &e) {
        reterror("getting keys failed with error: %d (%s). Are keys publicly available?",e.code(),e.what());
    }

    auto iBSS = getIPSWComponent(_client, build_identity, "iBSS");
    iBSS = move(libipatcher::patchiBSS((char*)iBSS.first, iBSS.second, iBSSKeys));

    auto iBEC = getIPSWComponent(_client, build_identity, "iBEC");
    iBEC = move(libipatcher::patchiBEC((char*)iBEC.first, iBEC.second, iBECKeys, bootargs));

    if (_client->image4supported) {
        /* if this is 64-bit, we need to back IM4P to IMG4
           also due to the nature of iBoot64Patchers sigpatches we need to stich a valid signed im4m to it (but nonce is ignored) */
        iBSS = move(libipatcher::packIM4PToIMG4(iBSS.first, iBSS.second, _im4ms[0].first, _im4ms[0].second));
        iBEC = move(libipatcher::packIM4PToIMG4(iBEC.first, iBEC.second, _im4ms[0].first, _im4ms[0].second));
    }

    /* send iBSS */
    info("Sending %s (%lu bytes)...\n", "iBSS", iBSS.second);
    mutex_lock(&_client->device_event_mutex);
    irecv_error_t err = irecv_send_buffer(_client->dfu->client, (unsigned char*)(char*)iBSS.first, (unsigned long)iBSS.second, 1);
    retassure(err == IRECV_E_SUCCESS,"ERROR: Unable to send %s component: %s\n", "iBSS", irecv_strerror(err));
    getDeviceMode(true);
    info("Booting iBSS, waiting for device to disconnect...\n");
    cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
    retassure(((_client->mode == &idevicerestore_modes[MODE_UNKNOWN]) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not disconnect. Possibly invalid iBSS. Reset device and try again");
    mutex_unlock(&_client->device_event_mutex);
    info("Booting iBSS, waiting for device to reconnect...\n");
    mutex_lock(&_client->device_event_mutex);
    cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
    retassure(((_client->mode->index == MODE_RECOVERY) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not reconnect. Possibly invalid iBSS. Reset device and try again");
    mutex_unlock(&_client->device_event_mutex);
    retassure(((recovery_client_new(_client) == IRECV_E_SUCCESS) || (mutex_unlock(&_client->device_event_mutex),0)), "Failed to connect to device in Recovery Mode!");
    mutex_lock(&_client->device_event_mutex);

    if (_client->build_major > 8) {
        retassure(irecv_usb_set_configuration(_client->recovery->client, 1) >= 0, "ERROR: set configuration failed\n");
        if (_client->build_major >= 20) {
            // Without this empty policy file & its special signature, iBEC won't start.
            if (recovery_send_component_and_command(_client, build_identity, "Ap,LocalPolicy", "lpolrestore") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send Ap,LocalPolicy to device\n");
                irecv_close(_client->recovery->client);
                _client->recovery->client = NULL;
                return;
            }

            // if (dfu_send_iboot_stage1_components(_client, build_identity) < 0) {
            //     mutex_unlock(&_client->device_event_mutex);
            //     error("ERROR: Unable to send iBoot stage 1 components to device\n");
            //     irecv_close(_client->recovery->client);
            //     _client->recovery->client = NULL;
            //     return;
            // }

            if (irecv_send_command(_client->recovery->client, "setenv auto-boot false") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->recovery->client);
                _client->recovery->client = NULL;
                return;
            }

            if (irecv_send_command(_client->recovery->client, "saveenv") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->recovery->client);
                _client->recovery->client = NULL;
                return;
            }

            if (irecv_send_command(_client->recovery->client, "setenvnp boot-args rd=md0 nand-enable-reformat=1 -progress -restore -v") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->recovery->client);
                _client->recovery->client = NULL;
                return;
            }

            if (recovery_send_component(_client, build_identity, "RestoreLogo") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send RestoreDCP to device\n");
                irecv_close(_client->recovery->client);
                _client->recovery->client = NULL;
                return;
            }

            if (irecv_send_command(_client->recovery->client, "setpicture 4") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->recovery->client);
                _client->recovery->client = NULL;
                return;
            }

            if (irecv_send_command(_client->recovery->client, "bgcolor 0 0 0") < 0) {
                mutex_unlock(&_client->device_event_mutex);
                error("ERROR: Unable to send command to device\n");
                irecv_close(_client->recovery->client);
                _client->recovery->client = NULL;
                return;
            }
        }
        /* send iBEC */
        info("Sending %s (%lu bytes)...\n", "iBEC", iBEC.second);
        err = irecv_send_buffer(_client->recovery->client, (unsigned char*)(char*)iBEC.first, (unsigned long)iBEC.second, 1);
        retassure(err == IRECV_E_SUCCESS,"ERROR: Unable to send %s component: %s\n", "iBEC", irecv_strerror(err));
        info("Booting iBEC, waiting for device to disconnect...\n");
        if(_client->mode->index == MODE_RECOVERY)
            retassure(((irecv_send_command(_client->recovery->client, "go") == IRECV_E_SUCCESS) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not disconnect/reconnect. Possibly invalid iBEC. Reset device and try again\n");
        getDeviceMode(true);
        cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
        retassure(((_client->mode == &idevicerestore_modes[MODE_UNKNOWN]) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not disconnect. Possibly invalid iBEC. Reset device and try again");
        mutex_unlock(&_client->device_event_mutex);
        info("Booting iBEC, waiting for device to reconnect...\n");
    }
    mutex_lock(&_client->device_event_mutex);
    cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
    retassure(((_client->mode->index == MODE_RECOVERY) || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not reconnect. Possibly invalid iBEC. Reset device and try again");
    mutex_unlock(&_client->device_event_mutex);
    retassure(((recovery_client_new(_client) == IRECV_E_SUCCESS) || (mutex_unlock(&_client->device_event_mutex),0)), "Failed to connect to device in Recovery Mode! Reset device and try again.");
    retassure((_client->mode->index == MODE_RECOVERY), "Failed to connect to device in Recovery Mode! Reset device and try again.");
    if (_client->build_major < 20) {
        irecv_usb_control_transfer(_client->recovery->client, 0x21, 1, 0, 0, 0, 0, 5000);
    }
    
    if (_client->image4supported) {
        char *deviceGen = NULL;
        cleanup([&]{
            safeFree(deviceGen);
        });
        /* IMG4 requires to have a generator set for the device to successfully boot after restore
           set generator now and make sure the nonce is the one we are trying to restore */
        
        assure(!irecv_send_command(_client->recovery->client, "bgcolor 255 0 0"));
        sleep(2); //yes, I like displaying colored screens to the user and making him wait for no reason :P
        
        auto nonceelem = img4tool::getValFromIM4M({_im4ms[0].first,_im4ms[0].second}, 'BNCH');

        printf("ApNonce pre-hax:\n");
        get_ap_nonce(_client, &_client->nonce, &_client->nonce_size);
        std::string generator = getGeneratorFromSHSH2(_client->tss);

        if (memcmp(_client->nonce, nonceelem.payload(), _client->nonce_size) != 0) {
            printf("ApNonce from device doesn't match IM4M nonce, applying hax...\n");
            
            assure(_client->tss);
            printf("Writing generator=%s to nvram!\n",generator.c_str());
            
            retassure(!irecv_setenv(_client->recovery->client, "com.apple.System.boot-nonce", generator.c_str()),"failed to write generator to nvram");
            retassure(!irecv_saveenv(_client->recovery->client), "failed to save nvram");
            
            /* send iBEC */
            info("Sending %s (%lu bytes)...\n", "iBEC", iBEC.second);
            mutex_lock(&_client->device_event_mutex);
            irecv_error_t err = irecv_send_buffer(_client->recovery->client, (unsigned char*)(char*)iBEC.first, (unsigned long)iBEC.second, 1);
            retassure(err == IRECV_E_SUCCESS,"ERROR: Unable to send %s component: %s\n", "iBEC", irecv_strerror(err));
            printf("waiting for device to reconnect...\n");
            retassure(!irecv_send_command(_client->recovery->client, "go"),"failed to re-launch iBEC after ApNonce hax");
            getDeviceMode(true);

            info("Booting 2nd iBEC, Waiting for device to disconnect...\n");
            cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
            retassure((_client->mode == &idevicerestore_modes[MODE_UNKNOWN] || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not disconnect after sending hax-iBEC in pwn-iBEC mode");
            mutex_unlock(&_client->device_event_mutex);
            
            info("Booting 2nd iBEC, Waiting for device to reconnect...\n");
            mutex_lock(&_client->device_event_mutex);
            cond_wait_timeout(&_client->device_event_cond, &_client->device_event_mutex, 10000);
            info("mode: %s\n", (_client->mode == &idevicerestore_modes[MODE_RECOVERY]) ? "RECOVERY" : (_client->mode == &idevicerestore_modes[MODE_DFU]) ? "DFU" : (_client->mode == &idevicerestore_modes[MODE_UNKNOWN]) ? "UNKNOWN" : (_client->mode == &idevicerestore_modes[MODE_WTF]) ? "WTF" : "ERR");
            retassure((_client->mode == &idevicerestore_modes[MODE_RECOVERY] || (mutex_unlock(&_client->device_event_mutex),0)), "Device did not reconnect after sending hax-iBEC in pwn-iBEC mode");
            mutex_unlock(&_client->device_event_mutex);

            retassure(!recovery_client_new(_client), "failed to reconnect to recovery after ApNonce hax");
            
            printf("APnonce post-hax:\n");
            get_ap_nonce(_client, &_client->nonce, &_client->nonce_size);
            assure(!irecv_send_command(_client->recovery->client, "bgcolor 255 255 0"));
            retassure(memcmp(_client->nonce, nonceelem.payload(), _client->nonce_size) == 0, "ApNonce from device doesn't match IM4M nonce after applying ApNonce hax. Aborting!");
        }else{
            printf("APNonce from device already matches IM4M nonce, no need for extra hax...\n");
        }
        retassure(!irecv_setenv(_client->recovery->client, "com.apple.System.boot-nonce", generator.c_str()),"failed to write generator to nvram");
        retassure(!irecv_saveenv(_client->recovery->client), "failed to save nvram");
        
        sleep(2); //yes, I like displaying colored screens to the user and making him wait for no reason :P
    }
    
#endif //HAVE_LIBIPATCHER
}

void get_custom_component(struct idevicerestore_client_t* client, plist_t build_identity, const char* component, unsigned char** data, unsigned int *size){
#ifndef HAVE_LIBIPATCHER
    reterror("compiled without libipatcher");
#else
    try {
        auto comp = getIPSWComponent(client, build_identity, component);
        comp = move(libipatcher::decryptFile3((char*)comp.first, comp.second, libipatcher::getFirmwareKey(client->device->product_type, client->build, component)));
        *data = (unsigned char*)(char*)comp.first;
        *size = comp.second;
        comp.first = NULL; //don't free on destruction
    } catch (tihmstar::exception &e) {
        reterror("ERROR: libipatcher failed with reason %d (%s)\n",e.code(),e.what());
    }
    
#endif
}

void futurerestore::doRestore(const char *ipsw){
    plist_t buildmanifest = NULL;
    int delete_fs = 0;
    char* filesystem = NULL;
    cleanup([&]{
        info("Cleaning up...\n");
        safeFreeCustom(buildmanifest, plist_free);
        if (delete_fs && filesystem) unlink(filesystem);
    });
    struct idevicerestore_client_t* client = _client;
    plist_t build_identity = NULL;

    client->ipsw = strdup(ipsw);
    if (_noRestore) client->flags |= FLAG_NO_RESTORE;
    if (!_isUpdateInstall) client->flags |= FLAG_ERASE;
    
    irecv_device_event_subscribe(&client->irecv_e_ctx, irecv_event_cb, client);
    idevice_event_subscribe(idevice_event_cb, client);
    client->idevice_e_ctx = (void*)idevice_event_cb;

    mutex_lock(&client->device_event_mutex);
    cond_wait_timeout(&client->device_event_cond, &client->device_event_mutex, 10000);
    
    retassure(client->mode != &idevicerestore_modes[MODE_UNKNOWN],  "Unable to discover device mode. Please make sure a device is attached.\n");
    if (client->mode != &idevicerestore_modes[MODE_RECOVERY]) {
        retassure(client->mode == &idevicerestore_modes[MODE_DFU], "Device is in unexpected mode detected!");
        retassure(_enterPwnRecoveryRequested, "Device is in DFU mode detected, but we were expecting recovery mode!");
    }else{
        retassure(!_enterPwnRecoveryRequested, "--use-pwndfu was specified, but device found in recovery mode!");
    }
        
    info("Found device in %s mode\n", client->mode->string);
    mutex_unlock(&client->device_event_mutex);

    info("Identified device as %s, %s\n", getDeviceBoardNoCopy(), getDeviceModelNoCopy());

    retassure(!access(client->ipsw, F_OK),"ERROR: Firmware file %s does not exist.\n", client->ipsw); // verify if ipsw file exists

    info("Extracting BuildManifest from iPSW\n");
    {
        int unused;
        retassure(!ipsw_extract_build_manifest(client->ipsw, &buildmanifest, &unused),"ERROR: Unable to extract BuildManifest from %s. Firmware file might be corrupt.\n", client->ipsw);
    }

    /* check if device type is supported by the given build manifest */
    retassure(!build_manifest_check_compatibility(buildmanifest, client->device->product_type),"ERROR: Could not make sure this firmware is suitable for the current device. Refusing to continue.\n");
    
    /* print iOS information from the manifest */
    build_manifest_get_version_information(buildmanifest, client);
    info("Product version: %s\n", client->version);
    info("Product build: %s Major: %d\n", client->build, client->build_major);
    client->image4supported = is_image4_supported(client);
    info("Device supports Image4: %s\n", (client->image4supported) ? "true" : "false");
    
    if (_enterPwnRecoveryRequested) //we are in pwnDFU, so we don't need to check nonces
        client->tss = _aptickets.at(0);
    else if (!(client->tss = nonceMatchesApTickets()))
        reterror("Device ApNonce does not match APTicket nonce\n");

    plist_dict_remove_item(client->tss, "BBTicket");
    plist_dict_remove_item(client->tss, "BasebandFirmware");

    irecv_device_event_unsubscribe(client->irecv_e_ctx);
    idevice_event_unsubscribe();
    client->idevice_e_ctx = NULL;

    irecv_device_event_subscribe(&_client->irecv_e_ctx, irecv_event_cb, _client);

    idevice_event_subscribe(idevice_event_cb, _client);
    _client->idevice_e_ctx = (void *)idevice_event_cb;

    if (_enterPwnRecoveryRequested && _client->image4supported) {
        retassure(plist_dict_get_item(_client->tss, "generator"), "signing ticket file does not contain generator. But a generator is required for 64-bit pwnDFU restore");
    }
    
    retassure(build_identity = getBuildidentityWithBoardconfig(buildmanifest, client->device->hardware_model, _isUpdateInstall),"ERROR: Unable to find any build identities for iPSW\n");

    if (_client->image4supported) {
        if (!(client->sepBuildIdentity = getBuildidentityWithBoardconfig(_sepbuildmanifest, client->device->hardware_model, _isUpdateInstall))){
            retassure(_isPwnDfu, "ERROR: Unable to find any build identities for SEP\n");
            warning("can't find buildidentity for SEP with InstallType=%s. However pwnDFU was requested, so trying fallback to %s",(_isUpdateInstall ? "UPDATE" : "ERASE"),(!_isUpdateInstall ? "UPDATE" : "ERASE"));
            retassure((client->sepBuildIdentity = getBuildidentityWithBoardconfig(_sepbuildmanifest, client->device->hardware_model, !_isUpdateInstall)),
                      "ERROR: Unable to find any build identities for SEP\n");
        }
    }

    plist_t manifest = plist_dict_get_item(build_identity, "Manifest"); //this is the buildidentity used for restore

    printf("checking APTicket to be valid for this restore...\n"); //if we are in pwnDFU, just use first APTicket. We don't need to check nonces.
    auto im4m = (_enterPwnRecoveryRequested || _rerestoreiOS9) ? _im4ms.at(0) : nonceMatchesIM4Ms();

    uint64_t deviceEcid = getDeviceEcid();
    uint64_t im4mEcid = 0;
    if (_client->image4supported) {
        auto ecid = img4tool::getValFromIM4M({im4m.first,im4m.second}, 'ECID');
        im4mEcid = ecid.getIntegerValue();
    }else{
        im4mEcid = getEcidFromSCAB(im4m.first, im4m.second);
    }

    retassure(im4mEcid, "Failed to read ECID from APTicket\n");

    if (im4mEcid != deviceEcid) {
        error("ECID inside APTicket does not match device ECID\n");
        printf("APTicket is valid for %16llu (dec) but device is %16llu (dec)\n",im4mEcid,deviceEcid);
        reterror("APTicket can't be used for restoring this device\n");
    }else
        printf("Verified ECID in APTicket matches device ECID\n");

    if (_client->image4supported) {
        printf("checking APTicket to be valid for this restore...\n");
        uint64_t deviceEcid = getDeviceEcid();

        if (im4mEcid != deviceEcid) {
            error("ECID inside APTicket does not match device ECID\n");
            printf("APTicket is valid for %16llu (dec) but device is %16llu (dec)\n",im4mEcid,deviceEcid);
            reterror("APTicket can't be used for restoring this device\n");
        }else
            printf("Verified ECID in APTicket matches device ECID\n");

        plist_t ticketIdentity = NULL;
        
        try {
            ticketIdentity = img4tool::getBuildIdentityForIm4m({im4m.first,im4m.second}, buildmanifest);
        } catch (tihmstar::exception &e) {
            //
        }
        
        if (!ticketIdentity) {
            printf("Failed to get exact match for build identity, using fallback to ignore certain values\n");
            ticketIdentity = img4tool::getBuildIdentityForIm4m({im4m.first,im4m.second}, buildmanifest, {"RestoreRamDisk","RestoreTrustCache"});
        }

        /* TODO: make this nicer!
           for now a simple pointercompare should be fine, because both plist_t should point into the same buildidentity inside the buildmanifest */
        if (ticketIdentity != build_identity ){
            error("BuildIdentity selected for restore does not match APTicket\n\n");
            printf("BuildIdentity selected for restore:\n");
            img4tool::printGeneralBuildIdentityInformation(build_identity);
            printf("\nBuildIdentity is valid for the APTicket:\n");

            if (ticketIdentity) img4tool::printGeneralBuildIdentityInformation(ticketIdentity),putchar('\n');
            else{
                printf("IM4M is not valid for any restore within the Buildmanifest\n");
                printf("This APTicket can't be used for restoring this firmware\n");
            }
            reterror("APTicket can't be used for this restore\n");
        }else{
            if (!img4tool::isIM4MSignatureValid({im4m.first,im4m.second})){
                printf("IM4M signature is not valid!\n");
                reterror("APTicket can't be used for this restore\n");
            }
            printf("Verified APTicket to be valid for this restore\n");
        }
    }else if (_enterPwnRecoveryRequested){
        info("[WARNING] skipping ramdisk hash check, since device is in pwnDFU according to user\n");

    }else{
        info("[WARNING] full buildidentity check is not implemented, only comparing ramdisk hash.\n");

        auto ticket = getRamdiskHashFromSCAB(im4m.first, im4m.second);
        const char *tickethash = ticket.first;
        size_t tickethashSize = ticket.second;

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
            reterror("APTicket can't be used for this restore\n");
        }
    }

    if (_basebandbuildmanifest){
        if (!(client->basebandBuildIdentity = getBuildidentityWithBoardconfig(_basebandbuildmanifest, client->device->hardware_model, _isUpdateInstall))){
            retassure(client->basebandBuildIdentity = getBuildidentityWithBoardconfig(_basebandbuildmanifest, client->device->hardware_model, !_isUpdateInstall), "ERROR: Unable to find any build identities for Baseband\n");
            info("[WARNING] Unable to find Baseband buildidentities for restore type %s, using fallback %s\n", (_isUpdateInstall) ? "Update" : "Erase",(!_isUpdateInstall) ? "Update" : "Erase");
        }

        client->bbfwtmp = (char*)_basebandPath;

        plist_t bb_manifest = plist_dict_get_item(client->basebandBuildIdentity, "Manifest");
        plist_t bb_baseband = plist_copy(plist_dict_get_item(bb_manifest, "BasebandFirmware"));
        plist_dict_set_item(manifest, "BasebandFirmware", bb_baseband);

        retassure(_client->basebandBuildIdentity, "BasebandBuildIdentity not loaded, refusing to continue");
    }else{
        warning("WARNING: we don't have a basebandbuildmanifest, does not flashing baseband!\n");
    }

    if (_client->image4supported) {
        //check SEP
        plist_t sep_manifest = plist_dict_get_item(client->sepBuildIdentity, "Manifest");
        plist_t sep_sep = plist_copy(plist_dict_get_item(sep_manifest, "SEP"));
        plist_dict_set_item(manifest, "SEP", sep_sep);
        unsigned char genHash[48]; //SHA384 digest length
        ptr_smart<unsigned char *>sephash = NULL;
        uint64_t sephashlen = 0;
        plist_t digest = plist_dict_get_item(sep_sep, "Digest");

        retassure(digest && plist_get_node_type(digest) == PLIST_DATA, "ERROR: can't find SEP digest\n");

        plist_get_data_val(digest, reinterpret_cast<char **>(&sephash), &sephashlen);

        if (sephashlen == 20)
            SHA1((unsigned char*)_client->sepfwdata, (unsigned int)_client->sepfwdatasize, genHash);
        else
            SHA384((unsigned char*)_client->sepfwdata, (unsigned int)_client->sepfwdatasize, genHash);
        retassure(!memcmp(genHash, sephash, sephashlen), "ERROR: SEP does not match sepmanifest\n");
    }

    build_identity_print_information(build_identity); // print information about current build identity

    //check for enterpwnrecovery, because we could be in DFU mode
    if (_enterPwnRecoveryRequested){
        retassure(getDeviceMode(true) == MODE_DFU, "unexpected device mode\n");
        if(_noIBSS) {
            info("RIPBOZO W.I.P. no eta bet patient!\n");
            exit(1);
            // enterPwnRecovery2(build_identity);
        }
        else
            enterPwnRecovery(build_identity);
    }
    
    // Get filesystem name from build identity
    char* fsname = NULL;
    retassure(!build_identity_get_component_path(build_identity, "OS", &fsname), "ERROR: Unable to get path for filesystem component\n");

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
        ipsw_get_file_size(client->ipsw, fsname, (uint64_t*)&fssize);
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

        info("Extracting filesystem from iPSW\n");
        retassure(!ipsw_extract_to_file_with_progress(client->ipsw, fsname, filesystem, 1),"ERROR: Unable to extract filesystem from iPSW\n");

        // rename <fsname>.extract to <fsname>
        if (strstr(filesystem, ".extract")) {
            remove(tmpf);
            rename(filesystem, tmpf);
            free(filesystem);
            filesystem = strdup(tmpf);
        }
    }

    if (_rerestoreiOS9) {
        mutex_lock(&_client->device_event_mutex);
        if (dfu_send_component(client, build_identity, "iBSS") < 0) {
            irecv_close(client->dfu->client);
            client->dfu->client = NULL;
            reterror("ERROR: Unable to send iBSS to device\n");
        }

        /* reconnect */
        dfu_client_free(client);
        
        info("Booting iBSS, Waiting for device to disconnect...\n");
        cond_wait_timeout(&client->device_event_cond, &client->device_event_mutex, 10000);
        retassure((client->mode == &idevicerestore_modes[MODE_UNKNOWN] || (mutex_unlock(&client->device_event_mutex),0)), "Device did not disconnect. Possibly invalid iBSS. Reset device and try again");
        mutex_unlock(&client->device_event_mutex);

        info("Booting iBSS, Waiting for device to reconnect...\n");
        mutex_lock(&_client->device_event_mutex);
        cond_wait_timeout(&client->device_event_cond, &client->device_event_mutex, 10000);
        retassure((client->mode == &idevicerestore_modes[MODE_DFU] || (mutex_unlock(&client->device_event_mutex),0)), "Device did not disconnect. Possibly invalid iBSS. Reset device and try again");
        mutex_unlock(&client->device_event_mutex);
        
        dfu_client_new(client);

        /* send iBEC */
        if (dfu_send_component(client, build_identity, "iBEC") < 0) {
            irecv_close(client->dfu->client);
            client->dfu->client = NULL;
            reterror("ERROR: Unable to send iBEC to device\n");
        }
        
        dfu_client_free(client);
        
        info("Booting iBEC, Waiting for device to disconnect...\n");
        mutex_lock(&_client->device_event_mutex);
        cond_wait_timeout(&client->device_event_cond, &client->device_event_mutex, 10000);
        /* retassure((client->mode == &idevicerestore_modes[MODE_UNKNOWN] || (mutex_unlock(&client->device_event_mutex),0)), "Device did not disconnect. Possibly invalid iBEC. Reset device and try again"); */
        mutex_unlock(&client->device_event_mutex);

        info("Booting iBEC, Waiting for device to reconnect...\n");
        mutex_lock(&_client->device_event_mutex);
        cond_wait_timeout(&client->device_event_cond, &client->device_event_mutex, 10000);
        retassure((client->mode == &idevicerestore_modes[MODE_RECOVERY] || (mutex_unlock(&client->device_event_mutex),0)), "Device did not reconnect. Possibly invalid iBEC. Reset device and try again");
        mutex_unlock(&client->device_event_mutex);

    }else{
        if ((client->build_major > 8)) {
            if (!client->image4supported) {
                /* send APTicket */
                if (recovery_send_ticket(client) < 0) {
                    error("WARNING: Unable to send APTicket\n");
                }
            }
        }
    }

    if (_enterPwnRecoveryRequested){
        if (!_client->image4supported) {
            if (strncmp(client->version, "10.", 3))//if pwnrecovery send all components decrypted, unless we're dealing with iOS 10
                client->recovery_custom_component_function = get_custom_component;
        }
    }else if (!_rerestoreiOS9){
        /* now we load the iBEC */
        retassure(!recovery_send_ibec(client, build_identity),"ERROR: Unable to send iBEC\n");

        printf("waiting for device to reconnect... ");
        recovery_client_free(client);
        
        debug("Waiting for device to disconnect...\n");
        mutex_unlock(&client->device_event_mutex);
        cond_wait_timeout(&client->device_event_cond, &client->device_event_mutex, 10000);
        /* retassure((client->mode == &idevicerestore_modes[MODE_UNKNOWN] || (mutex_unlock(&client->device_event_mutex),0)), "Device did not disconnect. Possibly invalid iBEC. Reset device and try again"); */
        mutex_unlock(&client->device_event_mutex);

        debug("Waiting for device to reconnect...\n");
        mutex_unlock(&client->device_event_mutex);
        cond_wait_timeout(&client->device_event_cond, &client->device_event_mutex, 10000);
        /* retassure((client->mode == &idevicerestore_modes[MODE_RECOVERY] || (mutex_unlock(&client->device_event_mutex),0)), "Device did not disconnect. Possibly invalid iBEC. Reset device and try again"); */
        mutex_unlock(&client->device_event_mutex);
    }

    retassure(client->mode == &idevicerestore_modes[MODE_RECOVERY], "failed to reconnect to device in recovery (iBEC) mode\n");

    //do magic
    if (_client->image4supported) get_sep_nonce(client, &client->sepnonce, &client->sepnonce_size);
    get_ap_nonce(client, &client->nonce, &client->nonce_size);
    get_ecid(client, &client->ecid);

    if (client->mode->index == MODE_RECOVERY) {
        retassure(client->srnm,"ERROR: could not retrieve device serial number. Can't continue.\n");

        if(client->device->chip_id < 0x8015) {
            retassure(!irecv_send_command(client->recovery->client, "bgcolor 0 255 0"), "ERROR: Unable to set bgcolor\n");
            info("[WARNING] Setting bgcolor to green! If you don't see a green screen, then your device didn't boot iBEC correctly\n");
            sleep(2); //show the user a green screen!
        }

        retassure(!recovery_enter_restore(client, build_identity),"ERROR: Unable to place device into restore mode\n");

        recovery_client_free(client);
    }

    if (_client->image4supported) {
        info("getting SEP ticket\n");
        retassure(!get_tss_response(client, client->sepBuildIdentity, &client->septss), "ERROR: Unable to get signing tickets for SEP\n");
        retassure(_client->sepfwdatasize && _client->sepfwdata, "SEP is not loaded, refusing to continue");
    }
    
    mutex_lock(&client->device_event_mutex);
    debug("Waiting for device to enter restore mode...\n");
    cond_wait_timeout(&client->device_event_cond, &client->device_event_mutex, 180000);
    retassure((client->mode == &idevicerestore_modes[MODE_RESTORE] || (mutex_unlock(&client->device_event_mutex),0)), "Device can't enter to restore mode");
    mutex_unlock(&client->device_event_mutex);

    info("About to restore device... \n");
    int result = restore_device(client, build_identity, filesystem);
    if (result == 2) return;
    else retassure(!(result), "ERROR: Unable to restore device\n");
}

int futurerestore::doJustBoot(const char *ipsw, string bootargs){
    reterror("not implemented");
//    int err = 0;
//    //some memory might not get freed if this function throws an exception, but you probably don't want to catch that anyway.
//
//    struct idevicerestore_client_t* client = _client;
//    int unused;
//    int result = 0;
//    plist_t buildmanifest = NULL;
//    plist_t build_identity = NULL;
//
//    client->ipsw = strdup(ipsw);
//
//    getDeviceMode(true);
//    info("Found device in %s mode\n", client->mode->string);
//
//    retassure((client->mode->index == MODE_DFU || client->mode->index == MODE_RECOVERY) && _enterPwnRecoveryRequested, "device not in DFU/Recovery mode\n");
//
//    // discover the device type
//    retassure(check_hardware_model(client) && client->device,"ERROR: Unable to discover device model\n");
//    info("Identified device as %s, %s\n", client->device->hardware_model, client->device->product_type);
//
//    // verify if ipsw file exists
//    retassure(!access(client->ipsw, F_OK), "ERROR: Firmware file %s does not exist.\n", client->ipsw);
//    info("Extracting BuildManifest from IPSW\n");
//
//    retassure(!ipsw_extract_build_manifest(client->ipsw, &buildmanifest, &unused),"ERROR: Unable to extract BuildManifest from %s. Firmware file might be corrupt.\n", client->ipsw);
//
//    /* check if device type is supported by the given build manifest */
//    retassure(!build_manifest_check_compatibility(buildmanifest, client->device->product_type),"ERROR: Could not make sure this firmware is suitable for the current device. Refusing to continue.\n");
//
//    /* print iOS information from the manifest */
//    build_manifest_get_version_information(buildmanifest, client);
//
//    info("Product Version: %s\n", client->version);
//    info("Product Build: %s Major: %d\n", client->build, client->build_major);
//
//    client->image4supported = is_image4_supported(client);
//    info("Device supports Image4: %s\n", (client->image4supported) ? "true" : "false");
//
//    retassure(build_identity = getBuildidentityWithBoardconfig(buildmanifest, client->device->hardware_model, 0),"ERROR: Unable to find any build identities for IPSW\n");
//
//    /* print information about current build identity */
//    build_identity_print_information(build_identity);
//
//    //check for enterpwnrecovery, because we could be in DFU mode
//    retassure(_enterPwnRecoveryRequested, "enterPwnRecoveryRequested is not set, but required");
//
//    retassure(getDeviceMode(true) == MODE_DFU || getDeviceMode(false) == MODE_RECOVERY, "unexpected device mode\n");
//
//    enterPwnRecovery(build_identity, bootargs);
//
//    client->recovery_custom_component_function = get_custom_component;
//
//    for (int i=0;getDeviceMode(true) != MODE_RECOVERY && i<40; i++) putchar('.'),usleep(USEC_PER_SEC*0.5);
//    putchar('\n');
//
//    retassure(check_mode(client), "failed to reconnect to device in recovery (iBEC) mode\n");
//
//    get_ecid(client, &client->ecid);
//
//    client->flags |= FLAG_BOOT;
//
//    if (client->mode->index == MODE_RECOVERY) {
//        retassure(client->srnm,"ERROR: could not retrieve device serial number. Can't continue.\n");
//
//        retassure(!irecv_send_command(client->recovery->client, "bgcolor 0 255 0"), "ERROR: Unable to set bgcolor\n");
//
//        info("[WARNING] Setting bgcolor to green! If you don't see a green screen, then your device didn't boot iBEC correctly\n");
//        sleep(2); //show the user a green screen!
//        client->image4supported = true; //dirty hack to not require apticket
//
//        retassure(!recovery_enter_restore(client, build_identity),"ERROR: Unable to place device into restore mode\n");
//
//        client->image4supported = false;
//        recovery_client_free(client);
//    }
//
//    info("Cleaning up...\n");
//
//error:
//    safeFree(client->sepfwdata);
//    safeFreeCustom(buildmanifest, plist_free);
//    if (!result && !err) info("DONE\n");
//    return result ? abs(result) : err;
}

futurerestore::~futurerestore(){
    recovery_client_free(_client);
    idevicerestore_client_free(_client);
    for (auto im4m : _im4ms){
        safeFree(im4m.first);
    }
    safeFree(_ibootBuild);
    safeFree(_firmwareJson);
    safeFree(_firmwareTokens);
    safeFree(__latestManifest);
    safeFree(__latestFirmwareUrl);
    for (auto plist : _aptickets){
        safeFreeCustom(plist, plist_free);
    }
    safeFreeCustom(_sepbuildmanifest, plist_free);
    safeFreeCustom(_basebandbuildmanifest, plist_free);
}

void futurerestore::loadFirmwareTokens(){
    if (!_firmwareTokens){
        if (!_firmwareJson) _firmwareJson = getFirmwareJson();
        retassure(_firmwareJson,"[TSSC] could not get firmware.json\n");
        int cnt = parseTokens(_firmwareJson, &_firmwareTokens);
        retassure(cnt>0,"[TSSC] parsing %s.json failed\n",(0) ? "ota" : "firmware");
    }
}

const char *futurerestore::getDeviceModelNoCopy(){
    if (!_client->device || !_client->device->product_type){

        int mode = getDeviceMode(true);
        retassure(mode == MODE_NORMAL || mode == MODE_RECOVERY || mode == MODE_DFU, "unexpected device mode=%d\n",mode);
        
        switch (mode) {
        case MODE_RESTORE:
            _client->device = restore_get_irecv_device(_client);
            break;
        case MODE_NORMAL:
            _client->device = normal_get_irecv_device(_client);
            break;
        case MODE_DFU:
        case MODE_RECOVERY:
            _client->device = dfu_get_irecv_device(_client);
            break;
        default:
            break;
        }
    }

    return _client->device->product_type;
}

const char *futurerestore::getDeviceBoardNoCopy(){
    if (!_client->device || !_client->device->product_type){

        int mode = getDeviceMode(true);
        retassure(mode == MODE_NORMAL || mode == MODE_RECOVERY || mode == MODE_DFU, "unexpected device mode=%d\n",mode);
        
        switch (mode) {
        case MODE_RESTORE:
            _client->device = restore_get_irecv_device(_client);
            break;
        case MODE_NORMAL:
            _client->device = normal_get_irecv_device(_client);
            break;
        case MODE_DFU:
        case MODE_RECOVERY:
            _client->device = dfu_get_irecv_device(_client);
            break;
        default:
            break;
        }
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
        retassure(versionCnt, "[TSSC] failed finding latest firmware version\n");
        char *bpos = NULL;
        while((bpos = strstr((char*)(versVals.version = strdup(versions[i++])),"[B]")) != 0){
            free((char*)versVals.version);
            if (--versionCnt == 0) reterror("[TSSC] automatic selection of firmware couldn't find for non-beta versions\n");
        }
        info("[TSSC] selecting latest firmware version: %s\n",versVals.version);
        if (bpos) *bpos= '\0';
        if (versions) free(versions[versionCnt-1]),free(versions);
        
        ptr_smart<const char*>autofree(versVals.version); //make sure it get's freed after function finishes execution by either reaching end or throwing exception
        
        __latestFirmwareUrl = getFirmwareUrl(device, &versVals, _firmwareTokens);
        retassure(__latestFirmwareUrl, "could not find url of latest firmware version\n");
        
        __latestManifest = getBuildManifest(__latestFirmwareUrl, device, versVals.version, versVals.buildID, 0);
        retassure(__latestManifest, "could not get buildmanifest of latest firmware version\n");
    }
    
    return __latestManifest;
}

char *futurerestore::getLatestFirmwareUrl(){
    return getLatestManifest(),__latestFirmwareUrl;
}

void futurerestore::downloadLatestRose(){
    char * manifeststr = getLatestManifest();
    char *roseStr = (elemExists("Rap,RTKitOS", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("Rap,RTKitOS", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);
    if(roseStr) {
        info("downloading Rose firmware\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), roseStr, _rosePath = ROSE_TMP_PATH), "could not download Rose\n");
        loadRose(ROSE_TMP_PATH);
    }
}

void futurerestore::downloadLatestSE(){
    char * manifeststr = getLatestManifest();
    char *seStr = (elemExists("SE,UpdatePayload", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("SE,UpdatePayload", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);
    if(seStr) {
        info("downloading SE firmware\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), seStr, _sePath = SE_TMP_PATH), "could not download SE\n");
        loadSE(SE_TMP_PATH);
    }
}

void futurerestore::downloadLatestSavage(){
    char * manifeststr = getLatestManifest();
    char *savageB0ProdStr = (elemExists("Savage,B0-Prod-Patch", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("Savage,B0-Prod-Patch", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);
    char *savageB0DevStr = (elemExists("Savage,B0-Dev-Patch", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("Savage,B0-Dev-Patch", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);
    char *savageB2ProdStr = (elemExists("Savage,B2-Prod-Patch", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("Savage,B2-Prod-Patch", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);
    char *savageB2DevStr = (elemExists("Savage,B2-Dev-Patch", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("Savage,B2-Dev-Patch", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);
    char *savageBAProdStr = (elemExists("Savage,BA-Prod-Patch", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("Savage,BA-Prod-Patch", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);
    char *savageBADevStr = (elemExists("Savage,BA-Dev-Patch", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("Savage,BA-Dev-Patch", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);

    if(savageB0ProdStr) {
        info("downloading Savage,B0-Prod-Patch\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), savageB0ProdStr, _savagePath[0] = SAVAGE_B0_PP_TMP_PATH), "could not download Savage,B0-Prod-Patch\n");
    }
    if(savageB0DevStr) {
        info("downloading Savage,B0-Dev-Patch\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), savageB0DevStr, _savagePath[1] = SAVAGE_B0_DP_TMP_PATH), "could not download Savage,B0-Dev-Patch\n");
    }
    if(savageB2ProdStr) {
        info("downloading Savage,B2-Prod-Patch\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), savageB2ProdStr, _savagePath[2] = SAVAGE_B2_PP_TMP_PATH), "could not download Savage,B2-Prod-Patch\n");
    }
    if(savageB2DevStr) {
        info("downloading Savage,B2-Dev-Patch\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), savageB2DevStr, _savagePath[3] = SAVAGE_B2_DP_TMP_PATH), "could not download Savage,B2-Dev-Patch\n");
    }
    if(savageBAProdStr) {
        info("downloading Savage,BA-Prod-Patch\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), savageBAProdStr, _savagePath[4] = SAVAGE_BA_PP_TMP_PATH), "could not download Savage,BA-Prod-Patch\n");
    }
    if(savageBADevStr) {
        info("downloading Savage,BA-Dev-Patch\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), savageBADevStr, _savagePath[5] = SAVAGE_BA_DP_TMP_PATH), "could not download Savage,BA-Dev-Patch\n");
    }
    if( savageB0ProdStr     && 
        savageB0DevStr      && 
        savageB2ProdStr     && 
        savageB2DevStr      && 
        savageBAProdStr     && 
        savageBADevStr)        {
            const char*savageArray[6] = {SAVAGE_B0_PP_TMP_PATH, SAVAGE_B0_DP_TMP_PATH, SAVAGE_B2_PP_TMP_PATH, SAVAGE_B2_DP_TMP_PATH, SAVAGE_BA_PP_TMP_PATH, SAVAGE_BA_DP_TMP_PATH};
        loadSavage(savageArray);
    }
}

void futurerestore::downloadLatestVeridian(){
    char * manifeststr = getLatestManifest();
    char *veridianDGMStr = (elemExists("BMU,DigestMap", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("BMU,DigestMap", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);
    char *veridianFWMStr = (elemExists("BMU,FirmwareMap", manifeststr, getDeviceBoardNoCopy(), 0) ? getPathOfElementInManifest("BMU,FirmwareMap", manifeststr, getDeviceBoardNoCopy(), 0) : NULL);
    if(veridianDGMStr) {
        info("downloading Veridian DigestMap\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), veridianDGMStr, _veridianDGMPath = VERIDIAN_DGM_TMP_PATH), "could not download Veridian DigestMap\n");
    }
    if(veridianFWMStr) {
        info("downloading Veridian FirmwareMap\n\n");
        retassure(!downloadPartialzip(getLatestFirmwareUrl(), veridianDGMStr, _veridianFWMPath = VERIDIAN_FWM_TMP_PATH), "could not download Veridian DigestMap\n");
    }
    if(veridianDGMStr && veridianFWMStr)
        loadVeridian(VERIDIAN_DGM_TMP_PATH, VERIDIAN_FWM_TMP_PATH);
}

void futurerestore::downloadLatestFirmwareComponents(){
    info("Downloading the latest firmware components...\n");
    char * manifeststr = getLatestManifest();
    if(elemExists("Rap,RTKitOS", manifeststr, getDeviceBoardNoCopy(), 0))
        downloadLatestRose();
    if(elemExists("SE,UpdatePayload", manifeststr, getDeviceBoardNoCopy(), 0))
        downloadLatestSE();
    if( elemExists("Savage,B0-Prod-Patch", manifeststr, getDeviceBoardNoCopy(), 0)      && 
        elemExists("Savage,B0-Dev-Patch", manifeststr, getDeviceBoardNoCopy(), 0)       && 
        elemExists("Savage,B2-Prod-Patch", manifeststr, getDeviceBoardNoCopy(), 0)      && 
        elemExists("Savage,B2-Dev-Patch", manifeststr, getDeviceBoardNoCopy(), 0)       &&  
        elemExists("Savage,BA-Prod-Patch", manifeststr, getDeviceBoardNoCopy(), 0)      && 
        elemExists("Savage,BA-Dev-Patch", manifeststr, getDeviceBoardNoCopy(), 0))         {
        downloadLatestSavage();
    }
    if(elemExists("BMU,DigestMap", manifeststr, getDeviceBoardNoCopy(), 0) || elemExists("BMU,FirmwareMap", manifeststr, getDeviceBoardNoCopy(), 0))
        downloadLatestVeridian();
    info("Finished downloading the latest firmware components!\n");
}

void futurerestore::loadLatestBaseband(){
    char * manifeststr = getLatestManifest();
    char *pathStr = getPathOfElementInManifest("BasebandFirmware", manifeststr, getDeviceBoardNoCopy(), 0);
    info("downloading Baseband\n\n");
    retassure(!downloadPartialzip(getLatestFirmwareUrl(), pathStr, _basebandPath = BASEBAND_TMP_PATH), "could not download baseband\n");
    saveStringToFile(manifeststr, BASEBAND_MANIFEST_TMP_PATH);
    setBasebandManifestPath(BASEBAND_MANIFEST_TMP_PATH);
    setBasebandPath(BASEBAND_TMP_PATH);
}

void futurerestore::loadLatestSep(){
    char * manifeststr = getLatestManifest();
    char *pathStr = getPathOfElementInManifest("SEP", manifeststr, getDeviceBoardNoCopy(), 0);
    info("downloading SEP\n\n");
    retassure(!downloadPartialzip(getLatestFirmwareUrl(), pathStr, SEP_TMP_PATH), "could not download SEP\n");
    loadSep(SEP_TMP_PATH);
    saveStringToFile(manifeststr, SEP_MANIFEST_TMP_PATH);
    setSepManifestPath(SEP_MANIFEST_TMP_PATH);
}

void futurerestore::setSepManifestPath(const char *sepManifestPath){
    retassure(_sepbuildmanifest = loadPlistFromFile(_sepbuildmanifestPath = sepManifestPath), "failed to load SEPManifest");
}

void futurerestore::setBasebandManifestPath(const char *basebandManifestPath){
    retassure(_basebandbuildmanifest = loadPlistFromFile(_basebandbuildmanifestPath = basebandManifestPath), "failed to load BasebandManifest");
};

void futurerestore::loadRose(const char *rosePath){
    FILE *frose = NULL;
    retassure(frose = fopen(rosePath, "rb"), "failed to read Rose\n");
    
    fseek(frose, 0, SEEK_END);
    _client->rosefwdatasize = ftell(frose);
    fseek(frose, 0, SEEK_SET);
    
    retassure(_client->rosefwdata = (char*)malloc(_client->rosefwdatasize), "failed to malloc memory for Rose\n");
    
    size_t freadRet=0;
    retassure((freadRet = fread(_client->rosefwdata, 1, _client->rosefwdatasize, frose)) == _client->rosefwdatasize,
              "failed to load Rose. size=%zu but fread returned %zu\n",_client->rosefwdatasize,freadRet);
    
    fclose(frose);
}

void futurerestore::loadSE(const char *sePath){
    FILE *fse = NULL;
    retassure(fse = fopen(sePath, "rb"), "failed to read SE\n");
    
    fseek(fse, 0, SEEK_END);
    _client->sefwdatasize = ftell(fse);
    fseek(fse, 0, SEEK_SET);
    
    retassure(_client->sefwdata = (char*)malloc(_client->sefwdatasize), "failed to malloc memory for SE\n");
    
    size_t freadRet=0;
    retassure((freadRet = fread(_client->sefwdata, 1, _client->sefwdatasize, fse)) == _client->sefwdatasize,
              "failed to load SE. size=%zu but fread returned %zu\n",_client->sefwdatasize,freadRet);
    
    fclose(fse);
}

void futurerestore::loadSavage(const char *savagePath[6]){
    for(int i = 0; i < 6; i++) {
        FILE *fsavage = NULL;
        retassure(fsavage = fopen(savagePath[i], "rb"), "failed to read Savage: %d\n", i);
        
        fseek(fsavage, 0, SEEK_END);
        _client->savagefwdatasize[i] = ftell(fsavage);
        fseek(fsavage, 0, SEEK_SET);
        
        retassure(_client->savagefwdata[i] = (char*)malloc(_client->savagefwdatasize[i]), "failed to malloc memory for Savage: %i\n", i);
        
        size_t freadRet=0;
        retassure((freadRet = fread(_client->savagefwdata[i], 1, _client->savagefwdatasize[i], fsavage)) == _client->savagefwdatasize[i],
                "failed to load Savage: %d. size=%zu but fread returned %zu\n",i,_client->savagefwdatasize[i],freadRet);
        
        fclose(fsavage);
    }
}

void futurerestore::loadVeridian(const char *veridianDGMPath, const char *veridianFWMPath){
    FILE *fveridiandgm = NULL;
    FILE *fveridianfwm = NULL;
    retassure(fveridiandgm = fopen(veridianDGMPath, "rb"), "failed to read Veridian DigestMap\n");
    retassure(fveridianfwm = fopen(veridianFWMPath, "rb"), "failed to read Veridian FirmwareMap\n");
    
    fseek(fveridiandgm, 0, SEEK_END);
    fseek(fveridianfwm, 0, SEEK_END);
    _client->veridiandgmfwdatasize = ftell(fveridiandgm);
    _client->veridianfwmfwdatasize = ftell(fveridianfwm);
    fseek(fveridiandgm, 0, SEEK_SET);
    fseek(fveridianfwm, 0, SEEK_SET);
    
    retassure(_client->veridiandgmfwdata = (char*)malloc(_client->veridiandgmfwdatasize), "failed to malloc memory for Veridian DigestMap\n");
    retassure(_client->veridianfwmfwdata = (char*)malloc(_client->veridianfwmfwdatasize), "failed to malloc memory for Veridian FirmwareMap\n");
    
    size_t freadRet=0;
    retassure((freadRet = fread(_client->veridiandgmfwdata, 1, _client->veridiandgmfwdatasize, fveridiandgm)) == _client->veridiandgmfwdatasize,
              "failed to load Veridian DigestMap. size=%zu but fread returned %zu\n",_client->veridiandgmfwdatasize,freadRet);
    retassure((freadRet = fread(_client->sefwdata, 1, _client->veridianfwmfwdatasize, fveridianfwm)) == _client->veridianfwmfwdatasize,
              "failed to load Veridian FirmwareMap. size=%zu but fread returned %zu\n",_client->veridianfwmfwdatasize,freadRet);
    
    fclose(fveridiandgm);
    fclose(fveridianfwm);
}

void futurerestore::loadSep(const char *sepPath){
    FILE *fsep = NULL;
    retassure(fsep = fopen(sepPath, "rb"), "failed to read SEP\n");
    
    fseek(fsep, 0, SEEK_END);
    _client->sepfwdatasize = ftell(fsep);
    fseek(fsep, 0, SEEK_SET);
    
    retassure(_client->sepfwdata = (char*)malloc(_client->sepfwdatasize), "failed to malloc memory for SEP\n");
    
    size_t freadRet=0;
    retassure((freadRet = fread(_client->sepfwdata, 1, _client->sepfwdatasize, fsep)) == _client->sepfwdatasize,
              "failed to load SEP. size=%zu but fread returned %zu\n",_client->sepfwdatasize,freadRet);
    
    fclose(fsep);
}

void futurerestore::setBasebandPath(const char *basebandPath){
    FILE *fbb = NULL;

    retassure(fbb = fopen(basebandPath, "rb"), "failed to read Baseband");
    _basebandPath = basebandPath;
    fclose(fbb);
}

#pragma mark static methods
inline void futurerestore::saveStringToFile(const char *str, const char *path){
    FILE *f = NULL;
    retassure(f = fopen(path, "w"), "can't save file at %s\n",path);
    size_t len = strlen(str);
    size_t wlen = fwrite(str, 1, len, f);
    fclose(f);
    retassure(len == wlen, "saving file failed, wrote=%zu actual=%zu\n",wlen,len);
}

std::pair<const char *,size_t> futurerestore::getNonceFromSCAB(const char* scab, size_t scabSize){
    retassure(scab, "Got empty SCAB\n");
    
    img4tool::ASN1DERElement bacs(scab,scabSize);
    
    try {
        bacs[3];
    } catch (...) {
        reterror("unexpected number of Elements in SCAB sequence (expects 4)\n");
    }
        
    img4tool::ASN1DERElement mainSet = bacs[1];

    for (auto &elem : mainSet) {
        if (*(uint8_t*)elem.buf() == 0x92) {
            return {(char*)elem.payload(),elem.payloadSize()};
        }
    }
    reterror("failed to get nonce from SCAB");
    return {NULL,0};
}

uint64_t futurerestore::getEcidFromSCAB(const char* scab, size_t scabSize){
    retassure(scab, "Got empty SCAB\n");
    
    img4tool::ASN1DERElement bacs(scab,scabSize);
    
    try {
        bacs[3];
    } catch (...) {
        reterror("unexpected number of Elements in SCAB sequence (expects 4)\n");
    }
    
    img4tool::ASN1DERElement mainSet = bacs[1];

    for (auto &elem : mainSet) {
        if (*(uint8_t*)elem.buf() == 0x81) {
            uint64_t ret = 0;
            for (int i=0; i<elem.payloadSize(); i++) {
                ret <<=8;
                ret |= ((uint8_t*)elem.payload())[i];
            }
            return __bswap_64(ret);
        }
    }

    reterror("failed to get ECID from SCAB");
    return 0;
}

std::pair<const char *,size_t>futurerestore::getRamdiskHashFromSCAB(const char* scab, size_t scabSize){
    retassure(scab, "Got empty SCAB\n");

    img4tool::ASN1DERElement bacs(scab,scabSize);

    try {
        bacs[3];
    } catch (...) {
        reterror("unexpected number of Elements in SCAB sequence (expects 4)\n");
    }

    img4tool::ASN1DERElement mainSet = bacs[1];

    for (auto &elem : mainSet) {
        if (*(uint8_t*)elem.buf() == 0x9A) {
            return {(char*)elem.payload(),elem.payloadSize()};
        }
    }
    reterror("failed to get nonce from SCAB");
    return {NULL,0};
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

char *futurerestore::getPathOfElementInManifest(const char *element, const char *manifeststr, const char *boardConfig, int isUpdateInstall){
    char *pathStr = NULL;
    ptr_smart<plist_t> buildmanifest(NULL,plist_free);
    
    plist_from_xml(manifeststr, (uint32_t)strlen(manifeststr), &buildmanifest);
    
    if (plist_t identity = getBuildidentityWithBoardconfig(buildmanifest._p, boardConfig, isUpdateInstall))
        if (plist_t manifest = plist_dict_get_item(identity, "Manifest"))
            if (plist_t elem = plist_dict_get_item(manifest, element))
                if (plist_t info = plist_dict_get_item(elem, "Info"))
                    if (plist_t path = plist_dict_get_item(info, "Path"))
                        if (plist_get_string_val(path, &pathStr), pathStr)
                            goto noerror;
    
    reterror("could not get %s path\n",element);
noerror:
    return pathStr;
}

bool futurerestore::elemExists(const char *element, const char *manifeststr, const char *boardConfig, int isUpdateInstall){
    char *pathStr = NULL;
    ptr_smart<plist_t> buildmanifest(NULL,plist_free);
    
    plist_from_xml(manifeststr, (uint32_t)strlen(manifeststr), &buildmanifest);
    
    if (plist_t identity = getBuildidentityWithBoardconfig(buildmanifest._p, boardConfig, isUpdateInstall))
        if (plist_t manifest = plist_dict_get_item(identity, "Manifest"))
            if (plist_t elem = plist_dict_get_item(manifest, element))
                if (plist_t info = plist_dict_get_item(elem, "Info"))
                    if (plist_t path = plist_dict_get_item(info, "Path"))
                        if (plist_get_string_val(path, &pathStr), pathStr)
                            goto noerror;
    
    return false;
noerror:
    return true;
}

std::string futurerestore::getGeneratorFromSHSH2(const plist_t shsh2){
    plist_t pGenerator = NULL;
    uint64_t gen = 0;
    char *genstr = NULL;
    cleanup([&]{
        safeFree(genstr);
    });
    retassure(pGenerator = plist_dict_get_item(shsh2, "generator"), "signing ticket file does not contain generator");
    
    retassure(plist_get_node_type(pGenerator) == PLIST_STRING, "generator has unexpected type! We expect string of the format 0x%16llx");
    
    plist_get_string_val(pGenerator, &genstr);
    assure(genstr);
    
    sscanf(genstr, "0x%16llx",&gen);
    retassure(gen, "failed to parse generator. Make sure it is in format 0x%16llx");
    
    return {genstr};
}
