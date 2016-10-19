ICONS = wxzphoto.ico wxzphoto.xpm
EXTRA_DIST = wxzphoto16.png wxzphoto32.png wxzphoto48.png $(ICONS)

wxzphoto.ico:
	png2ico zphoto.ico zphoto16.png zphoto32.png zphoto48.png

# FIXME: dirty way to change the variable name
wxzphoto.xpm:
	convert wxzphoto16.png wxzphoto.xpm
	perl -i -pe 's/wxzphoto\[\]/wxzphoto_xpm[]/' wxzphoto.xpm
