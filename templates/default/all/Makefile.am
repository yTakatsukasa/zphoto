templatesdir = $(pkgdatadir)/templates/default/all
templates_DATA  = zphoto.css .photo.html flash-colors.txt css-colors.txt

zphoto.css: $(top_srcdir)/templates/default/zphoto.css
	cp -p $(top_srcdir)/templates/default/zphoto.css .

.photo.html: $(top_srcdir)/templates/default/.photo.html
	cp -p $(top_srcdir)/templates/default/.photo.html .

flash-colors.txt: $(top_srcdir)/templates/default/flash-colors.txt
	cp $(top_srcdir)/templates/default/flash-colors.txt .

css-colors.txt: $(top_srcdir)/templates/default/css-colors.txt
	cp $(top_srcdir)/templates/default/css-colors.txt .

all: zphoto.css .photo.html
	@perl -e 'while (($$f = shift)) { $$f =~ m!.*/([^/]*)/([^/]*)$$!; \
		  print "cp -p $$f $$2.$$1\n"}' \
			$(top_srcdir)/templates/default/*/*.html\
			$(top_srcdir)/templates/default/*/*.js\
	| sh

install-data-local:
	mkdir -p $(templatesdir)
	cp -p *.html.* $(templatesdir)
	cp -p *.js.* $(templatesdir)

clean:
	rm -f zphoto.css .photo.html *.html.* *.js.* *.txt
