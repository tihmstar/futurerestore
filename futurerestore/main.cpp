//
//  main.cpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <iostream>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include "futurerestore.hpp"

extern "C"{
#include "tsschecker.h"
#undef VERSION_COMMIT_SHA
#undef VERSION_COMMIT_COUNT
};

#include <libgeneral/macros.h>
#ifdef HAVE_LIBIPATCHER
#include <libipatcher/libipatcher.hpp>
#endif

#ifdef WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

static struct option longopts[] = {
    { "apticket",           required_argument,      NULL, 't' },
    { "baseband",           required_argument,      NULL, 'b' },
    { "baseband-manifest",  required_argument,      NULL, 'p' },
    { "sep",                required_argument,      NULL, 's' },
    { "sep-manifest",       required_argument,      NULL, 'm' },
    { "wait",               no_argument,            NULL, 'w' },
    { "update",             no_argument,            NULL, 'u' },
    { "debug",              no_argument,            NULL, 'd' },
    { "exit-recovery",      no_argument,            NULL, 'e' },
    { "latest-sep",         no_argument,            NULL, '0' },
    { "latest-baseband",    no_argument,            NULL, '1' },
    { "no-baseband",        no_argument,            NULL, '2' },
#ifdef HAVE_LIBIPATCHER
    { "use-pwndfu",         no_argument,            NULL, '3' },
    { "just-boot",          optional_argument,      NULL, '4' },
#endif
    { NULL, 0, NULL, 0 }
};

#define FLAG_WAIT               1 << 0
#define FLAG_UPDATE             1 << 1
#define FLAG_LATEST_SEP         1 << 2
#define FLAG_LATEST_BASEBAND    1 << 3
#define FLAG_NO_BASEBAND        1 << 4
#define FLAG_IS_PWN_DFU         1 << 5

void cmd_help(){
    printf("Usage: futurerestore [OPTIONS] iPSW\n");
    printf("Allows restoring to non-matching firmware with custom SEP+baseband\n");
    printf("\nGeneral options:\n");
    printf("  -t, --apticket PATH\t\tSigning tickets used for restoring\n");
    printf("  -u, --update\t\t\tUpdate instead of erase install (requires appropriate APTicket)\n");
    printf("              \t\t\tDO NOT use this parameter, if you update from jailbroken firmware!\n");
    printf("  -w, --wait\t\t\tKeep rebooting until ApNonce matches APTicket (ApNonce collision, unreliable)\n");
    printf("  -d, --debug\t\t\tShow all code, use to save a log for debug testing\n");
    printf("  -e, --exit-recovery\t\tExit recovery mode and quit\n");
    
#ifdef HAVE_LIBIPATCHER
    printf("\nOptions for downgrading with Odysseus:\n");
    printf("      --use-pwndfu\t\tRestoring devices with Odysseus method. Device needs to be in pwned DFU mode already\n");
    printf("      --just-boot=\"-v\"\t\tTethered booting the device from pwned DFU mode. You can optionally set boot-args\n");
#endif
        
    printf("\nOptions for SEP:\n");
    printf("      --latest-sep\t\tUse latest signed SEP instead of manually specifying one (may cause bad restore)\n");
    printf("  -s, --sep PATH\t\tSEP to be flashed\n");
    printf("  -m, --sep-manifest PATH\tBuildManifest for requesting SEP ticket\n");
        
    printf("\nOptions for baseband:\n");
    printf("      --latest-baseband\t\tUse latest signed baseband instead of manually specifying one (may cause bad restore)\n");
    printf("  -b, --baseband PATH\t\tBaseband to be flashed\n");
    printf("  -p, --baseband-manifest PATH\tBuildManifest for requesting baseband ticket\n");
    printf("      --no-baseband\t\tSkip checks and don't flash baseband\n");
    printf("                   \t\tOnly use this for device without a baseband (eg. iPod touch or some Wi-Fi only iPads)\n\n");
}

#ifdef WIN32
    DWORD termFlags;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(handle, &termFlags))
        SetConsoleMode(handle, termFlags | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

using namespace std;
using namespace tihmstar;
int main_r(int argc, const char * argv[]) {
    int err=0;
    printf("Version: " VERSION_COMMIT_SHA " - " VERSION_COMMIT_COUNT "\n");
#ifdef HAVE_LIBIPATCHER
    printf("%s\n",libipatcher::version());
    printf("Odysseus for 32-bit support: yes\n");
    printf("Odysseus for 64-bit support: %s\n",(libipatcher::has64bitSupport() ? "yes" : "no"));
#else
    printf("Odysseus support: no\n");
#endif

    int optindex = 0;
    int opt = 0;
    long flags = 0;
    bool exitRecovery = false;
    
    int isSepManifestSigned = 0;
    int isBasebandSigned = 0;
    
    const char *ipsw = NULL;
    const char *basebandPath = NULL;
    const char *basebandManifestPath = NULL;
    const char *sepPath = NULL;
    const char *sepManifestPath = NULL;
    const char *bootargs = NULL;
    
    vector<const char*> apticketPaths;
    
    t_devicevals devVals = {0};
    t_iosVersion versVals = {0};
    
    if (argc == 1){
        cmd_help();
        return -1;
    }

    while ((opt = getopt_long(argc, (char* const *)argv, "ht:b:p:s:m:wude0123", longopts, &optindex)) > 0) {
        switch (opt) {
            case 't': // long option: "apticket"; can be called as short option
                apticketPaths.push_back(optarg);
                break;
            case 'b': // long option: "baseband"; can be called as short option
                basebandPath = optarg;
                break;
            case 'p': // long option: "baseband-manifest"; can be called as short option
                basebandManifestPath = optarg;
                break;
            case 's': // long option: "sep"; can be called as short option
                sepPath = optarg;
                break;
            case 'm': // long option: "sep-manifest"; can be called as short option
                sepManifestPath = optarg;
                break;
            case 'w': // long option: "wait"; can be called as short option
                flags |= FLAG_WAIT;
                break;
            case 'u': // long option: "update"; can be called as short option
                flags |= FLAG_UPDATE;
                break;
            case '0': // long option: "latest-sep";
                flags |= FLAG_LATEST_SEP;
                break;
            case '1': // long option: "latest-baseband";
                flags |= FLAG_LATEST_BASEBAND;
                break;
            case '2': // long option: "no-baseband";
                flags |= FLAG_NO_BASEBAND;
                break;
#ifdef HAVE_LIBIPATCHER
            case '3': // long option: "use-pwndfu";
                flags |= FLAG_IS_PWN_DFU;
                break;
            case '4': // long option: "just-boot";
                bootargs = (optarg) ? optarg : "";
                break;
            break;
#endif
            case 'e': // long option: "exit-recovery"; can be called as short option
                exitRecovery = true;
                break;
            case 'd': // long option: "debug"; can be called as short option
                idevicerestore_debug = 1;
                break;
            default:
                cmd_help();
                return -1;
        }
    }
    
    if (argc-optind == 1) {
        argc -= optind;
        argv += optind;
        
        ipsw = argv[0];
    }else if (argc == optind && flags & FLAG_WAIT) {
        info("User requested to only wait for ApNonce to match, but not for actually restoring\n");
    }else if (exitRecovery){
        info("Exiting to recovery mode\n");
    }else{
        error("argument parsing failed! agrc=%d optind=%d\n",argc,optind);
        if (idevicerestore_debug){
            for (int i=0; i<argc; i++) {
                printf("argv[%d]=%s\n",i,argv[i]);
            }
        }
        return -5;
    }
    
    futurerestore client(flags & FLAG_UPDATE, flags & FLAG_IS_PWN_DFU);
    retassure(client.init(),"can't init, no device found\n");
    
    printf("futurerestore init done\n");
    retassure(!bootargs || (flags & FLAG_IS_PWN_DFU),"--just-boot requires --use-pwndfu\n");
    
    if (exitRecovery) {
        client.exitRecovery();
        info("Done\n");
        return 0;
    }
    
    try {
        if (apticketPaths.size()) client.loadAPTickets(apticketPaths);
        
        if (!(
              ((apticketPaths.size() && ipsw)
               && ((basebandPath && basebandManifestPath) || ((flags & FLAG_LATEST_BASEBAND) || (flags & FLAG_NO_BASEBAND)))
               && ((sepPath && sepManifestPath) || (flags & FLAG_LATEST_SEP) || client.is32bit())
              ) || (ipsw && bootargs && (flags & FLAG_IS_PWN_DFU))
            )) {
            
            if (!(flags & FLAG_WAIT) || ipsw){
                error("missing argument\n");
                cmd_help();
                err = -2;
            }else{
                client.putDeviceIntoRecovery();
                client.waitForNonce();
                info("Done\n");
            }
            goto error;
        }
        if (bootargs){
            
        }else{
            devVals.deviceModel = (char*)client.getDeviceModelNoCopy();
            devVals.deviceBoard = (char*)client.getDeviceBoardNoCopy();
            
            if (flags & FLAG_LATEST_SEP){
                info("user specified to use latest signed SEP (WARNING, THIS CAN CAUSE A NON-WORKING RESTORE)\n");
                client.loadLatestSep();
            }else if (!client.is32bit()){
                client.loadSep(sepPath);
                client.setSepManifestPath(sepManifestPath);
            }
            
            versVals.basebandMode = kBasebandModeWithoutBaseband;
            if (!client.is32bit() && !(isSepManifestSigned = isManifestSignedForDevice(client.sepManifestPath(), &devVals, &versVals))){
                reterror("SEP firmware doesn't signed\n");
            }
            
            if (flags & FLAG_NO_BASEBAND){
                printf("\nWARNING: user specified is not to flash a baseband. This can make the restore fail if the device needs a baseband!\n");
                printf("if you added this flag by mistake, you can press CTRL-C now to cancel\n");
                int c = 10;
                printf("continuing restore in ");
                while (c) {
                    printf("%d ",c--);
                    fflush(stdout);
                    sleep(1);
                }
                printf("\n");
            }else{
                if (flags & FLAG_LATEST_BASEBAND){
                    info("user specified to use latest signed baseband (WARNING, THIS CAN CAUSE A NON-WORKING RESTORE)\n");
                    client.loadLatestBaseband();
                }else{
                    client.setBasebandPath(basebandPath);
                    client.setBasebandManifestPath(basebandManifestPath);
                    printf("Did set SEP+baseband path and firmware\n");
                }
                
                versVals.basebandMode = kBasebandModeOnlyBaseband;
                if (!(devVals.bbgcid = client.getBasebandGoldCertIDFromDevice())){
                    printf("[WARNING] using tsschecker's fallback to get BasebandGoldCertID. This might result in invalid baseband signing status information\n");
                }
                if (!(isBasebandSigned = isManifestSignedForDevice(client.basebandManifestPath(), &devVals, &versVals))) {
                    reterror("baseband firmware doesn't signed\n");
                }
            }
        }
        client.putDeviceIntoRecovery();
        if (flags & FLAG_WAIT){
            client.waitForNonce();
        }
    } catch (int error) {
        err = error;
        printf("[Error] Fail code=%d\n",err);
        goto error;
    }
    
    try {
        if (bootargs)
            client.doJustBoot(ipsw,bootargs);
        else
            client.doRestore(ipsw);
        printf("Done: restoring succeeded!\n");
    } catch (tihmstar::exception &e) {
        e.dump();
        printf("Done: restoring failed!\n");
    }
    
error:
    if (err){
        printf("Failed with error code=%d\n",err);
    }
    return err;
#undef reterror
}

int main(int argc, const char * argv[]) {
#ifdef DEBUG
    return main_r(argc, argv);
#else
    try {
        return main_r(argc, argv);
    } catch (tihmstar::exception &e) {
        printf("%s: failed with exception:\n",PACKAGE_NAME);
        e.dump();
        return e.code();
    }
#endif
}
