# futurerestore
[![CI Building](https://img.shields.io/github/workflow/status/marijuanARM/futurerestore/CI)](https://github.com/marijuanARM/futurerestore/actions?workflow=CI)

__Make sure to read this page before continuing.__

Latest **stable** compiled version can be found [here](https://github.com/marijuanARM/futurerestore/releases).

For A12-A14, and WiFi-only iPad restores - use the latest beta build for your platform [here](https://nightly.link/m1stadev/futurerestore/workflows/ci/test).

**There are currently no pre-compiled beta builds for Windows.**

__Only use if you are sure what you're doing.__

---
## What is FutureRestore?
FutureRestore is a modified idevicerestore wrapper, which allows manually specifying SEP and Baseband for restoring. This allows unsigned firmwares to be restored onto devices, providing you have a backup of the APTicket (SHSH Blobs), and can recreate all the specific conditions of the APTicket e.g. ECID, APNonce, Board ID.

# Features  
* Supports the following downgrade methods:
  * Prometheus for 64-bit devices:
    - Prometheus via APNonce recreation with the APNonce generator
    - Prometheus via APNonce collision
  * Odysseus for 32-bit & 64-bit (A7-A11) devices
  * Re-restoring 32-bit devices to iOS 9.x with [alitek123](https://github.com/alitek12)'s no-ApNonce method (alternative — [idevicererestore](https://downgrade.party)).

# Dependencies
* ## External libs
  Make sure these are installed
  * [curl](https://github.com/curl/curl) (Linux/Windows only, macOS already has curl preinstalled);
  * [openssl 1.1.1](https://github.com/openssl/openssl) (or CommonCrypto on macOS);
  * [libusb 1.0.24](https://github.com/libusb/libusb) (Linux/Windows only, macOS can use IOKit for libirecovery);
  * [libzip](https://github.com/nih-at/libzip);
  * [libplist](https://github.com/libimobiledevice/libplist);
  * [libusbmuxd](https://github.com/libimobiledevice/libusbmuxd);
  * [libirecovery](https://github.com/libimobiledevice/libirecovery);
  * [libimobiledevice](https://github.com/libimobiledevice/libimobiledevice);
  * [libpng16](https://github.com/glennrp/libpng);
  * [xpwn(fork)](https://github.com/nyuszika7h/xpwn);
  * [libgeneral](https://github.com/tihmstar/libgeneral);
  * [libfragmentzip](https://github.com/tihmstar/libfragmentzip);
  * [libinsn](https://github.com/tihmstar/libinsn);
  * [lzfse](https://github.com/lzfse/lzfse);
  * [img4tool](https://github.com/tihmstar/img4tool);
  * [liboffsetfinder64(fork))](https://github.com/Cryptiiiic/liboffsetfinder64);
  * [libipatcher(fork)](https://github.com/Cryptiiiic/libipatcher)

* ## Submodules
  Make sure these projects compile on your system (install it's dependencies):

  * [jssy](https://github.com/tihmstar/jssy);
  * [tsschecker(fork)](https://github.com/1Conan/tsschecker);
  * [idevicerestore(fork)](https://github.com/m1stadev/idevicerestore)

  If you are cloning this repository you may run:

  ```git clone https://github.com/m1stadev/futurerestore --recursive```
 
  which will clone these submodules for you.


Usage: `futurerestore [OPTIONS] iPSW`

| option (short) | option (long)                                      | description                                                                       |
|----------------|------------------------------------------|-----------------------------------------------------------------------------------|
|  ` -t `           | ` --apticket PATH	 `                    | Signing tickets used for restoring, commonly known as blobs |
|  ` -u `           | ` --update `                                    | Update instead of erase install (requires appropriate APTicket) |
|                       |                                                           | This parameter is recommended to not be used for downgrading. If you are jailbroken, make sure to have your orig-fs snapshot restored (Restore RootFS).  |
|  ` -w `           | ` --wait `                                        | Keep rebooting until ApNonce matches APTicket (ApNonce collision, unreliable) |
|  ` -d `           | ` --debug `                                      | Show all code, use to save a log for debug testing |
|  ` -e `           | ` --exit-recovery `                       | Exit recovery mode and quit |
|                       | ` --use-pwndfu `                           | Restoring devices with Odysseus method. Device needs to be in pwned DFU mode already |
|                       | ` --no-ibss `                           | Restoring devices with Odysseus method. For checkm8/iPwnder32 specifically, bootrom needs to be patched already with unless iPwnder. |
|                       | ` --rdsk PATH `                           | Set custom restore ramdisk for entering restoremode(requires use-pwndfu) |
|                       | ` --rkrn PATH `                           | Set custom restore kernelcache for entering restoremode(requires use-pwndfu) |
|                       | ` --set-nonce `                           | Set custom nonce from your blob then exit recovery(requires use-pwndfu) |
|                       | ` --set-nonce=0xNONCE `                           | Set custom nonce then exit recovery(requires use-pwndfu) |
|                       | ` --serial `                           | Enable serial during boot(requires serial cable and use-pwndfu) |
|                       | ` --boot-args "BOOTARGS" `                           | Set custom restore boot-args(PROCEED WITH CAUTION)(requires use-pwndfu) |
|                       | ` --no-cache `                           | Disable cached patched iBSS/iBEC(requires use-pwndfu) |
|                       | ` --skip-blob `                           | Skip SHSH blob validation(PROCEED WITH CAUTION)(requires use-pwndfu) |
|                       | ` --latest-sep `                             | Use latest signed SEP instead of manually specifying one |
|  ` -s `           | ` --sep PATH `                                 | Manually specify SEP to be flashed |
|  ` -m `           | ` --sep-manifest PATH `              | BuildManifest for requesting SEP ticket |
|                       | ` --latest-baseband `                | Use latest signed baseband instead of manually specifying one |
|  ` -b `           | ` --baseband PATH	`                     | Manually specify baseband to be flashed |
|  ` -p `           | ` --baseband-manifest PATH `  | BuildManifest for requesting baseband ticket |
|                       | ` --no-baseband `                         | Skip checks and don't flash baseband |
|                       |                                                           | Only use this for device without a baseband (eg. iPod touch or Wi-Fi only iPads) |

---

# 1) Prometheus (64-bit device) - APNonce recreation with generator method

### You can only downgrade if:
*  the destination firmware version is compatible with a currently signed SEP and baseband. Check whether your version is compatible [here.](#firmware-signing-info)
*  if you have a signing tickets files with a generator for **that specific firmware version.**


### Requirements
* A jailbreak or an exploit that allows nonce setting.
* Signing ticket files (`.shsh`, `.shsh2`, `.plist`) with a generator
  * A12+ users must also have a valid APNonce / generator pair due to nonce entanglement. Only having an APNonce without a generator is not sufficient.
* A computer with a minimum of 8 gigabytes of free space + IPSW of the target version downloaded. You can find the IPSW for your device at [IPSW.me](https://ipsw.me).
* On Windows machines, make sure to have [this version](https://www.apple.com/itunes/download/win64) of iTunes installed. Using the Microsoft Store version will cause issues.
### Method:
1. Jailbreak your device if it isn't jailbroken already.
2. Open your blob in any text editor and search for the word "generator". In most text editors you can use CTRL + F / CMD + F to look for it.

![GeneratorExample](https://user-images.githubusercontent.com/48022799/117004373-aa0b6700-acee-11eb-8a70-c488163e349b.jpeg) 
 - This should be a `0x` followed by 16 characters, which will be a combination of letters and numbers.
3. Note that value down. This is your generator.
   
   - **NOTE:** If there is no generator value, try to remember which jailbreak you were using at the time of saving blobs. If you were using unc0ver, your generator is most likely `0x1111111111111111`, and if you were using Chimera/Odyssey/Taurine, your generator is most likely `0xbd34a880be0b53f3`.

4. Set your device's APNonce generator. You can use your [jailbreak tool](#using-jailbreak-tools) to set your generator in its native settings. However, setting your generator with [dimentio](#Using-dimentio) is recommended.

5. Connect your device in normal mode to computer - make sure the trust dialog is accepted.
6. **Recommended:** Make a full backup of your device before running futurerestore.
7. On the computer run:
  
   ```futurerestore -t blob.shsh2 --latest-sep --latest-baseband -d target.ipsw```

   If you are upgrading and want to preserve user data you may run:
   
   ```futurerestore -u -t blob.shsh2 --latest-sep --latest-baseband -d target.ipsw```


## Using dimentio

To set generator with dimentio:
  1. Open your package manager on your jailbroken iDevice
1. Add [https://repo.1conan.com](https://repo.1conan.com) to your sources.
1. Add [https://repo.chariz.com](https://repo.chariz.com) to your sources.
2. Download and install dimentio
3. Download and install NewTerm2
4. If you're on iOS 14.0 or above:
    - Install `libkernrw` if you're using Taurine
    - Install `libkrw` if you're using unc0ver
    - checkra1n/odysseyra1n users don't need to install anything extra

5. Open NewTerm 2 on your iDevice and type the following command:
   
    ```su root -c 'dimentio [generator]'```
    - `[generator]` should be the APNonce generator you just grabbed.

   Example: `su root -c 'dimentio 0x1111111111111111'`
    
6. When asked for a password, enter your root password
    - By default, this is set to `alpine`
7. Near the end of the text, you should see the line `Set nonce to [generator].` This indicates that your generator has been set successfully.



## Using jailbreak tools:

Use jailbreak tools for setting boot-nonce generator:
1. [Meridian](https://meridian.sparkes.zone) for iOS 10.x;
2. [backr00m](https://nito.tv) or greeng0blin for tvOS 10.2-11.1;
3. [Electra and ElectraTV](https://coolstar.org/electra) for iOS and tvOS 11.x;
4. [Chimera and ChimeraTV](https://chimera.sh) for iOS 12.0-12.5.4 (Nonce setter only supports on 12.1.2 - 12.4.1 on A12, and 12.1.3 - 12.5.4 is only supported on A7 - A11 devices.)
5. [Odyssey](https://theodyssey.dev/) for iOS 13.0-13.7
   - Note that there are some reported issues with Odyssey's generator setter. Using it is not recommended.
6. [Taurine](https://taurine.app/) for iOS 14.0-14.3
7. [unc0ver](https://unc0ver.dev) for iOS 11.0-14.3

## Firmware Signing Info

Currently you can restore to the following versions with the latest SEP and baseband for your device:

Devices that only support up to iOS 12 (most A7 and A8 devices excluding iPad5,1 - iPad5,4): 11.3-12.5.4

A9 and A10: 14.0-14.7

A11 devices: 14.3-14.7

A12 devices and newer: 14.0-14.7

---

# Common Issues

## SEP Firmware is not being signed

This problem occurs when the user tries to manually specify SEP from the *target* version, instead of from the *latest* available version. To fix this problem, either choose the `latest-sep` argument or manually specify a SEP from the latest available iOS version.

## Could not connect to device in recovery mode / Failed to place device in recovery mode

**NOTE:** if the error is similarly named, follow these steps too.

If your device is in recovery mode:
- Run FutureRestore again while your device is in recovery mode.

If your device is not in recovery mode:
- Enter recovery mode manually, then run FutureRestore again.

## Device ApNonce doesn't match APTicket nonce

This error means that you have not set your generator on your device to that of the blob. In order to solve this problem, you must set your generator with [dimentio](#using-dimentio) or any [jailbreak tool](#using-jailbreak-tools). 
- If after following the steps you still cannot resolve this issue, your generator may not correspond to its respective APNonce. 
- If you saved blobs while unjailbroken on A12+ without [getnonce](https://github.com/nyuszika7h/getnonce) or [blobsaver v3](https://github.com/airsquared/blobsaver/releases/tag/v3.0.1), your APNonce/generator pair is invalid. This cannot be resolved.

## Unable to send iBEC (error -8)
1.  Leave the device plugged in, it'll stay on the Recovery screen;
2.  Head over to Device Manager under Control Panel in Windows;
3.  Locate "Apple Recovery (iBoot) USB Composite Device" (at the bottom);
4.  Right click and choose "Uninstall device".
    You may see a tick box that allows you to uninstall the driver software as well, tick that (all the three Apple mobile device entries under USB devices will disappear);
5.  Unplug the device and re-plug it in;
6.  Go back to futurerestore and send the restore command again (just press the up arrow to get it back, then enter).
7. Error `-8` should now be fixed.

## Error: Unable to receive message from FDR...

The fix for this is either waiting (it can take a very long time) or just re-trying the process. 
This is an error that has been diagnosed but no fix for it is available as of the time of writing this.

---

# 2) Prometheus (64-bit device) - ApNonce collision method (Recovery mode)
### Requirements
- **Device with A7 chip on iOS 9.1 - 10.2 or iOS 10.3 beta 1**;
- Jailbreak isn't required;
- Signing ticket files (`.shsh`, `.shsh2`, `.plist`) with a custom ApNonce;
- Signing ticket files needs to have one of the APNonces which the device generates a lot;

### Info
You can downgrade if the destination firmware version, if it is compatible with the [latest sep and baseband!](#firmware-signing-info). You also need to have **special signing ticket files**. If you don't know what this is, you probably can **NOT** use this method!

### How to use
1. Connect your device in normal or recovery mode;
2. On the computer run `futurerestore -w -t ticket.shsh --latest-baseband --latest-sep firmware.ipsw`
* If you have saved multiple signing tickets with different nonces you can specify more than
one to speed up the process: `futurerestore -w -t t1.shsh -t t2.shsh -t t3.shsh -t t4.shsh --latest-baseband --latest-sep firmware.ipsw`



---

# 3) Prometheus (64-bit device) - ApNonce collision method (DFU mode)
### Requirements
- A device with an A7 SoC:
  *  (iPhone 5s, iPad Air, iPad mini 2), A8 (iPhone 6 [+], iPad mini [2,3,4], iPod touch [6th generation]) and A8X (iPad Air 2) chips on all firmwares
- __Devices that have been released after ~ September, 2015 {PROBABLY};__
- Jailbreak isn't required;
- Signing ticket files (`.shsh`, `.shsh2`, `.plist`) with a customly chosen APNonce;
- Signing ticket files needs to have one of the ApNonces, which the device generates a lot;
- __[img4tool](https://github.com/tihmstar/img4tool) can't be used for Windows [problem with signing iBSS/iBEC], now it's TO-DO;__

### Info
You can downgrade if the destination firmware version, if it is compatible with the [latest SEP and baseband](#firmware-signing-info). You also need to have **special signing ticket files**. If you don't know what this is, you probably can **NOT** use this method!

### How to use
1. Connect your device in DFU mode;
2. Use [irecovery](https://github.com/libimobiledevice/libirecovery) for checking ApNonce, which booted in DFU;
3. Extract iBSS/iBEC from target firmware for downgrade (unsigned);
4. Check DFU-collisioned ApNonces with [irecovery](https://github.com/libimobiledevice/libirecovery), which booted in DFU.
    You can't automatically collision DFU ApNonces.
    
    __If ApNonce is not collisioned, "use hands" for DFU booting.__
    
    __If ApNonce is successfully collisioned, use this SHSH2 to sign iBSS/iBEC.__
5. Use img4tool for sign iBSS:
   `img4tool -s ticket.shsh -c iBSS.signed -p <original_iBSS>`;
6. Use img4tool for sign iBEC:
   `img4tool -s ticket.shsh -c iBEC.signed -p <original_iBEC>`;
7. So, after signing we can boot into Recovery with irecovery.

   `irecovery -f iBSS.signed` - loading iBSS;
   
   `irecovery -f iBEC.signed` - loading iBEC;
8. So good! On the computer run `futurerestore -t ticket.shsh --latest-baseband --latest-sep -w firmware.ipsw`.

---

## 4) Odysseus (32-bit / 64-bit devices)
### Requirements
- futurerestore compiled with libipatcher;
- Jailbreak or bootrom exploit (limera1n, checkm8);
- **32-bit**: firmware keys for the device/destination firmware version must be public (check [ipsw.me](https://ipsw.me))

- **64-bit**: Signing ticket files (`.shsh`, `.shsh2`, `.plist`) for the destination firmware (OTA blobs work too!).

### Info
If you have a jailbroken device, you can downgrade to **any** firmware version you have blobs for, as long as the [baseband](#firmware-signing-info) is compatible, SEP does not have to be compatible.

You can still get OTA blobs for iOS 6.1.3, 8.4.1 or 10.3.3 for some devices and use those.

### How to use
1. Get device into kDFU/pwnDFU
  * Pre-iPhone4s (limera1n devices):
    * Enter to pwnDFU mode with redsn0w or any other tool
  * iPhone 4s and later 32-bit devices:
    * Enter to kDFU mode with kDFU app (cydia: repo.tihmstar.net) or by loading a pwnediBSS from any existing odysseus bundle
  * Any 64-bit device:
    * Enter to pwnDFU mode and patch signature check with special fork of [ipwndfu](https://github.com/LinusHenze/ipwndfu_public)
2. Connect your device to computer in kDFU mode (or pwnDFU mode)
3. On the computer run `futurerestore --use-pwndfu -t ticket.shsh --latest-baseband -d firmware.ipsw`
- You can use **any** odysseus bundle for this.

## 5) iOS 9.x re-restore bug by @alitek123 (only for 32-bit devices)
### Requirements
- Jailbreak isn't required;
- Signing ticket files (`.shsh`, `.shsh2`, `.plist`) from by iOS 9.x without ApNonce (noNonce APTickets)

### Info
If you have **signing ticket files for iOS 9.x**, which **do not contain a ApNonce**, you can restore to that firmware.

### How to use
1. Connect your device in DFU mode
2. On the computer run `futurerestore -t ticket.shsh --latest-baseband ios9.ipsw`

---

## Report an issue
Before you report an issue, please check that it is not mentioned in the [Common Issues section](#common-issues).
If it is not, you can report your issue [here](https://github.com/m1stadev/futurerestore/issues).
