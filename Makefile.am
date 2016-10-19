AUTOMAKE_OPTIONS = 1.4 no-dependencies
ACLOCAL_AMFLAGS = -I m4

localedir = $(datadir)/locale

zphotofont = EfontSerifB.fdb
ZPHOTO_TEMPLATE_DIR_RELATIVE = templates/default/en
ZPHOTO_FONT_RELATIVE         = $(zphotofont)
ZPHOTO_TEMPLATE_DIR = $(pkgdatadir)/$(ZPHOTO_TEMPLATE_DIR_RELATIVE)
ZPHOTO_FONT         = $(pkgdatadir)/fonts/$(ZPHOTO_FONT_RELATIVE)


noinst_LIBRARIES    =	libzphoto.a
libzphoto_a_SOURCES =	alist.c exif.c progress.c template.c zphoto.c \
                        util.c flash.c image.cpp config.c zphoto.h 

EXTRA_PROGRAMS   = wxzphoto
wxzphoto_SOURCES = wxzphoto.cpp wxzphoto.h
wxzphoto_LDADD   = $(LDADD) $(LIBWX_LIBS) $(RESOURCE_OBJECT)

bin_PROGRAMS   = zphoto $(WXZPHOTO)
zphoto_SOURCES = main.cpp


SUBDIRS  =      m4 support templates doc icon po .
INCLUDES =	-I$(top_srcdir) $(LIBIMLIB2_CFLAGS) \
		$(LIBMAGICK_CFLAGS) $(AVIFILE_CXXFLAGS) \
		$(LIBWX_CXXFLAGS)
LDADD    =	libzphoto.a support/libsupport.a\
		$(LIBMING_LIBS) $(LIBPOPT_LIBS) $(LIBIMLIB2_LIBS) \
		$(LIBMAGICK_LIBS) $(LIBMAGICK_LDFLAGS) $(AVIFILE_LDFLAGS)
DEFS   =	@DEFS@ \
		-DLOCALEDIR=\"$(localedir)\"\
		-DZPHOTO_TEMPLATE_DIR='"$(ZPHOTO_TEMPLATE_DIR)"'\
		-DZPHOTO_FONT='"$(ZPHOTO_FONT)"'\
		-DZPHOTO_TEMPLATE_DIR_RELATIVE='"$(ZPHOTO_TEMPLATE_DIR_RELATIVE)"'\
		-DZPHOTO_FONT_RELATIVE='"$(ZPHOTO_FONT_RELATIVE)"'

fontsdir  = $(pkgdatadir)/fonts
fonts_DATA = $(zphotofont)
EXTRA_DIST = config.rpath mkinstalldirs $(fonts_DATA) wxzphoto.rc wxzphoto.wxg

zphoto_DEPENDENCIES = libzphoto.a
wxzphoto_DEPENDENCIES = $(RESOURCE_OBJECT) libzphoto.a


#
# For Windows
#

resource.o: wxzphoto.rc icon/wxzphoto.ico 
	windres -i wxzphoto.rc -o resource.o

locale: po/ja_JP.SJIS.gmo
	rm -rf locale
	mkdir -p locale/ja/LC_MESSAGES
	cp po/ja_JP.SJIS.gmo locale/ja/LC_MESSAGES/zphoto.mo

w32_pkgname = $(PACKAGE)-w32-$(VERSION)
dist-w32: all locale
	rm -rf $(w32_pkgname) $(w32_pkgname).zip
	mkdir -p $(w32_pkgname)
	cp -a locale $(w32_pkgname)
	cp -a EfontSerifB.fdb $(w32_pkgname)
	cp -a wxzphoto.exe $(w32_pkgname)/zphoto.exe
	strip $(w32_pkgname)/zphoto.exe
	nkf COPYING > $(w32_pkgname)/COPYING.TXT
	nkf README > $(w32_pkgname)/README.TXT
	mkdir -p $(w32_pkgname)/templates/default/{en,ja,all}
	find templates -regex '^.*\.html\(\...\)?$$' \
		-or -name '*.css' -or -name '*.txt' |\
		sed -e 's!\(.*\)!nkf -s \1 > $(w32_pkgname)/\1!' | sh
	find $(w32_pkgname)/templates -maxdepth 2 -type f |xargs rm
	zip -r $(w32_pkgname).zip $(w32_pkgname)
#	rm -rf $(w32_pkgname)

