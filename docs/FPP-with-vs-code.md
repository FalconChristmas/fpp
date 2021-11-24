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
Follow the video shows how to setup the remote-ssh extension in Visual Studio
code that allows the editor to remotely access the Raspberry Pi.  This step
alone allows for web development with FPP.

[![Remote-SSH Setup](https://img.youtube.com/vi/h2MhkFKUOow/0.jpg)](https://www.youtube.com/watch?v=h2MhkFKUOow)

Basic steps:
1) Edit "/etc/ssh/sshd_config" to set PermitRootLogin to yes
2) Set the root password (passwd)
3) Install the Remote-SSH extension into VSC

## Compiling FPPD
The default vscode tasks that we currently have setup include a "clean" target and a "make debug" target.  
If a C/C++ file is open, from the Terminal->Run Task... menu, you should be able to select either the
"make clean" or "Make FPPD (debug)" tasks which will cause vscode to run "make" on the remote host.  In 
addition, the "Make FPPD (debug)" task is setup as the default build task that is run when you hit
"ctrl-shift-B" (or "cmd-shift-B" on Mac).   When run, and C++ errors will appear on the "Problems" tab
and allow click navigation directly to the problem.


## Remote C/C++ Debugging
Follow video shows how to use the VS Code visual debugger along with the
[GDB Debugger - Beyond](https://marketplace.visualstudio.com/items?itemName=coolchyni.beyond-debug) extension
to preform interactive debugging of the *fppd* process on a remote
Raspberry Pi. Note, launch.json is now part of the fppd code base.  There you shouldn't need to manually copy it
any more.

[![Remote C/C++ Debugging](https://img.youtube.com/vi/bix6WzRrbEQ/0.jpg)](https://www.youtube.com/watch?v=bix6WzRrbEQ) 

## Clang Format Extension
It is recomended to install the [Clang Format Extension](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format) to automatic format
the FPP source code based on the included '.clang-format' file. For this extension to work, clang-format must
be installed on the local machine or the remote machine (i.e PI or BBB) if using the Remote-SSH extension.

To install clang-format on a Debian Based system(FPP, Ubuntu, etc.), run the following command from terminal.
```
sudo apt install clang-format
```
