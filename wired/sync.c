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
static void						wd_sync_sync_timer(wi_timer_t *);

static wi_integer_t				wd_sync_count;
wi_p7_spec_t					*wc_spec;
wi_mutable_array_t 				*wd_syncs;
wi_mutable_array_t 				*wd_running_syncs;



#pragma mark -

void wd_sync_initialize() {
	wd_syncs 				= wi_array_init(wi_mutable_array_alloc());
	wd_running_syncs 		= wi_array_init(wi_mutable_array_alloc());

	wd_running 				= false;
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
	wi_array_t			*syncs, *components;
	wi_string_t			*string, *path;
	wi_url_t			*url;
	wi_boolean_t		changed = false;
	
	if(wi_set_contains_data(changes, WI_STR("sync"))) {
		wi_array_wrlock(wd_syncs);
		
		if(wi_array_count(wd_syncs) > 0) {
			wi_mutable_array_remove_all_data(wd_syncs);
			
			changed = true;
		}
		
		syncs = wi_config_stringlist_for_name(wd_config, WI_STR("sync"));
		
		if(syncs) {
			enumerator = wi_array_data_enumerator(syncs);
			
			while((string = wi_enumerator_next_data(enumerator))) {
				components = wi_string_components_separated_by_string(string, WI_STR(" "));

				if(wi_array_count(components) < 2) {
					wi_log_error(WI_STR("Invalid sync entry \"%@\""), string);
					continue;
				}

				url = wi_autorelease(wi_url_init_with_string(wi_url_alloc(), wi_array_data_at_index(components, 0)));

				if(!wi_url_is_valid(url)) {
					wi_log_error(WI_STR("Could not parse remote URL \"%@\""), string);
					continue;
				}

				wi_mutable_array_add_data(wd_syncs, string);

				changed = true;
			}
		}
		
		wi_array_unlock(wd_syncs);
	}
		
	// if(changed)
	// 	wd_sync_start();
}






#pragma mark -


void wd_sync_start() {
	wi_enumerator_t		*enumerator;
	wi_string_t			*string, *url, *path;
	wi_integer_t 		interval;
	wi_timer_t 			*timer;
	wi_array_t			*components;

	wd_running = true;

	enumerator = wi_array_data_enumerator(wd_syncs);

	while((string = wi_enumerator_next_data(enumerator))) {
		components = wi_string_components_separated_by_string(string, WI_STR(" "));

		if(wi_array_count(components) == 0)
			continue;

		url  = wi_array_data_at_index(components, 0);
		path = wi_array_data_at_index(components, 1);
		interval = wi_string_int32(wi_array_data_at_index(components, 2));

		wd_sync(url, path, interval, true);
	}
}




void wd_sync_stop() {
	wd_running = false;
}





void wd_sync(wi_string_t *remote_url, wi_string_t *local_path, wi_integer_t interval, wi_boolean_t threaded) {
	wi_mutable_array_t *arguments;

	arguments = wi_mutable_array();

	wi_mutable_array_add_data(arguments, remote_url);
	wi_mutable_array_add_data(arguments, local_path);
	wi_mutable_array_add_data(arguments, WI_INT32(interval));

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
	wi_pool_t			*pool;
	wi_mutable_array_t  *arguments = argument;
	wi_p7_socket_t		*socket;
	wi_p7_message_t 	*message;
	wi_string_t			*sync;
	wi_string_t			*remote_path, *local_path, *file_path, *path, *incompleted_path;
	wi_string_t 		*remote_sub_path, *local_sub_path;
	wi_mutable_array_t 	*remote_files, *local_files, *diff_files;
	wi_enumerator_t		*local_enumerator, *remote_enumerator, *diff_enumerator;
	wi_url_t			*url;
	wi_integer_t		synced_count, failed_count;
	wi_boolean_t		is_dir;
	wi_integer_t		interval;
	
	pool = wi_pool_init(wi_pool_alloc());

	url 		= wi_url_with_string(wi_array_data_at_index(arguments, 0));
	local_path	= wi_array_data_at_index(arguments, 1);
	interval	= wi_number_int32(wi_array_data_at_index(arguments, 2));
	remote_path = wi_url_path(url);

	if(wi_string_contains_string(remote_path, WI_STR("%20"), 0))
		remote_path = wi_string_by_replacing_string_with_string(remote_path, WI_STR("%20"), WI_STR(" "), 0);

	remote_files = wi_autorelease(wi_array_init(wi_mutable_array_alloc()));
	local_files  = wi_autorelease(wi_array_init(wi_mutable_array_alloc()));
	diff_files   = wi_autorelease(wi_array_init(wi_mutable_array_alloc()));

	while(wd_running) {
		synced_count = 0;
		failed_count = 0;

		socket = wc_connect(url);
		
		if(!socket)
			continue;
		
		if(!wc_login(socket, url)) {
			wi_log_fatal(WI_STR("Could not login: %m"));
			continue;
		}
		
		// check if local path directory exists
		if(!wi_fs_path_exists(local_path, &is_dir) && !wi_fs_create_directory(local_path, 0777)) 
				wi_log_fatal(WI_STR("Could not create directory %m"));	

		// list the remote directory
		remote_files = wc_sync_list_directory_at_path(socket, remote_path, true);

		wi_log_info(WI_STR("Sync started: %d item(s)"), wi_array_count(remote_files));

		remote_enumerator = wi_array_data_enumerator(remote_files);

		while((message = wi_enumerator_next_data(remote_enumerator))) {
			remote_sub_path = wi_p7_message_string_for_name(message, WI_STR("wired.file.path"));
			wi_range_t range = wi_string_range_of_string(remote_sub_path, remote_path, 0);

			if(range.location == WI_NOT_FOUND) {
				wi_log_fatal(WI_STR("Could not resolve remote path %@"), remote_path);	
				failed_count++;
				continue;
			}

			if(wi_is_equal(wi_string_path_extension(remote_sub_path), WS_FILE_TRANSFER_EXT))
				continue;

			local_sub_path = wi_string_by_appending_path_component(local_path, wi_string_by_deleting_characters_in_range(remote_sub_path, range));
			
			incompleted_path = wi_string_by_appending_path_extension(local_sub_path, WI_STR("WiredTransfer"));

			wd_file_type_t 		file_type;

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

		if(interval > 0)
			wi_thread_sleep(interval);
		else
			break;
	}

	wi_release(pool);
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

	//check if local file already exist
	if(wi_fs_path_exists(completed_path, &is_dir) && !is_dir) {
		wi_log_debug(WI_STR("local file already exist"));	

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
	
	while(true) {
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
	
	if(!addresses)
		return NULL;
	
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




static void	wd_sync_sync_timer(wi_timer_t *timer) {
	// wi_string_t *string;

	// string = wi_timer_data(timer);

	// if(wi_dictionary_contains_key(wd_running_sync_timers, string))
	// 	return;

	// wi_dictionary_wrlock(wd_running_sync_timers);

	// wi_mutable_dictionary_set_data_for_key(wd_running_sync_timers, timer, string);

	// wi_dictionary_unlock(wd_running_sync_timers);

	// if(!wi_thread_create_thread(wd_sync_thread, string)) {
	// 	wi_mutable_dictionary_remove_data_for_key(wd_running_sync_timers, string);
	// 	wi_log_error(WI_STR("Could not create sync thread: %m"));
	// }
}
