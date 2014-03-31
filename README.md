# Wsync 1.0

Wsync is a files synchronization program for the Wired 2.0 protocol. It provides automatic files synchronization from several remote Wired server directories to your local computer. It is written in ANSI-C and portable to many UNIX-like operating systems. It has the ability to run as a one-shot command-line tool, or as a backround daemon.

This read me file is essentially focused on OSX and Debian-like installation instructions. Wsync is still young and has been quickly tested on these two operating systems.

### Dependencies

#### OSX

* Install Xcode and/or Xcode command-line tools: [https://developer.apple.com/xcode/](https://developer.apple.com/xcode/)

#### Linux

* 	Install minimal development tools (including GCC, autotools, etc): 

		apt-get install build-essential

* 	Install dev library for **Sqlite3**: 
	
		apt-get install libsqlite3-dev

* 	Install dev library for **OpenSSL**: 

		apt-get install libssl-dev

* 	Install dev library for **libxml2**: 

		apt-get install libxml2-dev

* 	Install **git** tool: 

		apt-get install git

### Installation

* 	Download sources from GitHub.com: 

		git clone https://github.com/Read-Write/wsync.git

* 	Move to cloned directory: 

		cd wsync

* 	Configure sources:

		./configure

* 	Compile: 

		make

* 	Install: 

		sudo make install

Wsync is installed in `/usr/local/bin` directory.

### How to use it

Wsync can be used both as a simple command-line program and as a background daemon program.

#### Command-line

Command-line is the default running mode of wsync. All you have to do is to give it a remote input directory URL (`-i`) and a local output directory path (`-o`) as arguments and wsync will download the content of the remote directory to the local directory. 

You can also define an interval `-I` in seconds that indicates to wsync at what frequency it needs to synchronize remote files with local files. If no interval is defined, wsync will exit just after the download operation.

The `-R` argument indicates that files will be synchronized recursively through remote directories. Without it, wsync will only synchronize the first level of the remote URL directory.

**Examples:**

	wsync -i wiredp7://admin:*****@example.org/repo/wsync -o /home/joe/wsync -R
	
	wsync -i wiredp7://admin:*****@example.org/repo/wsync -o /home/joe/wsync -R -I 3600
	

See below for more informations about wsync commands:

	Usage: wsync [-Dllhtuv] [-i url] [-o path] [-I interval]
	              [-f file] [-n lines] [-L file] [-s facility]
	Options:
	    -i url         wired URL of the remote directory to sync
	    -o path        local path of the destination directory
	    -I interval    time interval between two synchronization
	    -R             synchronize files recursively
	    -D             daemonize
	    -f file        set the config file to load
	    -h             display this message
	    -n lines       set limit on number of lines for -L
	    -L file        set alternate file for log output
	    -l             increase log level (can be used twice)
	    -s facility    set the syslog(3) facility
	    -t             run syntax test on config
	    -u             do not chroot(2) to root path
	    -v             display version information
	    
When running in the a terminal window, send an interruption signal (SIGINT) with the `^ + C` shortcut to terminate the program.

#### Daemon tool

To run wsync as a daemon, you have first to edit the `~/.wsync/wsync.conf` file. In this file you can define several sync commands that wsync will read in order to automatically synchronize your files in the background.

**Examples:**

	sync = -i wiredp7://admin:*****@example.org/repo/wsync -o /home/joe/wsync -I 3600 -R

	sync = -i wiredp7://admin:*****@example.org/repo/wired -o /home/joe/wired -I 86400 -R
	
	sync = -i "wiredp7://admin:*****@example.org/private/My Music" -o "/home/joe/Music/My Music" -I 86400 -R
	
*NB: Wsync supports usage of simple and double quotes to handle whitespaces in both URL and path (this also applies to the command-line mode).*
	
You can also define running user and group in the `wsync.conf` file, in order to start the program as root (ex: at boot) and let it switch to the user of your choice automatically using `setuid` (experimental).

Once your rules are defined in the conf file, you have to run wsync with the `-D` argument:

	/usr/local/bin/wsync -D
	
*NB: Ecxecutable fullpath is required on Linux.*
	
Wsync will create a PID file `~/.wsync/wsync.pid` which could help you to stop wsync running in daemon mode:

	kill `cat ~/.wsync/wsync.pid`
	
This will shutdown the daemon and delete the PID file.

### Features

* One-way files synchronization from Wired Server 2.0 to a local directory
* Run as daemon program in the background
* Handle each sync operation in a separated thread
* Low CPU consumption and small memory footprint

### Planned

* Fine thread pool management with queue and limits
* Reload config without stoping the program using SIGHUP (when running as daemon)
* Two-ways files synchronization: new local files are pushed to the server 
* Catch local FS events to synchonize the local folder with the server in real-time
* A cross-platform GUI program to handle wsync and wsync.conf
* Push some code to libwired in order to integrate wsync in servers and clients

### Author

* Rafaël Warnault - dev@read-write.fr

### License

	 Copyright (c) 2011-2014 Rafaël Warnault
	 
	 All rights reserved.
	 Redistribution and use in source and binary forms, with or without
	 modification, are permitted provided that the following conditions
	 are met:
	 1. Redistributions of source code must retain the above copyright
	    notice, this list of conditions and the following disclaimer.
	 2. Redistributions in binary form must reproduce the above copyright
	    notice, this list of conditions and the following disclaimer in the
	    documentation and/or other materials provided with the distribution.
	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
	STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.


