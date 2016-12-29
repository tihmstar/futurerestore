
//  main.cpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <iostream>
#include <getopt.h>
#include <string.h>
#include <vector>
#include "futurerestore.hpp"
#include "all_tsschecker.h"
#include "tsschecker.h"

#define safeFree(buf) if (buf) free(buf), buf = NULL
#define safePlistFree(buf) if (buf) plist_free(buf), buf = NULL

static struct option longopts[] = {
    { "apticket",           required_argument,      NULL, 't' },
    { "baseband",           required_argument,      NULL, 'b' },
    { "baseband-plist",     required_argument,      NULL, 'p' },
    { "sep",                required_argument,      NULL, 's' },
    { "sep-manifest",       required_argument,      NULL, 'm' },
    { "wait",               no_argument,            NULL, 'w' },
    { "update",             no_argument,            NULL, 'u' },
    { "debug",              no_argument,            NULL, 'd' },
    { "latest-sep",         no_argument,            NULL, '0' },
    { "latest-baseband",    no_argument,            NULL, '1' },
    { NULL, 0, NULL, 0 }
};

#define FLAG_WAIT               1 << 0
#define FLAG_UPDATE             1 << 1
#define FLAG_LATEST_SEP         1 << 2
#define FLAG_LATEST_BASEBAND    1 << 3

void cmd_help(){
    printf("Usage: futurerestore [OPTIONS] IPSW\n");
    printf("Allows restoring nonmatching iOS/Sep/Baseband\n\n");
    
    printf("  -t, --apticket PATH\t\tApticket used for restoring\n");
    printf("  -b, --baseband PATH\t\tBaseband to be flashed\n");
    printf("  -p, --baseband-manifest PATH\tBuildmanifest for requesting baseband ticket\n");
    printf("  -s, --sep PATH\t\tSep to be flashed\n");
    printf("  -m, --sep-manifest PATH\tBuildmanifest for requesting sep ticket\n");
    printf("  -w, --wait\t\t\tkeep rebooting until nonce matches APTicket\n");
    printf("  -u, --update\t\t\tupdate instead of erase install\n");
    printf("      --latest-sep\t\t\tuse latest signed sep instead of manually specifying one(may cause bad restore)\n");
    printf("      --latest-baseband\t\t\tse latest signed baseband instead of manually specifying one(may cause bad restore)\n");
    printf("\n");
}

using namespace std;
int main(int argc, const char * argv[]) {
#define reterror(code,a ...) do {error(a); err = code; goto error;} while (0)
    int err=0;
    int res = -1;
    
    int optindex = 0;
    int opt = 0;
    long flags = 0;
    
    int isSepManifestSigned = 0;
    int isBasebandSigned = 0;
    
    const char *ipsw = NULL;
    const char *basebandPath = NULL;
    const char *basebandManifestPath = NULL;
    const char *sepPath = NULL;
    const char *sepManifestPath = NULL;
    
    vector<const char*> apticketPaths;
    
    t_devicevals devVals;
    t_iosVersion versVals;
    memset(&devVals, 0, sizeof(devVals));
    memset(&versVals, 0, sizeof(versVals));
    
    if (argc == 1){
        cmd_help();
        return -1;
    }
    
    while ((opt = getopt_long(argc, (char* const *)argv, "ht:b:p:s:m:wud01", longopts, &optindex)) > 0) {
        switch (opt) {
            case 't': // long option: "apticket"; can be called as short option
                apticketPaths.push_back(optarg);
                break;
            case 'b': // long option: "baseband"; can be called as short option
                basebandPath = optarg;
                break;
            case 'p': // long option: "baseband-plist"; can be called as short option
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
    }
    
    futurerestore client;
    if (!client.init()) reterror(-3,"can't init, no device found\n");
    
    if (apticketPaths.size()) client.loadAPTickets(apticketPaths);
    
    if (!((apticketPaths.size() && ipsw)
        && ((basebandPath && basebandManifestPath) || (flags & FLAG_LATEST_BASEBAND))
        && ((sepPath && sepManifestPath) || (flags & FLAG_LATEST_SEP)))) {
        if (!(flags & FLAG_WAIT) || ipsw){
            error("missing argument\n");
            cmd_help();
            err = -2;
        }else{
            client.putDeviceIntoRecovery();
            client.waitForNonce();
            info("done\n");
        }
        goto error;
    }

    
    if (flags & FLAG_LATEST_SEP){
        info("user specified to use latest signed sep\n");
        client.loadLatestSep();
    }else{
        client.setSepPath(sepPath);
        client.setSepManifestPath(sepManifestPath);
    }
    if (flags & FLAG_LATEST_BASEBAND){
        info("user specified to use latest signed baseband (WARNING, THIS CAN CAUSE A NON-WORKING RESTORE)\n");
        client.loadLatestBaseband();
    }else{
        client.setBasebandPath(basebandPath);
        client.setBasebandManifestPath(basebandManifestPath);
    }

    
    versVals.basebandMode = kBasebandModeWithoutBaseband;
    if (!(isSepManifestSigned = isManifestSignedForDevice(client.sepManifestPath(), NULL, &devVals, &versVals))){
        reterror(-3,"sep firmware isn't signed\n");
    }
    
    versVals.basebandMode = kBasebandModeOnlyBaseband;
    if (!(isBasebandSigned = isManifestSignedForDevice(client.basebandManifestPath(), NULL, &devVals, &versVals))){
        reterror(-3,"baseband firmware isn't signed\n");
    }
    
    client.putDeviceIntoRecovery();
    if (flags & FLAG_WAIT){
        client.waitForNonce();
    }
    
    try {
        res = client.doRestore(ipsw, flags & FLAG_UPDATE);
    } catch (int error) {
        if (error == -20) error("maybe you forgot -w ?\n");
        err = error;
    }
    
    cout << "Done: restoring "<< (!res ? "succeeded" : "failed");
    if (res) cout << ". Errorcode="<<err;
    cout<<endl;
error:

    return err;
#undef reterror
}
