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


#ifndef WD_SYNC_H
#define WD_SYNC_H 1


#include <wired/wired.h>

enum _wd_file_type {
	WD_FILE_TYPE_FILE				= 0,
	WD_FILE_TYPE_DIR,
	WD_FILE_TYPE_UPLOADS,
	WD_FILE_TYPE_DROPBOX
};
typedef enum _wd_file_type			wd_file_type_t;

void								wd_sync_initialize(void);

void								wd_sync_schedule(void);

void 								wd_sync_start(void);
void 								wd_sync_stop(void);

void 								wd_sync(wi_array_t *, wi_boolean_t);

void 								wd_sync_apply_settings(wi_set_t *);

extern wi_p7_spec_t					*wc_spec;

#endif /* WD_SYNC_H */