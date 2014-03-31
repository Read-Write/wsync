/* $Id$ */

/*
 *  Copyright (c) 2011-2014 Rafaël Warnault
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

#include <signal.h>

#include "sync.h"
#include "settings.h"
#include "main.h"


#define WS_FILE_TRANSFER_EXT	WI_STR("WiredTransfer")


static void 					wd_sync_thread(wi_runtime_instance_t *);
static wi_boolean_t 			wc_download_file(wi_p7_socket_t *, wi_p7_message_t *, wi_string_t *, wi_string_t *);
static wi_p7_socket_t *			wc_connect(wi_url_t *);
static wi_boolean_t				wc_login(wi_p7_socket_t *, wi_url_t *);
static wi_p7_message_t *		wc_write_message_and_read_reply(wi_p7_socket_t *, wi_p7_message_t *, wi_string_t *);
static wi_array_t *		 		wc_sync_list_directory_at_path(wi_p7_socket_t *, wi_string_t *, wi_boolean_t);
static wi_array_t * 			wc_sync_parse_config_entry(wi_string_t *);

static wi_integer_t				wd_sync_count;
wi_p7_spec_t					*wc_spec;
wi_mutable_array_t 				*wd_syncs;
wi_mutable_array_t 				*wd_running_syncs;



#pragma mark -

void wd_sync_initialize() {
	wd_syncs 				= wi_array_init(wi_mutable_array_alloc());
	wd_running_syncs 		= wi_array_init(wi_mutable_array_alloc());

	wd_running 				= false;
	// load the wired specfication from a C header
	wc_spec 				= wi_p7_spec_init_with_string(wi_p7_spec_alloc(), wi_string_with_cstring(
#include "wired.xml.h"
	), WI_P7_CLIENT);
	
	if(!wc_spec)
		wi_log_fatal(WI_STR("Could not open wired.xml: %m"));
}





#pragma mark -

void wd_sync_schedule() {

}



void wd_sync_apply_settings(wi_set_t *changes) {
	wi_enumerator_t		*enumerator;
	wi_array_t			*syncs, *arguments;
	wi_string_t			*string, *path;
	wi_url_t			*url;
	wi_boolean_t		changed = false;
	
	if(wi_set_contains_data(changes, WI_STR("sync"))) {
		wi_array_wrlock(wd_syncs);
		
		if(wi_array_count(wd_syncs) > 0) {
			wi_mutable_array_remove_all_data(wd_syncs);
			
			changed = true;
		}
		
		// get an array of sync entries (string)
		syncs = wi_config_stringlist_for_name(wd_config, WI_STR("sync"));
		
		if(syncs) {
			enumerator = wi_array_data_enumerator(syncs);
			
			while((string = wi_enumerator_next_data(enumerator))) {
				// check if sync entry is valid
				arguments = wi_autorelease(wi_array_init_with_argument_string(wi_array_alloc(), string, -1));

				if(wi_array_count(arguments) < 4) {
					wi_log_error(WI_STR("Invalid sync entry, missing parameters \"%@\""), string);
					continue;
				}

				// add to local syncs
				wi_mutable_array_add_data(wd_syncs, string);

				changed = true;
			}
		}
		wi_array_unlock(wd_syncs);
	}
}






#pragma mark -


void wd_sync_start() {
	wi_enumerator_t		*enumerator;
	wi_string_t			*string;
	wi_timer_t 			*timer;
	wi_array_t			*arguments;

	wd_running = true;

	enumerator = wi_array_data_enumerator(wd_syncs);

	while((string = wi_enumerator_next_data(enumerator))) {
		arguments = wi_autorelease(wi_array_init_with_argument_string(wi_array_alloc(), string, -1));

		if(wi_array_count(arguments) < 4)
			continue;

		wd_sync(arguments, true);
	}
}




void wd_sync_stop() {
	wd_running = false;
}





void wd_sync(wi_array_t *arguments, wi_boolean_t threaded) {
	wd_running = true;

	if(threaded) {
		if(!wi_thread_create_thread(wd_sync_thread, arguments)) {
			wi_log_error(WI_STR("Could not create sync thread: %m"));
		}
	} else {
		wd_sync_thread(arguments);
	}
}






#pragma mark -

void wd_sync_thread(wi_runtime_instance_t *argument) {
	wi_pool_t			*pool, *subpool;
	wi_mutable_array_t  *arguments = argument;
	wi_string_t			*arg;
	wi_p7_socket_t		*socket;
	wi_p7_message_t 	*message;
	wi_string_t			*remote_path, *local_path, *incompleted_path;
	wi_string_t 		*remote_sub_path, *local_sub_path;
	wi_mutable_array_t 	*remote_files, *local_files;
	wi_enumerator_t		*args_enumerator, *remote_enumerator;
	wi_url_t			*url;
	wd_file_type_t 		file_type;
	wi_integer_t		synced_count, failed_count;
	wi_boolean_t		is_dir, recursive = false;
	wi_integer_t		interval = 0;

	pool = wi_pool_init(wi_pool_alloc());

	while(wd_running) {
		// parse arguments
		args_enumerator = wi_array_data_enumerator(arguments);

		while((arg = wi_enumerator_next_data(args_enumerator))) {
			if(wi_is_equal(arg, WI_STR("-i"))) {
				arg = wi_enumerator_next_data(args_enumerator);
				url = wi_url_with_string(arg);
			}
			else if(wi_is_equal(arg, WI_STR("-o"))) {
				arg = wi_enumerator_next_data(args_enumerator);
				local_path = arg;
			}
			else if(wi_is_equal(arg, WI_STR("-I"))) {
				arg = wi_enumerator_next_data(args_enumerator);
				interval = wi_string_int32(arg);
			}
			else if(wi_is_equal(arg, WI_STR("-R"))) {
				recursive = true;
			}
		}

		if(!url) {
			wi_log_fatal(WI_STR("Stopped. Missing arguments: -i"));
			break;
		} 
		else if(!local_path) {
			wi_log_fatal(WI_STR("Stopped. Missing arguments: -o"));
			break;
		}

		remote_path = wi_url_path(url);

		if(wi_string_contains_string(remote_path, WI_STR("%20"), 0))
			remote_path = wi_string_by_replacing_string_with_string(remote_path, WI_STR("%20"), WI_STR(" "), 0);

		remote_files = wi_mutable_array();
		local_files  = wi_mutable_array();

		socket = wc_connect(url);
		
		if(!socket) {
			wi_pool_drain(pool);
			continue;
		}
		
		if(!wc_login(socket, url)) {
			wi_log_fatal(WI_STR("Could not login: %m"));

			wi_pool_drain(pool);
			continue;
		}

		synced_count = 0;
		failed_count = 0;
		
		// check if local path directory exists
		if(!wi_fs_path_exists(local_path, &is_dir) && !wi_fs_create_directory(local_path, 0777)) 
				wi_log_fatal(WI_STR("Could not create directory %m"));	

		// list the remote directory
		remote_files = wc_sync_list_directory_at_path(socket, remote_path, recursive);

		wi_log_info(WI_STR("Sync started: %d item(s)"), wi_array_count(remote_files));

		remote_enumerator = wi_array_data_enumerator(remote_files);

		while(wd_running && (message = wi_enumerator_next_data(remote_enumerator))) {

			remote_sub_path = wi_p7_message_string_for_name(message, WI_STR("wired.file.path"));
			wi_range_t range = wi_string_range_of_string(remote_sub_path, remote_path, 0);

			if(range.location == WI_NOT_FOUND) {
				wi_log_fatal(WI_STR("Could not resolve remote path %@"), remote_path);	
				failed_count++;
				continue;
			}

			if(wi_is_equal(wi_string_path_extension(remote_sub_path), WS_FILE_TRANSFER_EXT)) {
				continue;
			}

			local_sub_path = wi_string_by_deleting_characters_in_range(remote_sub_path, range);
			local_sub_path = wi_string_by_appending_path_component(local_path, local_sub_path);
			incompleted_path = wi_string_by_appending_path_extension(local_sub_path, WI_STR("WiredTransfer"));

			wi_p7_message_get_enum_for_name(message, &file_type, WI_STR("wired.file.type"));

			// if file is a directory
			if(file_type != WD_FILE_TYPE_FILE) {		
				if(!wi_fs_path_exists(local_sub_path, &is_dir) && !wi_fs_create_directory(local_sub_path, 0777)) {
					wi_log_fatal(WI_STR("Could not create directory %m"));	
					failed_count++;
				} else {
					synced_count++;
				}

			// if file is a file
			} else {
				if(wc_download_file(socket, message, remote_sub_path, local_sub_path))
					synced_count++;
				else
					failed_count++;
			}
		}

		wi_log_info(WI_STR("Sync done: %d item(s) synchronized - %d item(s) failed"), synced_count, failed_count);

		wi_pool_drain(pool);

		if(interval == 0) {
			wd_running = false;
			break;
		}

		wi_thread_sleep(interval);
	}

	wi_release(pool);

	kill(getpid(), SIGINT);
}






#pragma mark -

static wi_boolean_t wc_download_file(wi_p7_socket_t *socket, wi_p7_message_t *message, wi_string_t *remote_path, wi_string_t *local_path) {
	wi_p7_message_t		*reply;
	wi_string_t			*name, *completed_path, *error;
	wi_file_t			*file;
	void 				*buffer;
	wi_p7_uint32_t		queue;
	wi_p7_uint64_t		size, offset;
	wi_integer_t		readsize;
	wi_boolean_t		is_dir;
	wi_p7_uint64_t 		local_size, remote_size;
	wi_fs_stat_t 		sb;


	completed_path = local_path;
	local_path = wi_string_by_appending_path_extension(local_path, WI_STR("WiredTransfer"));
	offset = 0;

	// check if local file already exist
	if(wi_fs_path_exists(completed_path, &is_dir) && !is_dir) {
		wi_log_debug(WI_STR("Skip, local file already exist: %@"), completed_path);	

		if(wi_fs_stat_path(completed_path, &sb)) {
			//compare remote and local file size, skip if 0 or equal
			local_size = sb.size;

			if(local_size > 0) {
				wi_p7_message_get_uint64_for_name(message, &remote_size, WI_STR("wired.file.data_size"));

				if(remote_size == local_size)
					return true;
			}
		}	
	}

	//check if local incomplete file already exist
	if(wi_fs_path_exists(local_path, &is_dir) && !is_dir) {
		wi_log_debug(WI_STR("local file already exist"));	

		if(wi_fs_stat_path(local_path, &sb)) {
			//compare remote and local file size, skip if 0 or equal
			local_size = sb.size;

			if(local_size > 0) {
				wi_p7_message_get_uint64_for_name(message, &remote_size, WI_STR("wired.file.data_size"));

				if(remote_size != local_size)
					offset = local_size;
			}
		}	
	}

	// check for incomplete file
	if(wi_fs_path_exists(wi_string_by_appending_path_extension(local_path, WI_STR("WiredTransfer")), &is_dir)) {
		local_path = wi_string_by_appending_path_extension(local_path, WI_STR("WiredTransfer"));
	}

	file = wi_file_for_updating(local_path);

	if(!file) {
		wi_log_error(WI_STR("Could not open %@: %m"), local_path);
		return false;
	}
	
	message = wi_p7_message_with_name(WI_STR("wired.transfer.download_file"), wc_spec);
	wi_p7_message_set_string_for_name(message, remote_path, WI_STR("wired.file.path"));
	wi_p7_message_set_uint64_for_name(message, offset, WI_STR("wired.transfer.data_offset"));
	wi_p7_message_set_uint64_for_name(message, 0, WI_STR("wired.transfer.rsrc_offset"));
	
	if(!wi_p7_socket_write_message(socket, 0.0, message))
		wi_log_fatal(WI_STR("Could not write message for %@: %m"), remote_path);
	
	while(wd_running) {
		message = wi_p7_socket_read_message(socket, 0.0);
		
		if(!message)
			wi_log_fatal(WI_STR("Could not read message for %@: %m"), remote_path);
		
		name = wi_p7_message_name(message);
		
		if(wi_is_equal(name, WI_STR("wired.transfer.download"))) {
			wi_p7_message_get_oobdata_for_name(message, &size, WI_STR("wired.transfer.data"));
			
			wi_log_debug(WI_STR("Downloading %@..."), remote_path);
			
			while(size > 0) {
				readsize = wi_p7_socket_read_oobdata(socket, 0.0, &buffer);
				
				if(readsize < 0)
					wi_log_fatal(WI_STR("Could not read download for %@: %m"), remote_path);

				wi_file_write_buffer(file, buffer, readsize);
				
				size -= readsize;
			}

			if(size == 0)
				wi_fs_rename_path(local_path, completed_path);
			
			return true;
		}
		else if(wi_is_equal(name, WI_STR("wired.transfer.queue"))) {
			wi_p7_message_get_uint32_for_name(message, &queue, WI_STR("wired.transfer.queue_position"));
			
			wi_log_info(WI_STR("Queued at position %u for %@"), queue, remote_path);
		}
		else if(wi_is_equal(name, WI_STR("wired.send_ping"))) {
			reply = wi_p7_message_with_name(WI_STR("wired.ping"), wc_spec);
			
			if(!wi_p7_socket_write_message(socket, 0.0, reply))
				wi_log_fatal(WI_STR("Could not send message for %@: %m"), remote_path);
		}
		else if(wi_is_equal(name, WI_STR("wired.error"))) {
			error = wi_p7_message_enum_name_for_name(message, WI_STR("wired.error"));
			
			wi_log_fatal(WI_STR("Could not download %@: %@"), remote_path, error);
			return false;
		}
		else {
			wi_log_fatal(WI_STR("Unexpected message %@ for download of %@"), name, remote_path);
			return false;
		}
	}
	return false;
}






static wi_p7_socket_t * wc_connect(wi_url_t *url) {
	wi_enumerator_t		*enumerator;
	wi_socket_t			*socket;
	wi_p7_socket_t		*p7_socket;
	wi_array_t			*addresses;
	wi_address_t		*address;
	wi_string_t 		*password;
	
	addresses = wi_host_addresses(wi_host_with_string(wi_url_host(url)));
	
	if(!addresses) {
		return NULL;
	}
	
	enumerator = wi_array_data_enumerator(addresses);
	
	while((address = wi_enumerator_next_data(enumerator))) {
		if(wi_url_port(url))
			wi_address_set_port(address, wi_url_port(url));
		else
			wi_address_set_port(address, 4871);

		socket = wi_socket_with_address(address, WI_SOCKET_TCP);
		
		if(!socket)
			continue;
		
		wi_socket_set_interactive(socket, true);
		
		wi_log_debug(WI_STR("Connecting to %@:%u..."), wi_address_string(address), wi_address_port(address));
		
		if(!wi_socket_connect(socket, 10.0)) {
			wi_socket_close(socket);
			
			continue;
		}
		
		wi_log_debug(WI_STR("Connected, performing handshake"));

		p7_socket = wi_autorelease(wi_p7_socket_init_with_socket(wi_p7_socket_alloc(), socket, wc_spec));

		password = wi_url_password(url);

		if(!password)
			password = WI_STR("");
		
		if(!wi_p7_socket_connect(p7_socket,
								 10.0,
								 WI_P7_ENCRYPTION_RSA_AES256_SHA1 | WI_P7_CHECKSUM_SHA1,
								 WI_P7_BINARY,
								 wi_url_user(url),
								 wi_string_sha1(password))) {
			wi_log_error(WI_STR("Could not connect to %@: %m"), wi_address_string(address));
			
			wi_socket_close(socket);
			
			continue;
		}
		
		wi_log_debug(WI_STR("Connected to P7 server with protocol %@ %@"),
			wi_p7_socket_remote_protocol_name(p7_socket), wi_p7_socket_remote_protocol_version(p7_socket));
		
		return p7_socket;
	}
	
	return NULL;
}



static wi_boolean_t wc_login(wi_p7_socket_t *socket, wi_url_t *url) {
	wi_p7_message_t		*message;
	wi_string_t 		*password;

	wi_log_debug(WI_STR("Performing Wired handshake..."));
	
	message = wi_p7_message_with_name(WI_STR("wired.client_info"), wc_spec);
	wi_p7_message_set_string_for_name(message, WI_STR("wsync"), WI_STR("wired.info.application.name"));
	wi_p7_message_set_string_for_name(message, WI_STR("1.0"), WI_STR("wired.info.application.version"));
	wi_p7_message_set_string_for_name(message, WI_STR("1"), WI_STR("wired.info.application.build"));
	wi_p7_message_set_string_for_name(message, wi_process_os_name(wi_process()), WI_STR("wired.info.os.name"));
	wi_p7_message_set_string_for_name(message, wi_process_os_release(wi_process()), WI_STR("wired.info.os.version"));
	wi_p7_message_set_string_for_name(message, wi_process_os_arch(wi_process()), WI_STR("wired.info.arch"));
	wi_p7_message_set_bool_for_name(message, false, WI_STR("wired.info.supports_rsrc"));

	message = wc_write_message_and_read_reply(socket, message, NULL);
									  
	wi_log_debug(WI_STR("Connected to \"%@\""), wi_p7_message_string_for_name(message, WI_STR("wired.info.name")));
	wi_log_debug(WI_STR("Logging in as \"%@\"..."), wi_url_user(url));

	password = wi_url_password(url);

	if(!password)
		password = WI_STR("");
	
	message = wi_p7_message_with_name(WI_STR("wired.send_login"), wc_spec);
	wi_p7_message_set_string_for_name(message, wi_url_user(url), WI_STR("wired.user.login"));
	wi_p7_message_set_string_for_name(message, wi_string_sha1(password), WI_STR("wired.user.password"));
	
	message = wc_write_message_and_read_reply(socket, message, NULL);
	
	if(!wi_is_equal(wi_p7_message_name(message), WI_STR("wired.login"))) {
		wi_log_error(WI_STR("Login failed"));
		
		return false;
	}

	message = wi_p7_socket_read_message(socket, 0.0);
	
	if(!message)
		return false;
	
	if(!wi_is_equal(wi_p7_message_name(message), WI_STR("wired.account.privileges"))) {
		wi_log_error(WI_STR("Login failed"));
		
		return false;
	}

	message = wi_p7_message_with_name(WI_STR("wired.user.set_nick"), wc_spec);
	wi_p7_message_set_string_for_name(message, WI_STR("wsync"), WI_STR("wired.user.nick"));
	
	wc_write_message_and_read_reply(socket, message, NULL);
	
	return true;
}



static wi_p7_message_t * wc_write_message_and_read_reply(wi_p7_socket_t *socket, wi_p7_message_t *message, wi_string_t *expected_error) {
	wi_string_t		*name, *error;
	
	if(!wi_p7_socket_write_message(socket, 0.0, message))
		wi_log_fatal(WI_STR("Could not write message: %m"));
	
	message = wi_p7_socket_read_message(socket, 0.0);
	
	if(!message)
		wi_log_fatal(WI_STR("Could not read message: %m"));
	
	name = wi_p7_message_name(message);
	
	if(wi_is_equal(name, WI_STR("wired.error"))) {
		error = wi_p7_message_enum_name_for_name(message, WI_STR("wired.error"));
		
		if(expected_error) {
			if(!wi_is_equal(error, expected_error))
			   wi_log_fatal(WI_STR("Unexpected error %@"), error);
		} else {
			wi_log_fatal(WI_STR("Unexpected error %@"), error);
		}
	}
	
	return message;
}



static wi_array_t * wc_sync_list_directory_at_path(wi_p7_socket_t *socket, wi_string_t *path, wi_boolean_t recursive) {
	wi_p7_message_t 		*message;
	wi_mutable_array_t 		*contents;

	contents = wi_autorelease(wi_array_init(wi_mutable_array_alloc()));

	message = wi_p7_message_with_name(WI_STR("wired.file.list_directory"), wc_spec);
	wi_p7_message_set_string_for_name(message, path, WI_STR("wired.file.path"));
	wi_p7_message_set_bool_for_name(message, recursive, WI_STR("wired.file.recursive"));

	wi_p7_socket_write_message(socket, 0.0, message);

	// append received path to the local files list
	while((message = wi_p7_socket_read_message(socket, 0.0))) {
		if(wi_is_equal(wi_p7_message_name(message), WI_STR("wired.file.file_list"))) {
			wi_mutable_array_add_data(contents, message);
		}
		else if(wi_is_equal(wi_p7_message_name(message), WI_STR("wired.file.file_list.done"))) {
			break;
		}
		else if(wi_is_equal(wi_p7_message_name(message), WI_STR("wired.error"))) {
			break;
		}
	}
	return contents;
}





static wi_array_t * wc_sync_parse_config_entry(wi_string_t *string) {
	wi_mutable_array_t 	*args;
	wi_regexp_t			*regexp;
	wi_string_t			*match;

	match	= NULL;
	args 	= wi_mutable_array();



	return args;
}
