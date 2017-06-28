# futurerestore
_futurerestore is a hacked up idevicerestore wrapper, which allows manually specifying SEP and Baseband for restoring_

Latest compiled version can be found here:  
(MacOS & Linux)  
http://api.tihmstar.net/builds/futurerestore/futurerestore-latest.zip

---

# Features  
* Supports the following downgrade methods
  * Prometheus 64bit devices (generator and nonce collision mode)
  * Odysseus for 32bit devices
  * Re-restoring 32bit devices to iOS 9 with @alitek123's no-nonce method
* Allows restoring any nonmatching signed iOS/Sep/Baseband

# Dependencies
*  ## Bundled Libs
  Those don't need to be installed manually
  * jsmn
* ## External Libs
  Make sure these are installed
  * libzip
  * libcurl
  * openssl (or CommonCrypto on OSX)
  * [libplist](https://github.com/libimobiledevice/libplist)
* ## Submodules
  Make sure these projects compile on your system (install it's dependencies)
  * [tsschecker](https://github.com/tihmstar/tsschecker)
  * [img4tool](https://github.com/tihmstar/img4tool)
  * [idevicerestore](https://github.com/tihmstar/idevicerestore)

# Help  
_(might become outdated):_

Usage: `futurerestore [OPTIONS] IPSW`


| option (short) | option (long)             | description                                                                       |
|----------------|---------------------------|-----------------------------------------------------------------------------------|
|  -t | --apticket | PATH		Apticket used for restoring |
|  -b | --baseband | PATH		Baseband to be flashed |
|  -p | --baseband-manifest | PATH	Buildmanifest for requesting baseband ticket |
|  -s | --sep PATH |		Sep to be flashed |
|  -m | --sep-manifest PATH |	Buildmanifest for requesting sep ticket |
|  -w | --wait		 |	keep rebooting until nonce matches APTicket |
|  -u | --update		 |	update instead of erase install |
|  -d | --debug		 |	show all code, use to save a log for debug testing |
|     |--latest-sep	 |	use latest signed sep instead of manually specifying  one(may cause bad restore) |
|     | --latest-baseband |		use latest signed baseband instead of manually  specifying one(may cause bad restore) |
|     | --no-baseband	 |	skip checks and don't flash baseband. WARNING: only use this for device without baseband (eg iPod or some wifi only iPads) |

---

## 0) What futurerestore can do
**Downgrade/Upgrade/Re-restore same iOS.**
Whenever you read "downgrade" nowadays it means you can also upgrade and re-restore if you're on the same iOS. Basically this allows restoring an iOS and the installed iOS doesn't matter.

---

## 1) Prometheus (64bit device) - generator method

### Requirements
- Jailbreak
- SHSH2 files with a generator
- nonceEnabler patch enabled

### Info
You can downgrade if the destination iOS is compatible with the latest signed SEP and if you have shsh2 files with a generator for that iOS.

### How to use
1. Device must be jailbroken and nonceEnabler patch must be active
2. Open shsh file and look up the generator
  * Looks like this: `<key>generator</key><string>0xde3318d224cf14a1</string>`
3. Write the generator to device's NVRAM
  * SSH into the device and run `nvram com.apple.System.boot-nonce=0xde3318d224cf14a1` to set the generator *0xde3318d224cf14a1*
  * verify with `nvram -p`
4. Connect your device in normal mode to computer
5. On the computer run `futurerestore -t ticket.shsh --latest-baseband --latest-sep ios.ipsw`

### Youtube
<a href="http://www.youtube.com/watch?feature=player_embedded&v=BIMx2Y13Ukc" target="_blank"><img src="http://img.youtube.com/vi/BIMx2Y13Ukc/0.jpg" alt="Prometheus" width="240" height="180"/></a>
*Prometheus*

<a href="http://www.youtube.com/watch?feature=player_embedded&v=UXxpUH71-s4" target="_blank"><img src="http://img.youtube.com/vi/UXxpUH71-s4/0.jpg" alt="Prometheus" width="240" height="180"/></a>
*NonceEnabler*

### Recommended method to active nonceEnabler patch
1. Get nvpatch https://github.com/Siguza/ios-kern-utils/releases/
2. Run on the device `nvpatch com.apple.System.boot-nonce`

### Activate tfp0 if jailbreak doesn't allow it
#### Method 1 (if jailbroken on 9.3.x)
  * reboot
  * reactivate jailbreak with https://jbme.qwertyoruiop.com/
  * done

#### Method 2
  * Use cl0ver (https://github.com/Siguza/cl0ver)

---

## 2) Prometheus (64bit device) - nonce collision method

### Requirements
- iPhone5s or iPad Air on iOS 9.1 - 10.2
- No Jailbreak required
- SHSH files with customly chosen APNonce
- The shsh file needs to have one of the nonces, which the device generates a lot

### Info
You can downgrade if the destination iOS is compatible with the latest signed SEP. You also need to have special shsh files. If you don't know what this is, you probably can **NOT** use this method!

### How to use
1. Connect your device in normal mode or recovery mode
2. On the computer run `futurerestore -w -t ticket.shsh --latest-baseband --latest-sep ios.ipsw`
* If you have saved multiple tickets with different nonces you can specify more than
one to speed up the process: `futurerestore -w -t t1.shsh -t t2.shsh -t t3.shsh -t t4.shsh --latest-baseband --latest-sep ios.ipsw`

---

## 3) Odysseus (32bit devices)

### Requirements
- futurerestore compiled with libipatcher (odysseus support)
- Jailbreak or bootrom exploit (limera1n)
- Firmware keys for the device/destination iOS must be public (check ipsw.me)
- SHSH files for the destination iOS (OTA blobs work too!)

### Info
If you have a jailbroken 32bit device you can downgrade to any iOS you have blobs for. You can still get OTA blobs for iOS 6.1.3 and 8.4.1 for some devices and use those.

### How to use
1. Get device into kDFU/pwnDFU
  * Pre-iPhone4s (limera1n devices):
    * Enter pwndfu mode with redsn0w or any other tool
  * iPhone4s and later:
    * Jailbreak required!
    * Enter kDFU mode with kDFU app (cydia: repo.tihmstar.net) or by loading a pwniBSS from any existing odysseus bundle.
2. Connect your device to computer in kDFU mode (or pwnDFU mode)
3. On the computer run `futurerestore --use-pwndfu -t ticket.shsh --latest-baseband ios.ipsw`

### Youtube
<a href="http://www.youtube.com/watch?feature=player_embedded&v=FQfcybsEWmM" target="_blank"><img src="http://img.youtube.com/vi/FQfcybsEWmM/0.jpg" alt="Odysseus" width="240" height="180"/></a>
*Futurerestore + Libipatcher*

<a href="http://www.youtube.com/watch?feature=player_embedded&v=8Ro4g6StPeI" target="_blank"><img src="http://img.youtube.com/vi/8Ro4g6StPeI/0.jpg" alt="Odysseus" width="240" height="180"/></a>
*kDFU App*

<a href="http://www.youtube.com/watch?feature=player_embedded&v=Wo7mGdMcjxw" target="_blank"><img src="http://img.youtube.com/vi/Wo7mGdMcjxw/0.jpg" alt="Odysseus" width="240" height="180"/></a>
*Enter kDFU Mode (watch up to the point where the screen goes black)*  

*You can use **any** odysseus bundle for this*

## 4) iOS 9 Re-restore bug (found by @alitek123) (32bit devices):
### Requirements
- No Jailbreak required
- SHSH files without a nonce (noNonce APTickets)

### Info
If you have shsh files for iOS9 which do not contain a nonce, you can restore to that firmware.

### How to use
1. Connect your device in DFU mode
2. On the computer run `futurerestore -t ticket.shsh --latest-baseband ios9.ipsw`
