# Scriptease
A usable (yet inchoate) TAS/Automation toolkit for the Nintendo Switch with homebrew, compatible with latest firmware. The sys-module is based on sys-botbase, but it extends sys-botbase's functionality for TAS purposes.

## Features:
### Remote Control:
- Set controller state
- Simulate buttons press, hold, and release
- Simulate touch screen drawing

### Memory Reading and Writing:
- Read/write x amount bytes of consecutive memory from RAM based on:
    1. Absolute memory address
    2. Address relative to main nso base
    3. Address relative to heap base

### Screen Capture:
- Capture current screen and return as JPG

## Disclaimer:
This project was created for the purpose of development for bot automation. The creators and maintainers of this project are not liable for any damages caused or bans received. Use at your own risk.

## Installation
TODO
<!-- Download [latest release](https://github.com/olliz0r/sys-botbase/releases/latest) and extract into your Nintendo Switch SD card. Restart your switch. 

When installed correctly, sys-botbase will make your docked joy-con's home button glow on switch bootup. If this does not happen, sys-botbase is not installed correctly.

![](joycon-glow.gif) -->

## Credits
- Big thank you to [jakibaki](https://github.com/jakibaki/sys-netcheat) for a great sysmodule base to learn and work with, as well as being helpful on the Reswitched discord!
- Thanks to RTNX on discord for bringing to my attention a nasty little bug that would very randomly cause RAM poking to go bad and the switch (sometimes) crashing as a result.
- Thanks to Anubis for stress testing!
