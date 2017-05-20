# futurerestore  
_futurerestore is a hacked up idevicerestore wrapper, which allows manually specifying SEP and Baseband for restoring_

Latest compiled version can be found here:  
(MacOS & Linux)  
http://api.tihmstar.net/builds/futurerestore/futurerestore-latest.zip

# Features  
* Used for prometheus downgrade method
* Allows restoring any nonmatching signed iOS/Sep/Baseband
* Supports downgrading 32bit devices to iOS 9 with @alitek123's no-nonce method

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
|     |--latest-sep	 |	use latest signed sep instead of manually specifying  one(may cause bad restore) |
|     | --latest-baseband |		se latest signed baseband instead of manually  specifying one(may cause bad restore) |
|     | --no-baseband	 |	skip checks and don't flash baseband. WARNING: only use this for device without baseband (eg iPod or some wifi only iPads) |
