# CTRsync

## Why?
I'm tired of launching sftpd and going to my desktop to download or upload common files. A few people asked for a way to automatically transfer their backed-up saves from JKSM. I like secure communications.

## Features
* Automatically log in using an SSH keypair.
* Set up host & local folders so synchronizing is a breeze.
* On-screen instructions so you aren't totally lost.

## Building
CTRsync requires libssh2 and a working build of mbedtls, which are currently only available in [my fork](https://github.com/smartperson/3ds_portlibs/tree/libssh2) of 3ds_portlibs. I'm going to get a PR ready for that, but until then you'll have to get that fork and `make mbedtls` `make libssh2` and `make install`.

## Installation
* If compiling, place `CTRsync.3dsx` and a valid `config.cfg` into `/3ds/CTRsync`.
* If using pre-compiled, merge the `3ds` folder into your SD card root. Make sure to add a valid `config.cfg`.

## Warnings
* If you configure it as such, you could overwrite your `/Nintendo 3DS`, `/luma`, and `/3ds` directories and really mess things up.
* Create a separate public/private key pair for your 3DS.
* I'm not responsible for files getting messed up. This may or may not go without saying.

## Known Issues
* Push from 3DS to host runs surprisingly slowly at ~700KB/s. I don't know why and would love to hear any ideas.
* Edge cases are not handled gracefully: no network connection, host unreachable, etc.

## Acknowledgements
* nedwill, WinterMute, and others on #3dsdev for advice on improving SD performance.
* smea, fincs, and others on #3dsdev for help with understanding threading in libctru better.
* Drew Hess for the effective ring buffer implementation in C.
