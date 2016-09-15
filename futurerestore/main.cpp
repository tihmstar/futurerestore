
//  main.cpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <iostream>
#include <getopt.h>
#include "futurerestore.hpp"
#include "all_tsschecker.h"
#include "tsschecker.h"

#define safeFree(buf) if (buf) free(buf), buf = NULL
#define safePlistFree(buf) if (buf) plist_free(buf), buf = NULL

static struct option longopts[] = {
    { "apticket",       required_argument,      NULL, 't' },
    { "baseband",       required_argument,      NULL, 'b' },
    { "baseband-plist", required_argument,      NULL, 'p' },
    { "sep",            required_argument,      NULL, 's' },
    { "sep-manifest",   required_argument,      NULL, 'm' },
    { "wait",           no_argument,            NULL, 'w' },
    { "update",         no_argument,            NULL, 'u' },
    { "debug",          no_argument,            NULL, 'd' },
    { NULL, 0, NULL, 0 }
};

#define FLAG_WAIT   1 << 0
#define FLAG_UPDATE 1 << 1

void cmd_help(){
    printf("Usage: futurerestore [OPTIONS] IPSW\n");
    printf("Allows restoring nonmatching iOS/Sep/Baseband\n\n");
    
    printf("  -t, --apticket PATH\t\tApticket used for restoring\n");
    printf("  -b, --baseband PATH\t\tBaseband to be flashed\n");
    printf("  -p, --baseband-manifest PATH\t\tBuildmanifest for requesting baseband ticket\n");
    printf("  -s, --sep PATH\t\tSep to be flashed\n");
    printf("  -m, --sep-manifest PATH\t\tBuildmanifest for requesting sep ticket\n");
    printf("  -w, --wait\t\tkeep rebooting until nonce matches APTicket\n");
    printf("  -u, --update\t\tupdate instead of erase install\n");
    printf("\n");
}

using namespace std;
int main(int argc, const char * argv[]) {
#define reterror(code,a ...) do {error(a); err = code; goto error;} while (0)
    int err=0;
    int res = 0;
    
    int optindex = 0;
    int opt = 0;
    long flags = 0;
    
    int isSepManifestSigned = 0;
    int isBasebandSigned = 0;
    
    const char *ipsw = NULL;
    const char *apticketPath = NULL;
    const char *basebandPath = NULL;
    const char *basebandManifestPath = NULL;
    const char *sepPath = NULL;
    const char *sepManifestPath = NULL;
    
    
    t_devicevals devVals;
    t_iosVersion versVals;
    memset(&devVals, 0, sizeof(devVals));
    memset(&versVals, 0, sizeof(versVals));
    
    if (argc == 1){
        cmd_help();
        return -1;
    }
    
    while ((opt = getopt_long(argc, (char* const *)argv, "ht:b:p:s:m:wud", longopts, &optindex)) > 0) {
        switch (opt) {
            case 't': // long option: "apticket"; can be called as short option
                apticketPath = optarg;
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
    client.init();
    
    if (apticketPath) client.loadAPTicket(apticketPath);
    
    if (flags & FLAG_WAIT){
        client.putDeviceIntoRecovery();
        client.waitForNonce();
    }
    if (!(apticketPath && basebandPath && basebandManifestPath && sepPath && sepManifestPath && ipsw)) {
        if (!(flags & FLAG_WAIT) || ipsw){
            error("missing argument\n");
            cmd_help();
            err = -2;
        }else{
            info("done\n");
        }
        goto error;
    }

    
    versVals.basebandMode = kBasebandModeWithoutBaseband;
    if (!(isSepManifestSigned = isManifestSignedForDevice(sepManifestPath, NULL, devVals, versVals))){
        reterror(-3,"sep firmware isn't signed\n");
    }
    
    versVals.basebandMode = kBasebandModeOnlyBaseband;
    if (!(isBasebandSigned = isManifestSignedForDevice(basebandManifestPath, NULL, devVals, versVals))){
        reterror(-3,"baseband firmware isn't signed\n");
    }
    
    client.setSepPath(sepPath);
    client.setSepManifestPath(sepManifestPath);
    client.setBasebandPath(basebandPath);
    client.setBasebandManifestPath(basebandManifestPath);
    
    
    client.putDeviceIntoRecovery();
    
    try {
        res = client.doRestore(ipsw, flags & FLAG_UPDATE);
    } catch (int error) {
        if (error == -20) error("maybe you forgot -w ?\n");
        err = error;
    }
    
    cout << "Done: restoring "<< (res ? "succeeded" : "failed");
    if (!res) cout << ". Errorcode="<<err;
    cout<<endl;
error:

    return err;
#undef reterror
}
