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
#include "futurerestore.hpp"
#include "common.h"
#include "all_tsschecker.h"
#include "../external/img4tool/img4tool/img4.h"
#include "img4tool.h"
#include "normal.h"
#include "recovery.h"
#include "ipsw.h"
#include "locking.h"
#include "restore.h"
#include "tsschecker.h"


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


#define reterror(code,msg ...) error(msg),throw int(code)
#define safeFree(buf) if (buf) free(buf), buf = NULL
#define safePlistFree(buf) if (buf) plist_free(buf), buf = NULL

futurerestore::futurerestore(){
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
    return _didInit = (check_mode(_client) != MODE_UNKNOWN);
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
        recovery_client_free(_client);
        return check_mode(_client);
    }
}

void futurerestore::putDeviceIntoRecovery(){
    if (!_didInit) reterror(-1, "did not init\n");
    
    getDeviceMode(false);
    info("Found device in %s mode\n", _client->mode->string);
    if (_client->mode->index == MODE_NORMAL) {
        info("Entering recovery mode...\n");
        if (normal_enter_recovery(_client) < 0) {
            reterror(-2,"Unable to place device into recovery mode from %s mode\n", _client->mode->string);
        }
    }else if (_client->mode->index == MODE_RECOVERY){
        info("Device already in Recovery mode\n");
    }else{
        reterror(-3, "unsupported devicemode, please put device in recovery mode or normal mode\n");
    }
    
    //only needs to be freed manually when function did't throw exception
    safeFree(_client->udid);
    
    //these get also freed by destructor
    normal_client_free(_client);
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
    if (getDeviceMode(true) != MODE_RECOVERY) reterror(-10, "Device not in recovery mode, can't check apnonce\n");
    
    unsigned char* realnonce;
    int realNonceSize = 0;
    recovery_get_ap_nonce(_client, &realnonce, &realNonceSize);
    
    vector<const char*>nonces;
    
    for (int i=0; i< _im4ms.size(); i++){
        if (memcmp(realnonce, (unsigned const char*)getNonceFromIM4M(_im4ms[i],NULL), realNonceSize) == 0) return _aptickets[i];
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
    
    for (int i=0; i< _im4ms.size(); i++){
        if (memcmp(realnonce, (unsigned const char*)getNonceFromIM4M(_im4ms[i],NULL), realNonceSize) == 0) return _im4ms[i];
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
    
    for (auto im4m : _im4ms){
        nonces.push_back(getNonceFromIM4M(im4m,&nonceSize));
    }
    
    waitForNonce(nonces,nonceSize);
}


void futurerestore::loadAPTickets(const vector<const char *> &apticketPaths){
    for (auto apticketPath : apticketPaths){
        
        FILE *f = fopen(apticketPath,"rb");
        if (!f) reterror(-9, "failed to load apticket at %s\n",apticketPath);
        fseek(f, 0, SEEK_END);
        
        size_t fSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = (char*)malloc(fSize+1);
        memset(buf, 0, fSize+1);
        
        size_t freadRet = 0;
        if ((freadRet = fread(buf, 1, fSize, f)) != fSize){
            reterror(-15,"fread=%zu but fSize=%zu",freadRet,fSize);
        }
        
        fclose(f);
        
        plist_t apticket = NULL;
        char *im4m = NULL;
        
        if (memcmp(buf, "bplist00", 8) == 0)
            plist_from_bin(buf, (uint32_t)fSize, &apticket);
        else
            plist_from_xml(buf, (uint32_t)fSize, &apticket);
        
        
        plist_t ticket = plist_dict_get_item(apticket, "ApImg4Ticket");
        uint64_t im4msize=0;
        plist_get_data_val(ticket, &im4m, &im4msize);
        
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

int futurerestore::doRestore(const char *ipsw, bool noerase){
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
    plist_t bb_build_identity = NULL;
    
    client->ipsw = strdup(ipsw);
    if (!noerase) client->flags |= FLAG_ERASE;
    
    getDeviceMode(true);
    info("Found device in %s mode\n", client->mode->string);
    if (client->mode->index != MODE_RECOVERY) reterror(-6, "device not in recovery mode\n");
    // discover the device type
    if (check_hardware_model(client) == NULL || client->device == NULL) {
        reterror(-2,"ERROR: Unable to discover device model\n");
    }
    info("Identified device as %s, %s\n", client->device->hardware_model, client->device->product_type);
    
    if (!(client->tss = nonceMatchesApTickets())) reterror(-20, "Devicenonce does not match APTicket nonce\n");
    
    
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
    
    plist_dict_remove_item(client->tss, "BBTicket");
    plist_dict_remove_item(client->tss, "BasebandFirmware");
    
    if (!(build_identity = getBuildidentityWithBoardconfig(buildmanifest, client->device->hardware_model, noerase)))
        reterror(-5,"ERROR: Unable to find any build identities for IPSW\n");

    if (!(sep_build_identity = getBuildidentityWithBoardconfig(_sepbuildmanifest, client->device->hardware_model, noerase)))
        reterror(-5,"ERROR: Unable to find any build identities for SEP\n");

    //this is the buildidentity used for restore
    plist_t manifest = plist_dict_get_item(build_identity, "Manifest");

    printf("checking APTicket to be valid for this restore...\n");
    plist_t ticketIdentity = getBuildIdentityForIM4M(nonceMatchesIM4Ms(), buildmanifest);
    
    //TODO: make this nicer!
    //for now a simple pointercompare should be fine, because both plist_t should point into the same buildidentity inside the buildmanifest
    if (ticketIdentity != build_identity){
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
        printf("Verified APTicket to be valid for this restore\n");
    }
    
    
    if (_basebandbuildmanifest){
        if (!(bb_build_identity = getBuildidentityWithBoardconfig(_basebandbuildmanifest, client->device->hardware_model, noerase)))
            reterror(-5,"ERROR: Unable to find any build identities for Baseband\n");
        
        plist_t bb_manifest = plist_dict_get_item(bb_build_identity, "Manifest");
        plist_t bb_baseband = plist_copy(plist_dict_get_item(bb_manifest, "BasebandFirmware"));
        plist_dict_set_item(manifest, "BasebandFirmware", bb_baseband);
        client->bbfwtmp = (char*)_basebandPath;
        client->basebandBuildIdentity = getBuildidentity(_basebandbuildmanifest, getDeviceModelNoCopy(), 0);
        
        if (!_client->basebandBuildIdentity)
            reterror(-55, "BasebandBuildIdentity not loaded, refusing to continue");
    }else{
        warning("WARNING: we don't have a basebandbuildmanifest, not flashing baseband!\n");
    }
    
    plist_t sep_manifest = plist_dict_get_item(sep_build_identity, "Manifest");
    plist_t sep_sep = plist_copy(plist_dict_get_item(sep_manifest, "SEP"));
    plist_dict_set_item(manifest, "SEP", sep_sep);
    
    
    
    
    /* print information about current build identity */
    build_identity_print_information(build_identity);
    
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
    
    if ((client->build_major > 8)) {
        if (!client->image4supported) {
            /* send ApTicket */
            if (recovery_send_ticket(client) < 0) {
                error("WARNING: Unable to send APTicket\n");
            }
        }
    }
    
    /* now we load the iBEC */
    if (recovery_send_ibec(client, build_identity) < 0) {
        reterror(-8,"ERROR: Unable to send iBEC\n");
    }
    recovery_client_free(client);
    
    /* this must be long enough to allow the device to run the iBEC */
    /* FIXME: Probably better to detect if the device is back then */
    sleep(7);
    
    check_mode(client);
    
    //do magic
    get_sep_nonce(client, &client->sepnonce, &client->sepnonce_size);
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
        warning("[WARNING] Setting bgcolor to green! If you don't see a green screen, then your ticket probably doesn't match the firmware you're trying to restore to\n");
        sleep(2); //show the user a green screen!
        if (recovery_enter_restore(client, build_identity) < 0) {
            reterror(-10,"ERROR: Unable to place device into restore mode\n");
        }
        
        recovery_client_free(client);
    }
    
    
    if (get_tss_response(client, sep_build_identity, &client->septss) < 0) {
        reterror(-11,"ERROR: Unable to get SHSH blobs for SEP\n");
    }
    
    
    
    if (!_client->sepfwdatasize || !_client->sepfwdata)
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


futurerestore::~futurerestore(){
    normal_client_free(_client);
    recovery_client_free(_client);
    idevicerestore_client_free(_client);
    for (auto im4m : _im4ms){
        safeFree(im4m);
    }
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
        if (mode != MODE_NORMAL && mode != MODE_RECOVERY)
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
        char **versions = getListOfiOSForDevice(_firmwareJson, _firmwareTokens, device, 0, &versionCnt);
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
        
        __latestFirmwareUrl = getFirmwareUrl(device, &versVals, _firmwareJson, _firmwareTokens);
        if (!__latestFirmwareUrl) reterror(-21, "could not find url of latest firmware\n");
        
        __latestManifest = getBuildManifest(__latestFirmwareUrl, device, versVals.version, 0);
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
    setBasebandManifestPath(BASEBAND_TMP_PATH);
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
    if (!(_sepbuildmanifest = loadPlistFromFile(sepManifestPath)))
        reterror(-14, "failed to load SEPManifest");
}

void futurerestore::setBasebandManifestPath(const char *basebandManifestPath){
    if (!(_basebandbuildmanifest = loadPlistFromFile(basebandManifestPath)))
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

char *futurerestore::getNonceFromIM4M(const char* im4m, size_t *nonceSize){
    char *ret = NULL;
    t_asn1Tag *mainSet = NULL;
    t_asn1Tag *manbSet = NULL;
    t_asn1Tag *manpSet = NULL;
    char *nonceOctet = NULL;
    char *bnch = NULL;
    char *manb = NULL;
    char *manp = NULL;
    
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
    
    bnch = getValueForTagInSet((char*)manpSet, 0x424e4348); //BNCH priv Tag
    if (asn1ElementsInObject(bnch)< 2){
        error("unexpected number of Elements in BNCH sequence\n");
        goto error;
    }
    nonceOctet = (char*)asn1ElementAtIndex(bnch, 1);
    nonceOctet++;
    
    ret = (char*)malloc(asn1Len(nonceOctet).dataLen);
    if (ret){
        memcpy(ret, nonceOctet + asn1Len(nonceOctet).sizeBytes, asn1Len(nonceOctet).dataLen);        
        if (nonceSize) *nonceSize = asn1Len(nonceOctet).dataLen;
    }
    
    
error:
    return ret;

}

char *futurerestore::getNonceFromAPTicket(const char* apticketPath){
    char *ret = NULL;
    if (char *im4m = im4mFormShshFile(apticketPath)){
        ret = getNonceFromIM4M(im4m,NULL);
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

