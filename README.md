# WSync 1.0

Wsync is a files synchronization program for the Wired 2.0 protocol. It provides automatic files synchronization from several remote Wired directories to your local computer. It is written in ANSI-C and portable to many UNIX-like operating systems. It has the ability to run as a one-shot command-line tool, or as a backround daemon.

This read me file is essentially focused on the Debian-like installation procedure.

### Dependencies

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

* 	Download archive: 

		wget http://switch.dl.sourceforge.net/project/wired2/wsync/wsync.tar.gz

* 	Untar binary: 

		tar -zxvf wsync.tar.gz

* 	Move to unarchived directory: 

		cd wsync

* 	Configure sources:

		./configure

* 	Compile: 

		make

* 	Install: 

		sudo make install

Wsync will be install in `/usr/local/bin` directory.

### How to use it

Wsync can be used both as a simple command-line program and as a background daemon program.

#### Command-line

Command-line is the default running mode of wsync. All you have to do is to give it a remote input directory URL (`-i`) and a local output directory path (`-o`) as arguments and wsync will download the content of the remote directory to the local directory. 

You can also define an interval `-I` in seconds that indicates to wsync at what frequency it needs to synchronize remote files with local files.

**Examples:**

	wsync -i wiredp7://admin:*****@example.org/repo/wsync -o /home/joe/wsync
	
	wsync -i wiredp7://admin:*****@example.org/repo/wsync -o /home/joe/wsync -I 3600
	

See below for more information about wsync commands:

	Usage: wsync [-Dllhtuv] [-i url] [-o path] [-I interval]
	              [-f file] [-n lines] [-L file] [-s facility]
	Options:
	    -i url         wired URL of the remote directory to sync
	    -o path        local path of the destination directory
	    -I interval    time interval between two synchronization
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
	    
When running in the a terminal window, send a interruption singal (SIGINT) with the `^ + C` shortcut to terminate the program.

#### Daemon tool

To run wsync as a daemon, you have to edit the `~/.wsync/wsync.conf` file. In this file you can defines several sync rules that wsync will read in order to automatically synchronize your files in the background.

**Examples:**

	sync = wiredp7://admin:*****@example.org/repo/wsync /home/joe/wsync

	sync = wiredp7://admin:*****@example.org/repo/wsync /home/joe/wsync 3600
	
You can also define running user and group in the `wsync.conf` file, in order to start the program as root (ex: at boot) and let it switch to the user of your choice automatically using `setuid`.

Once your rules are defined in the conf file, you have to run wsync with the `-D` argument:

	wsync -D
	
Wsync will create a PID file `~/.wsync/wsync.pid` which could help you to stop wsync running in daemon mode:

	kill `cat ~/.wsync/wsync.pid`
	
This will shutdown the daemon and delete the PID file.

### Features

* One-way files synchronization from Wired Server 2.0 to a local directory
* Run as daemon program in the background
* Handle each sync operation in a separated thread
* Low CPU consumption and small memory footprint

### Planned

* Reload config without stoping the program using SIGHUP (when running as daemon)
* Two-ways files synchronization: new local files are pushed to the server 
* Catch local FS events to synchonize the local folder with the server in real-time
* A cross-platform GUI program to handle wsync and wsync.conf
* Get rid of limitations…

### Known Limitations

* Remote URL currently only decode spcae HTML character (%20)
* Local path cannot contain spaces

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


