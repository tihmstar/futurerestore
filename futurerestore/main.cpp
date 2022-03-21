//
//  main.cpp
//  futurerestore
//
//  Created by tihmstar on 14.09.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <getopt.h>
#include "futurerestore.hpp"

extern "C"{
#include "tsschecker.h"
#undef VERSION_COMMIT_SHA
#undef VERSION_COMMIT_COUNT
#undef VERSION_RELEASE
};

#include <libgeneral/macros.h>
#include <img4tool/img4tool.hpp>
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
        { "apticket",                   required_argument,      nullptr, 't' },
        { "baseband",                   required_argument,      nullptr, 'b' },
        { "baseband-manifest",          required_argument,      nullptr, 'p' },
        { "sep",                        required_argument,      nullptr, 's' },
        { "sep-manifest",               required_argument,      nullptr, 'm' },
        { "wait",                       no_argument,            nullptr, 'w' },
        { "update",                     no_argument,            nullptr, 'u' },
        { "debug",                      no_argument,            nullptr, 'd' },
        { "exit-recovery",              no_argument,            nullptr, 'e' },
        { "custom-latest",              required_argument,      nullptr, 'c' },
        { "custom-latest-buildid",      required_argument,      nullptr, 'g' },
        { "help",                       no_argument,            nullptr, 'h' },
        { "custom-latest-beta",         no_argument,            nullptr, 'i' },
        { "latest-sep",                 no_argument,            nullptr, '0' },
        { "no-restore",                 no_argument,            nullptr, 'z' },
        { "latest-baseband",            no_argument,            nullptr, '1' },
        { "no-baseband",                no_argument,            nullptr, '2' },
#ifdef HAVE_LIBIPATCHER
        { "use-pwndfu",                 no_argument,            nullptr, '3' },
        { "no-ibss",                    no_argument,            nullptr, '4' },
        { "rdsk",                       required_argument,      nullptr, '5' },
        { "rkrn",                       required_argument,      nullptr, '6' },
        { "set-nonce",                  optional_argument,      nullptr, '7' },
        { "serial",                     no_argument,            nullptr, '8' },
        { "boot-args",                  required_argument,      nullptr, '9' },
        { "no-cache",                   no_argument,            nullptr, 'a' },
        { "skip-blob",                  no_argument,            nullptr, 'f' },
#endif
        { nullptr, 0, nullptr, 0 }
};

#define FLAG_WAIT                   1 << 0
#define FLAG_UPDATE                 1 << 1
#define FLAG_LATEST_SEP             1 << 2
#define FLAG_LATEST_BASEBAND        1 << 3
#define FLAG_NO_BASEBAND            1 << 4
#define FLAG_IS_PWN_DFU             1 << 5
#define FLAG_NO_IBSS                1 << 6
#define FLAG_RESTORE_RAMDISK        1 << 7
#define FLAG_RESTORE_KERNEL         1 << 8
#define FLAG_SET_NONCE              1 << 9
#define FLAG_SERIAL                 1 << 10
#define FLAG_BOOT_ARGS              1 << 11
#define FLAG_NO_CACHE               1 << 12
#define FLAG_SKIP_BLOB              1 << 13
#define FLAG_NO_RESTORE_FR          1 << 14
#define FLAG_CUSTOM_LATEST          1 << 15
#define FLAG_CUSTOM_LATEST_BUILDID  1 << 16
#define FLAG_CUSTOM_LATEST_BETA     1 << 17

void cmd_help(){
    printf("Usage: futurerestore [OPTIONS] iPSW\n");
    printf("Allows restoring to non-matching firmware with custom SEP+baseband\n");
    printf("\nGeneral options:\n");
    printf("  -h, --help\t\t\t\tShows this usage message\n");
    printf("  -t, --apticket PATH\t\t\tSigning tickets used for restoring\n");
    printf("  -u, --update\t\t\t\tUpdate instead of erase install (requires appropriate APTicket)\n");
    printf("              \t\t\t\tDO NOT use this parameter, if you update from jailbroken firmware!\n");
    printf("  -w, --wait\t\t\t\tKeep rebooting until ApNonce matches APTicket (ApNonce collision, unreliable)\n");
    printf("  -d, --debug\t\t\t\tShow all code, use to save a log for debug testing\n");
    printf("  -e, --exit-recovery\t\t\tExit recovery mode and quit\n");
    printf("  -z, --no-restore\t\t\tDo not restore and end right before NOR data is sent\n");
    printf("  -c, --custom-latest VERSION\t\tSpecify custom latest version to use for SEP, Baseband and other FirmwareUpdater components\n");
    printf("  -g, --custom-latest-buildid BUILDID\tSpecify custom latest buildid to use for SEP, Baseband and other FirmwareUpdater components\n");
    printf("  -i, --custom-latest-beta\t\tGet custom url from list of beta firmwares");

#ifdef HAVE_LIBIPATCHER
    printf("\nOptions for downgrading with Odysseus:\n");
    printf("      --use-pwndfu\t\t\tRestoring devices with Odysseus method. Device needs to be in pwned DFU mode already\n");
    printf("      --no-ibss\t\t\t\tRestoring devices with Odysseus method. For checkm8/iPwnder32 specifically, bootrom needs to be patched already with unless iPwnder.\n");
    printf("      --rdsk PATH\t\t\tSet custom restore ramdisk for entering restoremode(requires use-pwndfu)\n");
    printf("      --rkrn PATH\t\t\tSet custom restore kernelcache for entering restoremode(requires use-pwndfu)\n");
    printf("      --set-nonce\t\t\tSet custom nonce from your blob then exit recovery(requires use-pwndfu)\n");
    printf("      --set-nonce=0xNONCE\t\tSet custom nonce then exit recovery(requires use-pwndfu)\n");
    printf("      --serial\t\t\t\tEnable serial during boot(requires serial cable and use-pwndfu)\n");
    printf("      --boot-args\t\t\tSet custom restore boot-args(PROCEED WITH CAUTION)(requires use-pwndfu)\n");
    printf("      --no-cache\t\t\tDisable cached patched iBSS/iBEC(requires use-pwndfu)\n");
    printf("      --skip-blob\t\t\tSkip SHSH blob validation(PROCEED WITH CAUTION)(requires use-pwndfu)\n");
#endif

    printf("\nOptions for SEP:\n");
    printf("      --latest-sep\t\t\tUse latest signed SEP instead of manually specifying one\n");
    printf("  -s, --sep PATH\t\t\tSEP to be flashed\n");
    printf("  -m, --sep-manifest PATH\t\tBuildManifest for requesting SEP ticket\n");

    printf("\nOptions for baseband:\n");
    printf("      --latest-baseband\t\t\tUse latest signed baseband instead of manually specifying one\n");
    printf("  -b, --baseband PATH\t\t\tBaseband to be flashed\n");
    printf("  -p, --baseband-manifest PATH\t\tBuildManifest for requesting baseband ticket\n");
    printf("      --no-baseband\t\t\tSkip checks and don't flash baseband\n");
    printf("                   \t\t\tOnly use this for device without a baseband (eg. iPod touch or some Wi-Fi only iPads)\n\n");
}

using namespace std;
using namespace tihmstar;
int main_r(int argc, const char * argv[]) {
#ifdef WIN32
    DWORD termFlags;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(handle, &termFlags))
        SetConsoleMode(handle, termFlags | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    int err=0;
    printf("Version: " VERSION_RELEASE "(" VERSION_COMMIT_SHA "-" VERSION_COMMIT_COUNT ")\n");
    printf("%s\n",tihmstar::img4tool::version());
#ifdef HAVE_LIBIPATCHER
    printf("%s\n",libipatcher::version());
    printf("Odysseus for 32-bit support: yes\n");
    printf("Odysseus for 64-bit support: %s\n",(libipatcher::has64bitSupport() ? "yes" : "no"));
#else
    printf("Odysseus support: no\n");
#endif

    int optindex = 0;
    int opt;
    long flags = 0;
    bool exitRecovery = false;

    const char *ipsw = nullptr;
    const char *basebandPath = nullptr;
    const char *basebandManifestPath = nullptr;
    const char *sepPath = nullptr;
    const char *sepManifestPath = nullptr;
    const char *bootargs = nullptr;
    std::string customLatest;
    std::string customLatestBuildID;
    const char *ramdiskPath = nullptr;
    const char *kernelPath = nullptr;
    const char *custom_nonce = nullptr;

    vector<const char*> apticketPaths;

    t_devicevals devVals = {nullptr};
    t_iosVersion versVals = {nullptr};

    if (argc == 1){
        cmd_help();
        return -1;
    }

    while ((opt = getopt_long(argc, (char* const *)argv, "ht:b:p:s:m:c:g:hiwude0z123456789af", longopts, &optindex)) > 0) {
        switch (opt) {
            case 'h': // long option: "help"; can be called as short option
                cmd_help();
                return 0;
                break;
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
            case 'c': // long option: "custom-latest"; can be called as short option
                customLatest = (optarg) ? std::string(optarg) : std::string("");
                flags |= FLAG_CUSTOM_LATEST;
                break;
            case 'g': // long option: "custom-latest-buildid"; can be called as short option
                customLatestBuildID = (optarg) ? std::string(optarg) : std::string("");
                flags |= FLAG_CUSTOM_LATEST_BUILDID;
                break;
            case 'i': // long option: "custom-latest-beta"; can be called as short option
                flags |= FLAG_CUSTOM_LATEST_BETA;
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
            case '4': // long option: "no-ibss";
                flags |= FLAG_NO_IBSS;
                break;
            case '5': // long option: "rdsk";
                flags |= FLAG_RESTORE_RAMDISK;
                ramdiskPath = optarg;
                break;
            case '6': // long option: "rkrn";
                flags |= FLAG_RESTORE_KERNEL;
                kernelPath = optarg;
                break;
            case '7': // long option: "set-nonce";
                flags |= FLAG_SET_NONCE;
                custom_nonce = (optarg) ? optarg : nullptr;
                if(custom_nonce != nullptr) {
                    uint64_t gen;
                    retassure(strlen(custom_nonce) == 16 || strlen(custom_nonce) == 18,"Incorrect nonce length!\n");
                    gen = std::stoul(custom_nonce, nullptr, 16);
                    retassure(gen, "failed to parse generator. Make sure it is in format 0x%16llx");
                }
                break;
            case '8': // long option: "serial";
                flags |= FLAG_SERIAL;
                break;
            case '9': // long option: "boot-args";
                flags |= FLAG_BOOT_ARGS;
                bootargs = (optarg) ? optarg : nullptr;
                break;
            case 'a': // long option: "no-cache";
                flags |= FLAG_NO_CACHE;
                break;
            case 'f': // long option: "skip-blob";
                flags |= FLAG_SKIP_BLOB;
                break;
#endif
            case 'e': // long option: "exit-recovery"; can be called as short option
                exitRecovery = true;
                break;
            case 'z': // long option: "no-restore"; can be called as short option
                flags |= FLAG_NO_RESTORE_FR;
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
        argv += optind;

        ipsw = argv[0];
    }else if (argc == optind && flags & FLAG_WAIT) {
        info("User requested to only wait for ApNonce to match, but not for actually restoring\n");
    }else if (exitRecovery){
        info("Exiting from recovery mode to normal mode\n");
    }else{
        error("argument parsing failed! agrc=%d optind=%d\n",argc,optind);
        if (idevicerestore_debug){
            for (int i=0; i<argc; i++) {
                printf("argv[%d]=%s\n",i,argv[i]);
            }
        }
        return -5;
    }

    futurerestore client(flags & FLAG_UPDATE, flags & FLAG_IS_PWN_DFU, flags & FLAG_NO_IBSS, flags & FLAG_SET_NONCE, flags & FLAG_SERIAL, flags & FLAG_NO_RESTORE_FR);
    retassure(client.init(),"can't init, no device found\n");

    printf("futurerestore init done\n");
    if(flags & FLAG_NO_IBSS)
        retassure((flags & FLAG_IS_PWN_DFU),"--no-ibss requires --use-pwndfu\n");
    if(flags & FLAG_RESTORE_RAMDISK)
        retassure((flags & FLAG_IS_PWN_DFU),"--rdsk requires --use-pwndfu\n");
    if(flags & FLAG_RESTORE_KERNEL)
        retassure((flags & FLAG_IS_PWN_DFU),"--rkrn requires --use-pwndfu\n");
    if(flags & FLAG_SET_NONCE)
        retassure((flags & FLAG_IS_PWN_DFU),"--set-nonce requires --use-pwndfu\n");
    if(flags & FLAG_SET_NONCE && client.is32bit())
        error("--set-nonce not supported on 32bit devices.\n");
    if(flags & FLAG_RESTORE_RAMDISK)
        retassure((flags & FLAG_RESTORE_KERNEL),"--rdsk requires --rkrn\n");
    if(flags & FLAG_SERIAL) {
        retassure((flags & FLAG_IS_PWN_DFU),"--serial requires --use-pwndfu\n");
        retassure(!(flags & FLAG_BOOT_ARGS),"--serial conflicts with --boot-args\n");
    }
    if(flags & FLAG_BOOT_ARGS)
        retassure((flags & FLAG_IS_PWN_DFU),"--boot-args requires --use-pwndfu\n");
    if(flags & FLAG_NO_CACHE)
        retassure((flags & FLAG_IS_PWN_DFU),"--no-cache requires --use-pwndfu\n");
    if(flags & FLAG_SKIP_BLOB)
        retassure((flags & FLAG_IS_PWN_DFU),"--skip-blob requires --use-pwndfu\n");
    if(flags & FLAG_CUSTOM_LATEST_BETA)
        retassure((flags & FLAG_CUSTOM_LATEST_BUILDID),"-i, --custom-latest-beta requires -g, --custom-latest-buildid\n");
    if(flags & FLAG_CUSTOM_LATEST_BUILDID)
        retassure((flags & FLAG_CUSTOM_LATEST) == 0,"-g, --custom-latest-buildid is not compatible with -c, --custom-latest\n");

    if (exitRecovery) {
        client.exitRecovery();
        info("Done\n");
        return 0;
    }

    try {
        if (!apticketPaths.empty()) {
            client.loadAPTickets(apticketPaths);
        }

        if(!customLatest.empty()) {
            client.setCustomLatest(customLatest);
        }
        if(!customLatestBuildID.empty()) {
            client.setCustomLatestBuildID(customLatestBuildID, (flags & FLAG_CUSTOM_LATEST_BETA) != 0);
        }

        if (!(
                ((!apticketPaths.empty() && ipsw)
                 && ((basebandPath && basebandManifestPath) || ((flags & FLAG_LATEST_BASEBAND) || (flags & FLAG_NO_BASEBAND)))
                 && ((sepPath && sepManifestPath) || (flags & FLAG_LATEST_SEP) || client.is32bit())
                ) || (ipsw && (flags & FLAG_IS_PWN_DFU))
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

        devVals.deviceModel = (char*)client.getDeviceModelNoCopy();
        devVals.deviceBoard = (char*)client.getDeviceBoardNoCopy();

        if(flags & FLAG_RESTORE_RAMDISK) {
            client.setRamdiskPath(ramdiskPath);
            client.loadRamdisk(ramdiskPath);
        }

        if(flags & FLAG_RESTORE_KERNEL) {
            client.setKernelPath(kernelPath);
            client.loadKernel(kernelPath);
        }

        if(flags & FLAG_SET_NONCE) {
            client.setNonce(custom_nonce);
        }

        if(flags & FLAG_BOOT_ARGS) {
            client.setBootArgs(bootargs);
        }

        if(flags & FLAG_NO_CACHE) {
            client.disableCache();
        }

        if(flags & FLAG_SKIP_BLOB) {
            client.skipBlobValidation();
        }

        if (flags & FLAG_LATEST_SEP){
            info("user specified to use latest signed SEP\n");
            client.downloadLatestSep();
        }else if (!client.is32bit()){
            client.setSepPath(sepPath);
            client.setSepManifestPath(sepManifestPath);
            client.loadSep(sepPath);
            client.loadSepManifest(sepManifestPath);
        }

        versVals.basebandMode = kBasebandModeWithoutBaseband;
        if (!client.is32bit() && !(isManifestSignedForDevice(client.getSepManifestPath().c_str(), &devVals, &versVals, nullptr))){
            reterror("SEP firmware is NOT being signed!\n");
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
                info("user specified to use latest signed baseband\n");
                client.downloadLatestBaseband();
            }else{
                client.setBasebandPath(basebandPath);
                client.setBasebandManifestPath(basebandManifestPath);
                client.loadBaseband(basebandPath);
                client.loadBasebandManifest(basebandManifestPath);
                printf("Did set SEP+baseband path and firmware\n");
            }

            versVals.basebandMode = kBasebandModeOnlyBaseband;
            if (!(devVals.bbgcid = client.getBasebandGoldCertIDFromDevice())){
                printf("[WARNING] using tsschecker's fallback to get BasebandGoldCertID. This might result in invalid baseband signing status information\n");
            }
            if (!(isManifestSignedForDevice(client.getBasebandManifestPath().c_str(), &devVals, &versVals, nullptr))) {
                reterror("baseband firmware is NOT being signed!\n");
            }
        }

        if(!client.is32bit())
            client.downloadLatestFirmwareComponents();
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
