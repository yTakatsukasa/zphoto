templatesdir = $(pkgdatadir)/templates/default/ja
templates_DATA  =	index.html noflash.html zphoto.css .photo.html \
			flash-colors.txt css-colors.txt zoom.js
EXTRA_DIST = $(templates_DATA)

zphoto.css: $(top_srcdir)/templates/default/zphoto.css
	cp $(top_srcdir)/templates/default/zphoto.css .

.photo.html: $(top_srcdir)/templates/default/.photo.html
	cp $(top_srcdir)/templates/default/.photo.html .

flash-colors.txt: $(top_srcdir)/templates/default/flash-colors.txt
	cp $(top_srcdir)/templates/default/flash-colors.txt .

css-colors.txt: $(top_srcdir)/templates/default/css-colors.txt
	cp $(top_srcdir)/templates/default/css-colors.txt .
