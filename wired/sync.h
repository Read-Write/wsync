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

void 								wd_sync(wi_string_t*, wi_string_t*, wi_integer_t, wi_boolean_t);

void 								wd_sync_apply_settings(wi_set_t *);

extern wi_p7_spec_t					*wc_spec;

#endif /* WD_SYNC_H */