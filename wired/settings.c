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

#include "config.h"

#include <wired/wired.h>

#include "main.h"
#include "sync.h"
#include "settings.h"


wi_config_t						*wd_config;



void wd_settings_initialize(void) {
	wi_dictionary_t		*types, *defaults;
	
	types = wi_dictionary_with_data_and_keys(
		WI_INT32(WI_CONFIG_STRINGLIST),		WI_STR("sync"),
		WI_INT32(WI_CONFIG_GROUP),			WI_STR("group"),
		WI_INT32(WI_CONFIG_USER),			WI_STR("user"),
		NULL);
	
	defaults = wi_dictionary_with_data_and_keys(
		wi_array(),							WI_STR("sync"),
		WI_STR("daemon"),					WI_STR("group"),
		WI_STR("wsync"),					WI_STR("user"),
		NULL);
	
	wd_config = wi_config_init_with_path(wi_config_alloc(), wd_config_path, types, defaults);
}



wi_boolean_t wd_settings_read_config(void) {
	wi_boolean_t	result;
	
	result = wi_config_read_file(wd_config);
	
	if(result) {
		wd_settings_apply_settings(wi_config_changes(wd_config));
		
		wi_config_clear_changes(wd_config);
	}
	
	return result;
}



void wd_settings_apply_settings(wi_set_t *changes) {
	wd_sync_apply_settings(changes);
}

