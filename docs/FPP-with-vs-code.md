# Remote FPP Debugging with VS Code
This document outlines how to setup Visual Studio Code on your laptop
to enable remote development of fpp on a Raspberry Pi3+.

_While technically this works on the BBB, this process is not recommend on the BBB due to limited resources (memory, CPU)
on the device. The extra processes tend to cause the device to crash._    If using it on a BBB, it might be advised to add
a swapfile:
```
sudo fallocate -l 512M /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
sudo sysctl vm.swappiness=10
```

## Setup Remote-SSH
Follow the video shows how to setup the remote-ssh plugin in Visual Studio
code that allows the editor to remotely access the Raspberry Pi.  This step
alone allows for web development with FPP.

[![Remote-SSH Setup](https://img.youtube.com/vi/h2MhkFKUOow/0.jpg)](https://www.youtube.com/watch?v=h2MhkFKUOow)

## Remote C/C++ Debugging 
Follow video shows how to use the VS Code visual debugger along with the
[GDB Debugger - Beyond](https://marketplace.visualstudio.com/items?itemName=coolchyni.beyond-debug) extension
to preform interactive debugging of the *fppd* process on a remote
Raspberry Pi. Note, launch.json is now part of the fppd code base.  There you shouldn't need to manually copy it
any more. 

[![Remote C/C++ Debugging](https://img.youtube.com/vi/bix6WzRrbEQ/0.jpg)](https://www.youtube.com/watch?v=bix6WzRrbEQ) 

launch.json
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "type": "by-gdb",
            "request": "launch",
            "name": "fppd(gdb)",
            "program": "/opt/fpp/src/fppd",
            "cwd": "/opt/fpp/src"
        },
        {
            "type": "by-gdb",
            "request": "launch",
            "name": "fppcapedetect(gdb)",
            "program": "/opt/fpp/src/fppcapedetect",
            "cwd": "/opt/fpp/src"
        }
    ]
}
```
