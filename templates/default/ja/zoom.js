var InternetExplorer = navigator.appName.indexOf("Microsoft") != -1;

function zoom (percentage) {
  var mc = InternetExplorer ? window.zphoto : window.document.zphoto;
  mc.Zoom(percentage);
}


/*
 * 2.0 zoom in with five frames. 
 */
i = 0;
function zoom_in () {
  if (i < 5) {
	zoom(87.06);
        setTimeout("zoom_in()", 1);
        i += 1;
  } else {
     i = 0;
  }
}

/*
 * 2.0 zoom out with five frames. 
 */
j = 0;
function zoom_out () {
  if (j < 5) {
	zoom(114.87);
        setTimeout("zoom_out()", 1);
        j += 1;
  } else {
     j = 0;
  }
}

function ie_on_win32 () {
    if (navigator.appName.indexOf("Microsoft") != -1 &&
        navigator.platform == "Win32")
    {
	return true;
    } else {
	return false;
    }
}

if (ie_on_win32()) {
    document.write('<a href="JavaScript:onClick=zoom_in();">拡大</a>');
    document.write(' ');
    document.write('<a href="JavaScript:onClick=zoom_out();">縮小</a>');
    document.write(' ');
    document.write('<a href="JavaScript:onClick=zoom(0);">元に戻す</a>');
} else {
    document.write('アルバム全体をズームするときは上の表示領域で右クリックをお試しください。');
}
