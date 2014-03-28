/* $Id$ */

/*
 *  Copyright (c) 2003-2009 Axel Andersson
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#ifdef HAVE_CORESERVICES_CORESERVICES_H
#include <CoreServices/CoreServices.h>
#endif

#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <openssl/ssl.h>
#include <wired/wired.h>


#include "main.h"
#include "sync.h"
#include "settings.h"


static void						wd_cleanup(void);
static void						wd_usage(void);
static void						wd_version(void);

static void						wd_write_pid(void);
static void						wd_delete_pid(void);
static void						wd_delete_status(void);

static void						wd_signals_init(void);
static void						wd_block_signals(void);
static int						wd_wait_signals(void);
static void						wd_signal_thread(wi_runtime_instance_t *);
static void						wd_signal_crash(int);
static void						wd_signal_pipe(int);

static void						wd_schedule(void);

wi_boolean_t					wd_running = true;

wi_string_t						*wd_config_path;

wi_sqlite3_database_t			*wd_database;

wi_lock_t						*wd_status_lock;
wi_date_t						*wd_start_date;
wi_uinteger_t					wd_current_users, wd_total_users;
wi_uinteger_t					wd_current_downloads, wd_total_downloads;
wi_uinteger_t					wd_current_uploads, wd_total_uploads;
wi_file_offset_t				wd_downloads_traffic, wd_uploads_traffic;
wi_uinteger_t					wd_tracker_current_servers;
wi_uinteger_t					wd_tracker_current_users;
wi_file_offset_t				wd_tracker_current_files;
wi_file_offset_t				wd_tracker_current_size;



int main(int argc, const char **argv) {
	wi_mutable_array_t		*arguments;
	wi_pool_t				*pool;
	wi_string_t				*string, *root_path, *user, *group, *remote_url, *local_path, *homepath, *wsyncpath;
	uint32_t				uid, gid;
	int						ch, facility, interval;
	wi_boolean_t			test_config, daemonize, change_directory, switch_user, dir;

	wi_initialize();
	wi_load(argc, argv);
	
	pool = wi_pool_init(wi_pool_alloc());

	//wi_p7_message_debug		= true;
	//wi_p7_socket_debug		= true;
	wi_log_syslog			= true;
	wi_log_stderr 			= true;
	wi_log_syslog_facility	= LOG_DAEMON;
	wd_status_lock			= wi_lock_init(wi_lock_alloc());
	wd_start_date			= wi_date_init(wi_date_alloc());
	test_config				= false;
	daemonize				= false;
	change_directory		= true;
	switch_user				= true;
	wi_settings_config_path	= wi_string_init_with_cstring(wi_string_alloc(), WD_WSYNC_CONFIG_PATH);
	wd_config_path			= wi_string_init_with_cstring(wi_string_alloc(), WD_WSYNC_CONFIG_PATH);
	arguments				= wi_array_init(wi_mutable_array_alloc());
	root_path				= WI_STR(".");

	interval 				= 0;
	remote_url				= NULL;
	local_path				= NULL;

	while((ch = getopt(argc, (char * const *) argv, "Dd:i:o:I:f:hn:L:ls:tuVvXx")) != -1) {
		switch(ch) {
			case 'i':
				remote_url = wi_string_with_cstring(optarg);
				break;

			case 'o':
				local_path = wi_string_with_cstring(optarg);
				break;

			case 'I':
				interval = wi_string_int32(wi_string_with_cstring(optarg));
				break;

			case 'D':
				daemonize = true;
				wi_log_stderr = false;
				break;

			case 'd':
				root_path = wi_string_with_cstring(optarg);
				break;

			case 'f':
				wi_release(wi_settings_config_path);
				wi_settings_config_path = wi_string_init_with_cstring(wi_string_alloc(), optarg);

				wi_release(wd_config_path);
				wd_config_path = wi_string_init_with_cstring(wi_string_alloc(), optarg);
				break;

			case 'n':
				wi_log_limit = wi_string_uint32(wi_string_with_cstring(optarg));
				break;

			case 'L':
				wi_log_syslog = false;
				wi_log_file = true;
				
				wi_release(wi_log_path);
				wi_log_path = wi_string_init_with_cstring(wi_string_alloc(), optarg);
				break;

			case 'l':
				wi_log_level++;
				break;

			case 's':
				string = wi_string_with_cstring(optarg);
				facility = wi_log_syslog_facility_with_name(string);
				
				if(facility < 0)
					wi_log_fatal(WI_STR("Could not find syslog facility \"%@\": %m"), string);
				
				wi_log_syslog_facility = facility;
				break;

			case 't':
				test_config = true;
				break;

			case 'u':
				break;

			case 'V':
			case 'v':
				wd_version();
				break;
			
			case 'X':
				daemonize = false;
				break;
			
			case 'x':
				daemonize = false;
				change_directory = false;
				switch_user = false;
				break;
			
			case '?':
			case 'h':
			default:
				wd_usage();
				break;
		}
		
		wi_mutable_array_add_data(arguments, wi_string_with_format(WI_STR("-%c"), ch));
		
		if(optarg)
			wi_mutable_array_add_data(arguments, wi_string_with_cstring(optarg));
	}

	if(daemonize) {
		wi_mutable_array_add_data(arguments, WI_STR("-X"));
		
		switch(wi_fork()) {
			case -1:
				wi_log_fatal(WI_STR("Could not fork: %m"));
				break;
				
			case 0:
				if(!wi_execv(wi_string_with_cstring(argv[0]), arguments))
					wi_log_fatal(WI_STR("Could not execute %s: %m"), argv[0]);
				break;
				
			default:
				_exit(0);
				break;
		}
	}
	
	wi_release(arguments);
	
	// if(change_directory) {
	// 	if(!wi_fs_change_directory(root_path))
	// 		wi_log_fatal(WI_STR("Could not change directory to \"%@\": %m"), root_path);
	// }

	wi_log_open();

	homepath 	= wi_user_home();
	wsyncpath 	= wi_string_by_appending_path_component(homepath, WI_STR(WD_WSYNC_PATH));

	wi_fs_create_directory(wsyncpath, 0700);

	wd_config_path = wi_retain(wi_string_by_appending_path_component(homepath, WI_STR(WD_WSYNC_CONFIG_PATH)));
	
	wd_sync_initialize();
	wd_settings_initialize();

	if(!wi_fs_path_exists(wd_config_path, &dir)) {
		wi_config_write_file(wd_config);
	}

	if(!wd_settings_read_config())
		exit(1);

	if(test_config) {
		printf("Config OK\n");

		exit(0);
	}
	
	wi_log_debug(WI_STR("Started as %@ %@"),
		wi_process_path(wi_process()),
		wi_array_components_joined_by_string(wi_process_arguments(wi_process()), WI_STR(" ")));
	
	wi_log_info(WI_STR("Starting WSync version %s (%s)"), WD_VERSION, WI_REVISION);
	
	if(switch_user) {
		uid = wi_config_uid_for_name(wd_config, WI_STR("user"));
		gid = wi_config_uid_for_name(wd_config, WI_STR("group"));
	
		wi_switch_user(uid, gid);
	}
	
	user = wi_user_name();
	group = wi_group_name();
	
	if(user && group) {
		wi_log_info(WI_STR("Operating as user %@ (%d), group %@ (%d)"),
			user, wi_user_id(), group, wi_group_id());
	} else {
		wi_log_info(WI_STR("Operating as user %d, group %d"),
			wi_user_id(), wi_group_id());
	}

	wd_signals_init();
	wd_block_signals();

	if(remote_url && local_path) {
		if(interval > 0)
			wd_sync(remote_url, local_path, interval, true);
		else
			wd_sync(remote_url, local_path, interval, false);
	}
	else {
		daemonize = true;
		wd_sync_start();
	}

	wi_log_debug(WI_STR("Exiting..."));

	wd_write_pid();
	//wd_write_status(true);
		
	wi_pool_drain(pool);
	
	if(interval > 0 || daemonize)
		wd_signal_thread(NULL);
	
	wd_cleanup();
	wi_log_close();
	
	wi_release(pool);

	return 0;
}



static void wd_cleanup(void) {
	
	// wd_server_cleanup();
	// wd_database_close();
	
	wd_delete_pid();
	//wd_delete_status();
}



static void wd_usage(void) {
	fprintf(stderr,
"Usage: wsync [-Dllhtuv] [-i url] [-o path] [-I interval]\n\
              [-f file] [-n lines] [-L file] [-s facility]\n\
Options:\n\
    -i url         wired URL of the remote directory to sync\n\
    -o path        local path of the destination directory\n\
    -I interval    time interval between two synchronization\n\
    -D             daemonize\n\
    -f file        set the config file to load\n\
    -h             display this message\n\
    -n lines       set limit on number of lines for -L\n\
    -L file        set alternate file for log output\n\
    -l             increase log level (can be used twice)\n\
    -s facility    set the syslog(3) facility\n\
    -t             run syntax test on config\n\
    -u             do not chroot(2) to root path\n\
    -v             display version information\n\
\n\
By Rafaël Warnault <%s>\n", WD_BUGREPORT);

	exit(2);
}



static void wd_version(void) {
	fprintf(stderr, "Wired %s (%s)\n", WD_VERSION, WI_REVISION);

	exit(2);
}



#pragma mark -

static void wd_write_pid(void) {
	wi_string_t		*homepath, *wsyncpath, *path, *string;

	homepath 	= wi_user_home();
	wsyncpath 	= wi_string_by_appending_path_component(homepath, WI_STR(WD_WSYNC_PATH));

	path 	= wi_string_by_appending_path_component(wsyncpath, WI_STR("wsync.pid"));
	string 	= wi_string_with_format(WI_STR("%d\n"), getpid());
	
	if(!wi_string_write_to_file(string, path))
		wi_log_error(WI_STR("Could not write to \"%@\": %m"), path);
}



static void wd_delete_pid(void) {
	wi_string_t		*homepath, *wsyncpath, *path;

	homepath 	= wi_user_home();
	wsyncpath 	= wi_string_by_appending_path_component(homepath, WI_STR(WD_WSYNC_PATH));

	path = wi_string_by_appending_path_component(wsyncpath, WI_STR("wsync.pid"));

	if(!wi_fs_delete_path(path))
		wi_log_error(WI_STR("Could not delete \"%@\": %m"), path);
}



void wd_write_status(wi_boolean_t force) {
	static wi_time_interval_t	update;
	wi_string_t					*path, *string;
	wi_time_interval_t			interval;

	interval = wi_time_interval();

	if(!force && interval - update < 1.0)
		return;

	update = interval;
	
	wi_process_set_name(wi_process(), wi_string_with_format(WI_STR("%u %@"),
		wd_current_users,
		wd_current_users == 1
			? WI_STR("user")
			: WI_STR("users")));

	path = WI_STR("wired.status");
	string = wi_string_with_format(WI_STR("%.0f %u %u %u %u %u %u %llu %llu %u %u %llu %llu\n"),
								   wi_date_time_interval(wd_start_date),
								   wd_current_users,
								   wd_total_users,
								   wd_current_downloads,
								   wd_total_downloads,
								   wd_current_uploads,
								   wd_total_uploads,
								   wd_downloads_traffic,
								   wd_uploads_traffic,
								   wd_tracker_current_servers,
								   wd_tracker_current_users,
								   wd_tracker_current_files,
								   wd_tracker_current_size);
	
	if(!wi_string_write_to_file(string, path))
		wi_log_error(WI_STR("Could not write to \"%@\": %m"), path);
}



static void wd_delete_status(void) {
	wi_string_t		*path;
	
	path = WI_STR("wired.status");

	if(!wi_fs_delete_path(path))
		wi_log_error(WI_STR("Could not delete \"%@\": %m"), path);
}





#pragma mark -

static void wd_signals_init(void) {
	signal(SIGILL, wd_signal_crash);
	signal(SIGABRT, wd_signal_crash);
	signal(SIGFPE, wd_signal_crash);
	signal(SIGBUS, wd_signal_crash);
	signal(SIGSEGV, wd_signal_crash);
	signal(SIGPIPE, wd_signal_pipe);
}



static void wd_block_signals(void) {
	wi_thread_block_signals(SIGHUP, SIGUSR1, SIGUSR2, SIGINT, SIGTERM, SIGQUIT, SIGPIPE, SIGPROF, 0);
}



static int wd_wait_signals(void) {
	return wi_thread_wait_for_signals(SIGHUP, SIGUSR1, SIGUSR2, SIGINT, SIGTERM, SIGQUIT, SIGPIPE, SIGPROF, 0);
}



void wd_signal_thread(wi_runtime_instance_t *arg) {
	wi_pool_t		*pool;
	int				signal;

	pool = wi_pool_init(wi_pool_alloc());

	while(wd_running) {
		signal = wd_wait_signals();
		
		switch(signal) {
			case SIGPIPE:
				wi_log_warn(WI_STR("Signal PIPE received, ignoring"));
				break;
				
			case SIGHUP:
				wi_log_info(WI_STR("Signal HUP received, reloading configuration"));

				// wd_settings_read_config();
				
				// wd_schedule();
				break;
				
			case SIGUSR1:
				wi_log_info(WI_STR("Signal USR1 received, registering with trackers"));
				//wd_trackers_register();
				break;

			case SIGUSR2:
				wi_log_info(WI_STR("Signal USR2 received, indexing files"));
				//wd_index_index_files(false);
				break;
				
			case SIGPROF:
				wi_log_info(WI_STR("Signal PROF received, cleaning events"));
				//wd_events_delete_events();
				break;

			case SIGINT:
				wi_log_info(WI_STR("Signal INT received, quitting"));
				wd_running = false;
				break;

			case SIGQUIT:
				wi_log_info(WI_STR("Signal QUIT received, quitting"));
				wd_running = false;
				break;

			case SIGTERM:
				wi_log_info(WI_STR("Signal TERM received, quitting"));
				wd_running = false;
				break;
		}
		
		wi_pool_drain(pool);
	}
	
	wi_release(pool);
}



static void wd_signal_crash(int sigraised) {
	wd_cleanup();

	signal(sigraised, SIG_DFL);
}



static void wd_signal_pipe(int sigraised) {
}



#pragma mark -

static void wd_schedule(void) {
	wd_sync_schedule();
	// wd_files_schedule();
	// wd_index_schedule();
	// wd_server_schedule();
	// wd_servers_schedule();
	// wd_trackers_schedule();
	// wd_transfers_schedule();
	// wd_users_schedule();
}

