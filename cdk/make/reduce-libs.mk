$(flashprefix)/root-neutrino-cramfs/lib/ld.so.1 \
$(flashprefix)/root-neutrino-squashfs/lib/ld.so.1 \
$(flashprefix)/root-neutrino-squashfs_nolzma/lib/ld.so.1 \
$(flashprefix)/root-neutrino-jffs2/lib/ld.so.1 \
$(flashprefix)/root-neutrino-jffs2_lzma/lib/ld.so.1 \
$(flashprefix)/root-neutrino-jffs2_lzma_klzma/lib/ld.so.1 \
$(flashprefix)/root-radiobox-cramfs/lib/ld.so.1 \
$(flashprefix)/root-radiobox-squashfs/lib/ld.so.1 \
$(flashprefix)/root-radiobox-squashfs_nolzma/lib/ld.so.1 \
$(flashprefix)/root-radiobox-jffs2/lib/ld.so.1 \
$(flashprefix)/root-radiobox-jffs2_lzma/lib/ld.so.1 \
$(flashprefix)/root-radiobox-jffs2_lzma_klzma/lib/ld.so.1 \
$(flashprefix)/root-enigma-cramfs/lib/ld.so.1 \
$(flashprefix)/root-enigma-squashfs/lib/ld.so.1 \
$(flashprefix)/root-enigma-squashfs_nolzma/lib/ld.so.1 \
$(flashprefix)/root-enigma-jffs2/lib/ld.so.1 \
$(flashprefix)/root-enigma-jffs2_lzma/lib/ld.so.1 \
$(flashprefix)/root-enigma-jffs2_lzma_klzma/lib/ld.so.1 \
$(flashprefix)/root-enigma+neutrino-squashfs/lib/ld.so.1 \
$(flashprefix)/root-enigma+neutrino-squashfs_nolzma/lib/ld.so.1 \
$(flashprefix)/root-enigma+neutrino-jffs2_lzma/lib/ld.so.1 \
$(flashprefix)/root-enigma+neutrino-jffs2_lzma_klzma/lib/ld.so.1 \
$(flashprefix)/root-lcars-jffs2/lib/ld.so.1 \
$(flashprefix)/root-null-jffs2_lzma/lib/ld.so.1 \
$(flashprefix)/root-null-jffs2_lzma_klzma/lib/ld.so.1 \
$(flashprefix)/root-null-jffs2/lib/ld.so.1: \
%/lib/ld.so.1: %
	if [ -e $(appsdir)/misc/libs/libscan/ld.so.1.libscan ] && [ -d $</bin ]; then \
		cd $</bin && \
		gunzip -cd $(appsdir)/misc/libs/libscan/ld.so.1.libscan | TAPE=- tar -x; \
		cd $(buildprefix); \
	fi
	find $</lib -maxdepth 1 -type f -o -type l | xargs rm -f
	-cp -d $(targetprefix)/lib/libnss_dns-?.*.so $</lib
	-cp -d $(targetprefix)/lib/libnss_files-?.*.so $</lib
if ENABLE_PROCPS
	if [ -e $(targetprefix)/lib/libproc-*.so ]; then \
		cp -d $(targetprefix)/lib/libproc-*.so $</lib; \
		chmod +w $</lib/libproc-*.so; \
	fi
endif
	$(MKLIBS) --target $(target) --libc-extras-dir \
		$(targetprefix)/lib/libc_pic \
		-d $</lib \
		-D -L $(mklibs_librarypath) \
		--root $< \
		`find $</bin/ -path "*bin/?*" -type f` \
		`find $</var/bin/ -path "*bin/?*" -type f` \
		`find $</lib/ -name "libnss_*" -type f` \
		`find $</lib/ -name "libsqlite3*" -type f` \
		`find $</lib/ -name "*.so" -type f` \
		`find $</sbin/ -path "*sbin/?*" -type f`
if ENABLE_UCLIBC
	UCLIBC_FILES="libcrypt-@VERSION_uclibc@.so libdl-@VERSION_uclibc@.so libm-@VERSION_uclibc@.so libnsl-@VERSION_uclibc@.so libpthread-@VERSION_uclibc@.so libresolv-@VERSION_uclibc@.so librt-@VERSION_uclibc@.so libuClibc-@VERSION_uclibc@.so libutil-@VERSION_uclibc@.so"; \
	for i in $$UCLIBC_FILES; do cp $(targetprefix)/lib/$$i $</lib/; done; \
	for i in $$UCLIBC_FILES; do ln -sf $$i $</lib/`echo $$i | cut -d - -f 1`.so.0; done
	ln -sf libuClibc-@VERSION_uclibc@.so $</lib/libc.so.0
	ln -sf ld-uClibc.so.0 $</lib/ld-uClibc-@VERSION_uclibc@.so
	if [ -e $</lib/libtuxbox-configfile.so.0 ]; then \
		cp $(targetprefix)/lib/libtuxbox-configfile.so.0.0.0 $</lib/libtuxbox-configfile.so.0; \
	fi
endif
	if [ -e $(flashprefix)/root/lib/liblufs-ftpfs.so.2.0.0 ]; then \
		cp $(flashprefix)/root/lib/liblufs-ftpfs.so.2.0.0 $</lib/liblufs-ftpfs.so.2.0.0 ; \
		ln -sf liblufs-ftpfs.so.2.0.0 $</lib/liblufs-ftpfs.so.2 ; \
		ln -sf liblufs-ftpfs.so.2.0.0 $</lib/liblufs-ftpfs.so ; \
	fi
	if [ -d $(flashprefix)/root/lib/directfb-1.0-0 ]; then \
		cp -va $(flashprefix)/root/lib/directfb-1.0-0 $</lib/ ; \
	fi
	$(target)-strip --remove-section=.comment --remove-section=.note \
		`find $</bin/ -path "*bin/?*"` \
		`find $</sbin/ -path "*sbin/?*"` 2>/dev/null || /bin/true
	$(target)-strip --remove-section=.comment --remove-section=.note --strip-unneeded \
		`find $</lib/tuxbox -name "*.so"` 2>/dev/null || /bin/true
	$(target)-strip $</lib/* 2>/dev/null || /bin/true
	chmod u+rwX,go+rX -R $</
	find $</lib -name *.la | xargs rm -f
	find $</bin -name *.libscan | xargs rm -f
	if [ ! -d $</var_init/lib ]; then \
		$(INSTALL) -d $</var_init/lib ; \
	fi
	for i in libcrypto.so.0.9.7 libssl.so.0.9.7; do \
		if [ -f $</lib/$$i ]; then \
			cp $</lib/$$i $</var_init/lib/$$i; \
		fi; \
	done
	rm -rf $</include
	rm -rf $</lib/pkgconfig
