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

#include <libgeneral/macros.h>

#include <tsschecker/TssRequest.hpp>
#include <tsschecker/tsschecker.hpp>

#include <ra1nsn0w/ra1nsn0w_plugins.hpp>

#ifdef WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

extern int idevicerestore_debug;

static struct option longopts[] = {
    { "debug",              no_argument,            NULL, 'd' },
    { "ecid",               required_argument,      NULL, 'e' },
    { "ramdisk",            required_argument,      NULL, 'r' },
    { "apticket",           required_argument,      NULL, 't' },
    { "fresh-apticket",     optional_argument,      NULL, 'T' },
    { "update",             no_argument,            NULL, 'u' },
    { "32bit",              no_argument,            NULL,  0  },
    { "decrypt-ramdisk",    no_argument,            NULL,  0  },
    { "no-ticket",          no_argument,            NULL,  0  },
    { "ra1nsn0warg",        required_argument,      NULL,  0  },
    { "ra1nsn0warg-persist",required_argument,      NULL,  0  },
    { "restore-pwnage",     no_argument,            NULL,  0  },
    { "restore-24kpwn",     no_argument,            NULL,  0  },

    { "tethered-downgrade", required_argument,      NULL,  0  },
    { "bootargs",           required_argument,      NULL, 'b' },

    { "default-sep",        no_argument,            NULL,  0  },
    { "latest-sep",         no_argument,            NULL,  0  },
    { "sep",                required_argument,      NULL, 's' },
    { "sep-manifest",       required_argument,      NULL, 'm' },

    { "latest-baseband",    no_argument,            NULL,  0  },
    { "baseband",           required_argument,      NULL, 'B' },
    { "baseband-manifest",  required_argument,      NULL, 'M' },
    { "no-baseband",        no_argument,            NULL,  0  },

    { NULL, 0, NULL, 0 }
};

void cmd_help(){
    printf(
            "Usage: futurerestore [OPTIONS] IPSW\n"
            "Allows restoring to non-matching firmware with custom SEP+baseband\n"
            "\nGeneral options:\n"
            "  -d, --debug\t\t\t\tShow all code, use to save a log for debug testing\n"
            "  -e, --ecid\t\t\t\tSet device ECID\n"
            "  -r, --ramdisk PATH\t\t\tBoot custom ramdisk\n"
            "  -t, --apticket PATH\t\t\tSigning tickets used for restoring\n"
            "  -T, --fresh-apticket [VARIANT]\tSigning tickets used for restoring\n"
            "  -u, --update\t\t\t\tUpdate instead of erase install (requires appropriate APTicket)\n"
            "              \t\t\t\tDO NOT use this parameter, if you update from jailbroken firmware!\n"
            "      --32bit\t\t\t\tUse 32bit patchfinder instead of 64bit\n"
            "      --decrypt-ramdisk\t\t\tDecrypt ramdisk before sending (needed for iOS 3.0 and 3.0.1 restore)\n"
            "      --no-ticket\t\t\tDon't expect ticket (used for iOS 2.0 restore)\n"
            "      --ra1nsn0warg <arg>\t\tPass extra arg to ra1nsn0w (can use multiple times)\n"
            "      --ra1nsn0warg-persist <arg>\t\tPass extra arg to ra1nsn0w for persistent patches (for use with restore-pwnage)\n"
            "      --restore-pwnage\t\tFlash jailbroken bootchain (only works for 0x8900 devices)\n"
            "      --restore-24kpwn\t\tFlash jailbroken bootchain with 24kpwn exploit (only works for 0x8720 and 0x8920 devices)\n"

            "\nOptions for downgrading with Odysseus:\n"
            "  -b, --bootargs=\"-v\"\t\t\tSet bootargs\n"
            "      --tethered-downgrade=<LLB IPSW>\tSkip APTicket checks when downgrading (flash donor LLB from other IPSW)\n"
                
            "\nOptions for SEP:\n"
            "  -s, --sep PATH\t\tSEP to be flashed\n"
            "  -m, --sep-manifest PATH\tBuildManifest for requesting SEP ticket\n"
            "      --latest-sep\t\tUse latest signed SEP instead of manually specifying one (may cause bad restore)\n"
            "      --default-sep\t\tUse the SEP inside the IPSW\n"

            
            "\nOptions for baseband:\n"
            "  -B, --baseband PATH\t\tBaseband to be flashed\n"
            "  -M, --baseband-manifest PATH\tBuildManifest for requesting baseband ticket\n"
            "      --latest-baseband\t\tUse latest signed baseband instead of manually specifying one (may cause bad restore)\n"
            "      --no-baseband\t\tSkip baseband checks\n"
            "                   \t\tOnly use this for device without a baseband (eg. iPod touch or some Wi-Fi only iPads)\n\n"
           );
}
using namespace tihmstar;

MAINFUNCTION
int main_r(int argc, const char * argv[]) {
    info("%s", VERSION_STRING);
    
    if (ra1nsn0w::loadPlugins()) {
        info("ra1nsn0w plugin support: yes");
    }else{
        info("ra1nsn0w plugin support: no");
    }

    int optindex = 0;
    int opt = 0;

    uint64_t ecid = 0;

    const char *ipsw = NULL;
    const char *llbipsw = NULL;
    const char *bootargs = NULL;
    const char *requestFreshAPTicketVariant = NULL;
    const char *basebandPath = NULL;
    const char *basebandManifestPath = NULL;
    const char *SEPPath = NULL;
    const char *SEPManifestPath = NULL;
    const char *ramdiskPath = NULL;

    bool is32Bit = false;
    bool isUpdateRestore = false;
    bool getLatestBaseband = false;
    bool getLatestSEP = false;
    bool useDefaultSEP = false;
    bool skipBasebandChecks = false;
    bool dontExpectAPTicket = false;
    bool doRestorePwnage = false;
    bool doRestore24kPwn = false;
    bool needsDecryptedRamdisk = false;

    tihmstar::ra1nsn0w::launchConfig cfg = {};
    tihmstar::ra1nsn0w::launchConfig cfg_persist = {};

    std::vector<const char*> apticketPaths;
        
    if (argc == 1){
        cmd_help();
        return -1;
    }
    
    std::vector<const char*> ra1nargs;
    std::vector<const char*> ra1nargs_persist;
    ra1nargs.push_back("");
    ra1nargs_persist.push_back("");

    while ((opt = getopt_long(argc, (char* const *)argv, "hb:B:de:m:M:r:s:t:T::u", longopts, &optindex)) >= 0) {
        switch (opt) {
            case 0: //long opts
            {
                std::string curopt = longopts[optindex].name;
                
                if (curopt == "32bit") {
                    is32Bit = true;
                }else if (curopt == "decrypt-ramdisk"){
                    needsDecryptedRamdisk = true;
                }else if (curopt == "default-sep"){
                    useDefaultSEP = true;
                }else if (curopt == "latest-baseband"){
                    getLatestBaseband = true;
                }else if (curopt == "latest-sep"){
                    getLatestSEP = true;
                }else if (curopt == "no-baseband"){
                    skipBasebandChecks = true;
                }else if (curopt == "no-ticket"){
                    dontExpectAPTicket = true;
                }else if (curopt == "ra1nsn0warg"){
                    ra1nargs.push_back(optarg);
                }else if (curopt == "ra1nsn0warg-persist"){
                    ra1nargs_persist.push_back(optarg);
                }else if (curopt == "restore-pwnage"){
                    doRestorePwnage = true;
                }else if (curopt == "restore-24kpwn"){
                    doRestore24kPwn = true;
                }else if (curopt == "tethered-downgrade"){
                    if (optarg){
                        llbipsw = optarg;
                    }
                }else{
                    reterror("Unknown longopt '%s'",curopt.c_str());
                }
            }
                break;
            case 'b': //long option: "boot-args"
                bootargs = optarg;
                break;

            case 'B': //long option: "baseband"
                basebandPath = optarg;
                break;

            case 'd': // long option: "debug"; can be called as short option
                idevicerestore_debug++;
                break;
                
            case 'e': // long option: "buildid"
                if (strncmp(optarg, "0x", 2) == 0) {
                    ecid = strtoul_l(optarg, NULL, 16, LC_GLOBAL_LOCALE);
                }else{
                    ecid = strtoul_l(optarg, NULL, 10, LC_GLOBAL_LOCALE);
                }
                break;
                
            case 'm': //long option: "sep-manifest"
                SEPManifestPath = optarg;
                break;

            case 'M': //long option: "baseband-manifest"
                basebandManifestPath = optarg;
                break;

            case 'r': //long option: "ramdisk"
                ramdiskPath = optarg;
                break;

            case 's': //long option: "sep"
                SEPPath = optarg;
                break;

            case 't': // long option: "apticket"; can be called as short option
                apticketPaths.push_back(optarg);
                break;
                
            case 'T': // long option: "fresh-apticket"; can be called as short option
                if (!(requestFreshAPTicketVariant = optarg)) requestFreshAPTicketVariant = RESTORE_VARIANT_ERASE_INSTALL;
                break;
                
            case 'u': // long option: "update"; can be called as short option
                isUpdateRestore = true;
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
    }else{
        error("argument parsing failed! agrc=%d optind=%d",argc,optind);
        return -5;
    }
    
    {
        optind = 0;
        optopt = 0;
        opterr = 0;
#ifdef _OPTRESET
        optreset = 1;
#endif

        int ropt = 0;
        int roptindex = 0;
        const struct option *rlongopts = ra1nsn0w::getLongOpts();
        const char *rshortopts = ra1nsn0w::getShortOpts();
        ra1nargs.push_back(NULL);
        
        while ((ropt = getopt_long((int)(ra1nargs.size()-1), (char * const *)ra1nargs.data(), rshortopts, rlongopts, &roptindex)) >= 0) {
            std::string curopt = rlongopts[roptindex].name;
            if (ropt) {
                for (const struct option *clo = rlongopts; clo->name; clo++) {
                    if (clo->val == ropt){
                        curopt = clo->name;
                        break;
                    }
                }
            }
            if (!ra1nsn0w::parseArgument(cfg, curopt, optarg)) {
                std::string orig;
                try {
                    orig = ra1nargs.at(optind-1);
                } catch (...) {
                    //
                }
                error("Unrecognised ra1nsn0w longopt '%s' ('%s')",curopt.c_str(),orig.c_str());
                return -5;
            }
        }
    }
    
    if (doRestorePwnage || doRestore24kPwn){
        optind = 0;
        optopt = 0;
        opterr = 0;
#ifdef _OPTRESET
        optreset = 1;
#endif

        int ropt = 0;
        int roptindex = 0;
        const struct option *rlongopts = ra1nsn0w::getLongOpts();
        const char *rshortopts = ra1nsn0w::getShortOpts();
        ra1nargs_persist.push_back(NULL);
        
        while ((ropt = getopt_long((int)(ra1nargs_persist.size()-1), (char * const *)ra1nargs_persist.data(), rshortopts, rlongopts, &roptindex)) >= 0) {
            std::string curopt = rlongopts[roptindex].name;
            if (ropt) {
                for (const struct option *clo = rlongopts; clo->name; clo++) {
                    if (clo->val == ropt){
                        curopt = clo->name;
                        break;
                    }
                }
            }
            if (!ra1nsn0w::parseArgument(cfg_persist, curopt, optarg)) {
                std::string orig;
                try {
                    orig = ra1nargs_persist.at(optind-1);
                } catch (...) {
                    //
                }
                error("Unrecognised ra1nsn0w-persist longopt '%s' ('%s')",curopt.c_str(),orig.c_str());
                return -5;
            }
        }
    }
    
    futurerestore client(ecid, isUpdateRestore);
    
    if (doRestorePwnage || doRestore24kPwn) {
        client.setRestorePwnage(doRestorePwnage);
        client.setPersistCfg(cfg_persist);
    }
    if (doRestore24kPwn) client.setRestore24kpwn(doRestore24kPwn);    
    if (needsDecryptedRamdisk) client.setNeedsDecryptedRamdisk(needsDecryptedRamdisk);
    if (apticketPaths.size()) client.loadAPTickets(apticketPaths);
    if (requestFreshAPTicketVariant){
        if (llbipsw) {
            client.requestFreshAPTicket(llbipsw, requestFreshAPTicketVariant);
        }else{
            client.requestFreshAPTicket(ipsw, requestFreshAPTicketVariant);
        }
    }
    if (llbipsw) client.setLLBFromIPSW(llbipsw);

    if (ramdiskPath) client.setCustomRamdisk(ramdiskPath);
    
    if (getLatestBaseband){
        client.fetchLatestBaseband();
    } else {
        if (basebandPath) client.setCustomBasebandPath(basebandPath);
        if (basebandManifestPath){
            plist_t p_BuildManifest = NULL;
            cleanup([&]{
                safeFreeCustom(p_BuildManifest, plist_free);
            });
            plist_err_t perr = PLIST_ERR_SUCCESS;
            retassure(!(perr = plist_read_from_file(basebandManifestPath, &p_BuildManifest, NULL)), "Failed to read plist from path '%s' with err=%d",basebandManifestPath,perr);
            client.setCustomBasebandManifest(p_BuildManifest);
        }
    }
    if (!skipBasebandChecks) client.verifyBasebandTicketCanBeRequested();
    
    if (useDefaultSEP) {
        info("Using default SEP from IPSW");
    }else if (getLatestSEP){
        client.fetchLatestSEP();
    } else {
        if (SEPPath) client.setCustomSEPPath(SEPPath);
        if (SEPManifestPath){
            plist_t p_BuildManifest = NULL;
            cleanup([&]{
                safeFreeCustom(p_BuildManifest, plist_free);
            });
            plist_err_t perr = PLIST_ERR_SUCCESS;
            retassure(!(perr = plist_read_from_file(SEPManifestPath, &p_BuildManifest, NULL)), "Failed to read plist from path '%s' with err=%d",SEPManifestPath,perr);
            client.setCustomSEPManifest(p_BuildManifest);
        }
    }

    client.setNoTicketNeeded(dontExpectAPTicket);

    if (!client.isInRestoreMode()) {
        client.enterPwnRestore(ipsw, is32Bit, bootargs, cfg);
    }
    
    client.doRestore(ipsw);
    info("Done: restoring succeeded!");

    return 0;
}
