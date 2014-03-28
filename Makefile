# Makefile.in

abs_top_srcdir		= /Users/nark/Development/Me/Cocoa/Wired/wired2/wsync
datarootdir			= ${prefix}/share
exec_prefix			= ${prefix}
fake_prefix			= ${prefix}
installdir			= $(prefix)/$(wireddir)
objdir				= obj
rundir				= run
mandir				= ${datarootdir}/man
prefix				= /usr/local
wireddir			= wsync

WD_VERSION			= 1.0
WD_MAINTAINER		= 0
WD_USER				= nark
WD_GROUP			= daemon

DISTFILES			= INSTALL LICENSE NEWS README Makefile Makefile.in \
					  config.guess config.status config.h.in config.sub configure \
					  configure.in install-sh libwired man run thirdparty wired revision
SUBDIRS				= libwired

WIREDOBJS			= $(addprefix $(objdir)/wired/,$(notdir $(patsubst %.c,%.o,$(shell find $(abs_top_srcdir)/wired -name "[a-z]*.c"))))
NATPMPOBJS			= $(addprefix $(objdir)/natpmp/,$(notdir $(patsubst %.c,%.o,$(shell find $(abs_top_srcdir)/thirdparty/natpmp -name "[a-z]*.c"))))
MINIUPNPCOBJS		= $(addprefix $(objdir)/miniupnpc/,$(notdir $(patsubst %.c,%.o,$(shell find $(abs_top_srcdir)/thirdparty/miniupnpc -name "[a-z]*.c"))))
TRANSFERTESTOBJS	= $(addprefix $(objdir)/transfertest/,$(notdir $(patsubst %.c,%.o,$(shell find $(abs_top_srcdir)/test/transfertest -name "[a-z]*.c"))))

DEFS				= -DHAVE_CONFIG_H -DENABLE_STRNATPMPERR -DMINIUPNPC_SET_SOCKET_TIMEOUT
CC					= gcc
CFLAGS				= -g -O2
CPPFLAGS			= -I/usr/local/include -DWI_PTHREADS -DWI_CORESERVICES -DWI_CARBON -DWI_DIGESTS -DWI_CIPHERS -DWI_SQLITE3 -DWI_RSA -I/usr/include/libxml2 -DWI_LIBXML2 -DWI_PLIST -DWI_ZLIB -DWI_P7
LDFLAGS				= -L$(rundir)/libwired/lib -L/usr/local/lib
LIBS				= -lwired -framework CoreServices -framework Carbon -lsqlite3 -lcrypto -lxml2 -lz
INCLUDES			= -I$(abs_top_srcdir) -I$(rundir)/libwired/include -I$(abs_top_srcdir)/thirdparty

INSTALL				= /usr/bin/install -c
COMPILE				= $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
PREPROCESS			= $(CC) -E $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
DEPEND				= $(CC) -MM $(INCLUDES)
LINK				= $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@
ARCHIVE				= ar rcs $@

.PHONY: all all-recursive clean-recursive distclean-recursive install install-only install-wired install-man dist clean distclean scmclean
.NOTPARALLEL:

all: all-recursive $(rundir)/wsync $(rundir)/wsyncctl $(rundir)/etc/wsync.conf

ifeq ($(WD_MAINTAINER), 1)
all: Makefile configure config.h.in $(rundir)/transfertest

Makefile: Makefile.in config.status
	./config.status
	        
configure: configure.in
	autoconf

config.h.in: configure.in
	autoheader
	touch $@
	rm -f $@~
endif

all-recursive clean-recursive distclean-recursive:
	@list='$(SUBDIRS)'; \
	for subdir in $$list; do \
		target=`echo $@ | sed s/-recursive//`; \
		(cd $$subdir && $(MAKE) -e $$target) || exit 1; \
	done

$(rundir)/wsync: $(WIREDOBJS) $(NATPMPOBJS) $(MINIUPNPCOBJS) $(rundir)/libwired/lib/libwired.a
	@test -d $(@D) || mkdir -p $(@D)
	$(LINK) $(WIREDOBJS) $(NATPMPOBJS) $(MINIUPNPCOBJS) $(LIBS)

$(rundir)/wsyncctl: $(abs_top_srcdir)/wired/wsyncctl.in
	@test -d $(@D) || mkdir -p $(@D)
	sed -e 's,@wireddir\@,$(fake_prefix)/$(wireddir),g' $< > $@
	chmod +x $@

$(rundir)/etc/wsync.conf: $(abs_top_srcdir)/wired/wsync.conf.in
	@test -d $(@D) || mkdir -p $(@D)
	sed -e 's,@WD_USER\@,$(WD_USER),g' -e 's,@WD_GROUP\@,$(WD_GROUP),g' $< > $@

$(rundir)/transfertest: $(TRANSFERTESTOBJS) $(rundir)/libwired/lib/libwired.a
	@test -d $(@D) || mkdir -p $(@D)
	$(LINK) $(TRANSFERTESTOBJS) $(LIBS)

$(objdir)/wired/%.o: $(abs_top_srcdir)/wired/%.c
	@test -d $(@D) || mkdir -p $(@D)
	$(COMPILE) -I$(<D) -c $< -o $@

$(objdir)/wired/%.d: $(abs_top_srcdir)/wired/%.c
	@test -d $(@D) || mkdir -p $(@D)
	($(DEPEND) $< | sed 's,$*.o,$(@D)/&,g'; echo "$@: $<") > $@

$(objdir)/natpmp/%.o: $(abs_top_srcdir)/thirdparty/natpmp/%.c
	@test -d $(@D) || mkdir -p $(@D)
	$(COMPILE) -I$(<D) -c $< -o $@

$(objdir)/natpmp/%.d: $(abs_top_srcdir)/thirdparty/natpmp/%.c
	@test -d $(@D) || mkdir -p $(@D)
	($(DEPEND) $< | sed 's,$*.o,$(@D)/&,g'; echo "$@: $<") > $@

$(objdir)/miniupnpc/%.o: $(abs_top_srcdir)/thirdparty/miniupnpc/%.c
	@test -d $(@D) || mkdir -p $(@D)
	$(COMPILE) -I$(<D) -c $< -o $@

$(objdir)/miniupnpc/%.d: $(abs_top_srcdir)/thirdparty/miniupnpc/%.c
	@test -d $(@D) || mkdir -p $(@D)
	($(DEPEND) $< | sed 's,$*.o,$(@D)/&,g'; echo "$@: $<") > $@

$(objdir)/transfertest/%.o: $(abs_top_srcdir)/test/transfertest/%.c
	@test -d $(@D) || mkdir -p $(@D)
	$(COMPILE) -I$(<D) -c $< -o $@

$(objdir)/transfertest/%.d: $(abs_top_srcdir)/test/transfertest/%.c
	@test -d $(@D) || mkdir -p $(@D)
	($(DEPEND) $< | sed 's,$*.o,$(@D)/&,g'; echo "$@: $<") > $@

install: all install-man install-wired

install-only: install-man install-wired

install-wired:
	@if [ -e $(mandir)/man8/wsync.8 ]; then \
		touch .update; \
	fi

	$(INSTALL) -m 755 -o $(WD_USER) -g $(WD_GROUP) -d $(HOME)/.wsync/

	@if [ ! -f $(HOME)/.wsync/wsync.conf ]; then \
		$(INSTALL) -m 644 -o $(WD_USER) -g $(WD_GROUP) $(rundir)/etc/wsync.conf $(HOME)/.wsync/ ; \
	fi

	$(INSTALL) -m 755 -o $(WD_USER) -g $(WD_GROUP) $(rundir)/wsync /usr/local/bin/

	@if [ -f .update ]; then \
		echo ""; \
		echo "Update complete!"; \
		echo ""; \
	else \
		echo ""; \
		echo "Installation complete!"; \
		echo ""; \
	fi

	@rm -f .update

install-man:
	$(INSTALL) -m 755 -o $(WD_USER) -g $(WD_GROUP) -d $(mandir)/
	
	$(INSTALL) -m 755 -o $(WD_USER) -g $(WD_GROUP) -d $(mandir)/man5/
	$(INSTALL) -m 644 -o $(WD_USER) -g $(WD_GROUP) $(abs_top_srcdir)/man/wsync.conf.5 $(mandir)/man5/
	$(INSTALL) -m 755 -o $(WD_USER) -g $(WD_GROUP) -d $(mandir)/man8/
	$(INSTALL) -m 644 -o $(WD_USER) -g $(WD_GROUP) $(abs_top_srcdir)/man/wsync.8 $(mandir)/man8/

dist:
	rm -rf dist/wired
	rm -f dist/wired.tar.gz
	mkdir dist/wired

	git describe --always > revision

	@for i in $(DISTFILES); do \
		if [ -e $$i ]; then \
			echo cp -LRp $$i dist/wired/$$i; \
			cp -LRp $$i dist/wired/$$i; \
		fi \
	done

	$(SHELL) -ec "cd dist/wired && WD_MAINTAINER=0 WI_MAINTAINER=0 $(MAKE) -e distclean scmclean"

	$(SHELL) -ec "cd dist/ && tar -czf wired.tar.gz wired"
	rm -rf dist/wired

clean: clean-recursive
	rm -f $(objdir)/wired/*.o
	rm -f $(objdir)/wired/*.d
	rm -f $(objdir)/transfertest/*.o
	rm -f $(objdir)/transfertest/*.d
	rm -f $(objdir)/natpmp/*.o
	rm -f $(objdir)/natpmp/*.d
	rm -f $(objdir)/miniupnpc/*.o
	rm -f $(objdir)/miniupnpc/*.d
	rm -f $(rundir)/wsync
	rm -f $(rundir)/wsyncctl
	rm -f $(rundir)/etc/wsync.conf

distclean: clean distclean-recursive
	rm -rf $(objdir)
	rm -f Makefile config.h config.log config.status config.cache
	rm -f dist/wired.tar.gz

scmclean:
	find . -name .DS_Store -print0 | xargs -0 rm -f
	find . -name CVS -print0 | xargs -0 rm -rf
	find . -name .svn -print0 | xargs -0 rm -rf

ifeq ($(WD_MAINTAINER), 1)
-include $(WIREDOBJS:.o=.d)
-include $(NATPMPOBJS:.o=.d)
-include $(MINIUPNPSOBJS:.o=.d)
-include $(TRANSFERTESTOBJS:.o=.d)
endif
