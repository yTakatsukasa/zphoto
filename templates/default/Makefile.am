noinst_DATA  = flash-colors.txt css-colors.txt
EXTRA_DIST = zphoto.css .photo.html $(noinst_DATA)
SUBDIRS = . en ja all

flash-colors.txt:
	perl -ne 'printf("%-30s = %s\n", lc($$1), $$2) if /^#define ZPHOTO_(.*_COLOR)\s+"(.*)"/' $(top_srcdir)/zphoto.h |grep -v '^css_' > flash-colors.txt

css-colors.txt:
	perl -ne 'printf("%-30s = %s\n", lc($$1), $$2) if /^#define ZPHOTO_(CSS_.*_COLOR)\s+"(.*)"/' $(top_srcdir)/zphoto.h > css-colors.txt

clean:
	rm -f $(noinst_DATA)
