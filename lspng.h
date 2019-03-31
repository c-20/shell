// requires sched.h for sched_yield();

#define LSPNGSTRINGSIZE		128
#define LSPNGMAXDIGITS		9

#define LSPNGHEADERSIZE		8
#define LSPNGBITDEPTH		8
#define LSPNGWIDEBITDEPTH	16
#define LSPNGRGBCHANNELS	3
//#define LSPNGCOLOURTYPE		PNG_COLOR_TYPE_RGB
#define LSPNGCOLOURTYPE		PNG_COLOR_TYPE_RGBA
#define LSPNGNOINTERLACE	PNG_INTERLACE_NONE
#define LSPNGCOMPRESSTYPE	PNG_COMPRESSION_TYPE_DEFAULT
#define LSPNGCOMPRESSLEVEL	6 // Z_DEFAULT_COMPRESSION
#define LSPNGFILTERTYPE		PNG_FILTER_TYPE_DEFAULT
#define LSPNGVERSION		PNG_LIBPNG_VER_STRING

typedef uint8_t byte;
//typedef struct { UCH r, g, b, a; } px;
typedef struct { byte a, b, g, r; } px;
typedef struct { IN width, height; px *pixels; } bitmap;

static bitmap *createbitmap(IN width, IN height) {
  bitmap *map = (bitmap *)malloc(sizeof(bitmap));
  map->width = width;
  map->height = height;
  map->pixels = (px *)calloc(sizeof(px), width * height);
  RT map;
}

static bitmap *createemptybitmap() {
  bitmap *map = (bitmap *)malloc(sizeof(bitmap));
  map->width = 0;
  map->height = 0;
  map->pixels = NULL;
  RT map;
}

static VD flipbitmaph(bitmap *map) {
  IN x = -1;
  IN xmax = map->width - 1;
  IN xhalf = xmax / 2; // w=4,5,6 xh=1,2,2
  WI (INC x LQ xhalf) {
    IN y = -1;
    WI (INC y LT map->height) {
      IN pxi = y * map->width + x;
      IN pxj = y * map->width + (xmax - x);
      px swap = map->pixels[pxj];
      map->pixels[pxj] = map->pixels[pxi];
      map->pixels[pxi] = swap;
    }
  }
}

static VD flipbitmapv(bitmap *map) {
  IN y = -1;
  IN ymax = map->height - 1;
  IN yhalf = ymax / 2; // y=4,5,6 yh=1,2,2
  WI (INC y LQ yhalf) {
    IN x = -1;
    WI (INC x LT map->width) {
      IN pxi = y * map->width + x;
      IN pxj = (ymax - y) * map->width + x;
      px swap = map->pixels[pxj];
      map->pixels[pxj] = map->pixels[pxi];
      map->pixels[pxi] = swap;
    }
  }
}

// to rotate a bitmap we need to copy it from -- shape to || shape

static VD destroybitmap(bitmap *map) {
  IF (map) {
    IF (map->pixels)
      { free(map->pixels); }
    free(map);
  }
}

static IN loadpngtobitmap(CS path, bitmap *map) {
  FILE *fp = OPENBLOCKFILE(path);
  IF (!fp) { RT -1; }
  CH header[LSPNGHEADERSIZE];
  fread(header, 1, LSPNGHEADERSIZE, fp);
  IF (png_sig_cmp(header, 0, LSPNGHEADERSIZE)) { RT -2; }
  png_structp readptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  IF (!readptr) { fclose(fp); RT -3; }
  png_infop rinfoptr = png_create_info_struct(readptr);
  IF (!rinfoptr)
    { png_destroy_read_struct(&readptr, NULL, NULL); fclose(fp); RT -4; }
  png_infop rendinfoptr = png_create_info_struct(readptr);
  IF (!rendinfoptr)
    { png_destroy_read_struct(&readptr, &rinfoptr, NULL); fclose(fp); RT -5; }
  IF (setjmp(png_jmpbuf(readptr)))
    { png_destroy_read_struct(&readptr, &rinfoptr, &rendinfoptr); fclose(fp); RT -6; }
  png_init_io(readptr, fp);
  png_set_sig_bytes(readptr, LSPNGHEADERSIZE);
  png_read_info(readptr, rinfoptr);
  map->width = png_get_image_width(readptr, rinfoptr);
  map->height = png_get_image_height(readptr, rinfoptr);
  IN bitdepth = png_get_bit_depth(readptr, rinfoptr);
MLOG1("BITDEPTH: %d ", bitdepth);
  IF (bitdepth == LSPNGWIDEBITDEPTH)
    { png_set_strip_16(readptr); }
  EF (bitdepth < LSPNGBITDEPTH)
    { png_set_packing(readptr); }
  IN colortype = png_get_color_type(readptr, rinfoptr);
  IF (colortype == PNG_COLOR_TYPE_GRAY || colortype == PNG_COLOR_TYPE_GRAY_ALPHA)
    { png_set_gray_to_rgb(readptr); }
  IF (colortype & PNG_COLOR_MASK_ALPHA)
    { png_set_strip_alpha(readptr); RLOG("MASKALPHA"); }
  png_read_update_info(readptr, rinfoptr);
  bitdepth = png_get_bit_depth(readptr, rinfoptr);
CLOG1("BITDEPTH2: %d ", bitdepth);
  colortype = png_get_color_type(readptr, rinfoptr);
  IF (bitdepth != LSPNGBITDEPTH || colortype != LSPNGCOLOURTYPE)
    { fprintf(stderr, "transform fail\n");
      png_destroy_read_struct(&readptr, &rinfoptr, &rendinfoptr); fclose(fp); RT -7; }
  IN channels = png_get_channels(readptr, rinfoptr);
YLOG1("CHANNELS: %d ", channels);
  IN rowbytes = png_get_rowbytes(readptr, rinfoptr);
  IF (channels != LSPNGRGBCHANNELS || (channels != sizeof(px)) ||
      (rowbytes != (map->width * channels)))
    { fprintf(stderr, "channel or size mismatch\n");
      fprintf(stdout, "channels: %d %d %d\n", channels, LSPNGRGBCHANNELS, sizeof(px));
      fprintf(stdout, "size: %d %d\n", rowbytes, map->width * channels);
      png_destroy_read_struct(&readptr, &rinfoptr, &rendinfoptr); fclose(fp); RT -8; }

  map->pixels = (px *)calloc(sizeof(px), map->width * map->height);

  png_bytep *rrowptrs = (png_bytep *)calloc(sizeof(png_bytep), map->height);
  IN rri = 0; WI (rri < map->height)
    { rrowptrs[rri] = (png_bytep)&map->pixels[rri * map->width]; rri++; }
//  png_set_rows(readptr, rinfoptr, &rrowptrs);
  png_read_image(readptr, rrowptrs);
  png_read_end(readptr, rendinfoptr);
  png_destroy_read_struct(&readptr, &rinfoptr, &rendinfoptr);
  free(rrowptrs);
  CLOSEBLOCKFILE(fp);
  RT 0;
}

static IN savebitmaptopng(bitmap *map, CS path) {
  IF (!map->pixels) { RT -1; }
  FILE *fp = SAVEBLOCKFILE(path);
  IF (!fp) { RT -2; }

  png_structp writeptr = png_create_write_struct(LSPNGVERSION, NULL, NULL, NULL);
  IF (!writeptr) { fclose(fp); RT -3; }
  png_infop winfoptr = png_create_info_struct(writeptr);
  IF (!winfoptr)
    { png_destroy_write_struct(&writeptr, NULL); fclose(fp); RT -4; }
  IF (setjmp(png_jmpbuf(writeptr)))
    { png_destroy_write_struct(&writeptr, &winfoptr); fclose(fp); RT -5; }

  png_init_io(writeptr, fp);
  png_set_compression_level(writeptr, LSPNGCOMPRESSLEVEL);
  png_set_IHDR(writeptr, winfoptr, map->width, map->height,
    LSPNGBITDEPTH, LSPNGCOLOURTYPE, LSPNGNOINTERLACE, LSPNGCOMPRESSTYPE, LSPNGFILTERTYPE);
  png_bytep *wrowptrs = (png_bytep *)calloc(sizeof(png_bytep), map->height);
  IN wri = 0; WI (wri < map->height)
    { wrowptrs[wri] = (png_bytep)&map->pixels[wri * map->width]; wri++; }
  png_write_info(writeptr, winfoptr);
  png_write_image(writeptr, wrowptrs);
  png_write_end(writeptr, winfoptr);
  free(wrowptrs);
  CLOSEBLOCKFILE(fp);
  RT 0;
}

static VGImage createimagefrombitmap(bitmap *map) {
  VGImageFormat rgba = VG_sABGR_8888;
  VGImage image = vgCreateImage(rgba, map->width, map->height, VG_IMAGE_QUALITY_BETTER);
  vgImageSubData(image, map->pixels, sizeof(px) * map->width, rgba, 0, 0, map->width, map->height);
  // vgSetPixels(x, y, image, 0, 0, width, height); vgDestroyImage(image);
  RT image;
}


static VGImage createimagefrompng(CS path) {
  bitmap *map = createemptybitmap();
  IF (loadpngtobitmap(path, map) EQ 0) {
    VGImage img = createimagefrombitmap(map);
    destroybitmap(map);
    RT img;
  } EL { destroybitmap(map); RT (VGImage)NULL; }
}

static VD drawimage(VGImage img, VGfloat x, VGfloat y, IN w, IN h) {
  vgSetPixels(x, y, img, 0, 0, w, h);
}

static CH saveimagetobitmap(VGImage img, bitmap *map) {
Rs("TODO");
}

static CH saveimagetopng(VGImage img, CS path) {
Rs("TODO");
}

static VD destroyimage(VGImage img) {
  vgDestroyImage(img);
}

//#define STREAMSRCDIR "/dev/shm/ramba"
#define STREAMSETID  "0"
#define STREAMEATID  "1"
CH savescreenshot(IN x, IN y, IN w, IN h, CS path) {
  bitmap *map = createbitmap(w, h);
  vgReadPixels(map->pixels, sizeof(px) * w, VG_sABGR_8888, x, y, w, h);
  flipbitmapv(map);
//FS cmd = OPENCMD("ls -lh /dev/shm/ramba/?*.png");
//WI (1) { IN ch = GETFCH(cmd); BKEQEOF(ch); Wc(ch); }
//CLOSECMD(cmd);
  FS png0 = OPENFILE(path);
  WI (png0) {
    CLOSEFILE(png0);
    Cxc(96); WAIT100MS;     // wait for receiver to remove frame
    png0 = OPENFILE(path);
  } // wait for failure (won't be open)
  savebitmaptopng(map, path);
//  VGImage img = createimagefrombitmap(map);
//  drawimage(img, 50, h / 2, w, h);
//  destroyimage(img);
  destroybitmap(map);
Mxc(96);
  RT 0;
}

//typedef struct _Login {
//  IN loginid;
//  CS username;
//  struct _Login *next;
//} Login;
//Login *allLogins = NULL;
typedef struct _Box {
  IN x, y, w, h;
  IN id;
  IN pathlen;
  CS pathstr;
//  struct _User *users;
//  struct _Box *prev;
  struct _Box *next;
} Box;

typedef struct _User {
  IN userid;
  CS username;
  Box *stream;
  IN streamx, streamy, streamw, streamh;
  CH needslogin;
  CH sessionstatus;
  CS sessioncode;
  FS sessionfile;
//  CH passchar; // consider Unicode entry
//  IN passcount;
//  CH passstate;
  LF gpslat, gpslong;
  LF mousex, mousey;
  LF fontsize;
  CS inputstr;
  FS inputfile;
//  Login *login;
  struct _User *next;
} User;
User *allusers = NULL;
Box *allstreams = NULL;

//Box screenstream = { 0, 0, 0, 0, 0, 0, STREAMSRCDIR "/-0.png", NULL, NULL };
CH update0screenstream(Box *stream) {
  Box *this = stream;
  CS tgtpathstr = (CS)malloc(sizeof(CH) * (this->pathlen + 1));
//  snprintf(tgtpathstr, this->pathlen + 1, "%s/=%d.png", STREAMSRCDIR, this->id);
  snprintf(tgtpathstr, this->pathlen + 1, "=%d.png", this->id);
  FS cache = OPENFILE(tgtpathstr);
  WI (cache) {
    CLOSEFILE(cache);
//    sched_yield();
Yc('.');
    cache = OPENFILE(tgtpathstr);
  } // stop when cache cannot be opened (indicating receiver has received and removed it).... ---- this is a heavy loop! hence yield!
//  savescreenshot(this->x, this->y, this->w, this->h, this->pathstr);
  FS zeroframe = WRITEFILE(this->pathstr);
  IF (!zeroframe) { RT 0; } // failure
  CLOSEFILE(zeroframe); // should be empty and existing
  IF (rename(this->pathstr, tgtpathstr) EQ 0)
    { Gs("0="); } EL { Rs("0=!!"); }
  UNMEM(tgtpathstr);
  RT 1; // success
}
CH updatescreenstream(Box *stream) {
//  Box *ss = &screenstream;
//  WI (ss->next NQNULL AND ss->id NQ id)
//    { ss = ss->next; }
//  IF (ss EQNULL) { RT 1; } // fail
  Box *this = stream; // ss;
// make sure receiver picked up the previous frame

  CS tgtpathstr = (CS)malloc(sizeof(CH) * (this->pathlen + 1));
//  snprintf(tgtpathstr, this->pathlen + 1, "%s/=%d.png", STREAMSRCDIR, this->id);
  snprintf(tgtpathstr, this->pathlen + 1, "=%d.png", this->id);
//  Mi(this->id);
//  FS cache = OPENFILE(this->pathstr); // only checks that the data was written, not read
//Cs("C");
  FS cache = OPENFILE(tgtpathstr);
  WI (cache) {
    CLOSEFILE(cache);
//    Mc(Adot); 
//    WAIT20MS; // 50fps wait limit
//    sched_yield();
Cc('c'); WAIT100MS;
//    cache = OPENFILE(this->pathstr);
    cache = OPENFILE(tgtpathstr);
  } // stop when cache cannot be opened (indicating receiver has received and removed it).... ---- this is a heavy loop! hence yield!
//Cs("NC");
//FS cmd = OPENCMD("ls -lh /dev/shm/ramba/?*.png");
//WI (1) { IN ch = GETFCH(cmd); BKEQEOF(ch); Wc(ch); }
//CLOSECMD(cmd);
//Y1(" NOCACHE %s ", this->pathstr);
//Ys("U");
  savescreenshot(this->x, this->y, this->w, this->h, this->pathstr); // need to check for failure to write pathstr ... checked below in a way
//Gs("S");
  IF (rename(this->pathstr, tgtpathstr) EQ 0)
    { Gs("-="); } EL { Rs("-=!!"); }
  UNMEM(tgtpathstr);
//cmd = OPENCMD("ls -lh /dev/shm/ramba/?*.png");
//WI (1) { IN ch = GETFCH(cmd); BKEQEOF(ch); Wc(ch); }
//CLOSECMD(cmd);
  RT 'y'; //1;
}

IN numstreamids = 0; // count how many streams
Box *savescreenstream(IN x, IN y, IN w, IN h) {
//  Box *ss = &screenstream;
//  IF (ss->pathlen EQ 0) // fix Box 0 (static str has size 0)
//    { ss->pathlen = snprintf(NULL, 0, ss->pathstr); }
//  WI (ss->next NQNULL)
//    { ss = ss->next; } // move to last item in list
//  ss->next = (Box *)malloc(sizeof(Box));
//  Box *this = ss->next;
//  this->prev = ss;
  Box *this = (Box *)malloc(sizeof(Box));
  IF (allstreams) {
    Box *ss = allstreams;
    WI (ss->next) { ss = ss->next; }
    ss->next = this;    // next (last) in list
  } EL { allstreams = this; } // first in list
  this->id = INC numstreamids;
  this->x = x;
  this->y = y;
  this->w = w;
  this->h = h;
//  this->id = ss->id + 1;
//  this->users = NULL;
//  this->pathlen = snprintf(NULL, 0, "%s/-%d.png", STREAMSRCDIR, this->id);
  this->pathlen = snprintf(NULL, 0, "-%d.png", this->id);
  this->pathstr = (CS)malloc(sizeof(CH) * (this->pathlen + 1));
//  snprintf(this->pathstr, this->pathlen + 1, "%s/-%d.png", STREAMSRCDIR, this->id);
  snprintf(this->pathstr, this->pathlen + 1, "-%d.png", this->id);
  IF ISFILE(this->pathstr) {
    IN val = unlink(this->pathstr); // rm -1.png in first screenshot of stream
//FS cmd = OPENCMD("ls");
//WI (1) { IN ch = GETFCH(cmd); BKEQEOF(ch); Wc(ch); }
//CLOSECMD(cmd);
//  Ys(this->pathstr);
//  Bi(val);
//  Ri(errno);
//  Gs(strerror(errno));
//  Ys("RM2");
    IF (remove(this->pathstr) NQ 0)
      { } // file is there but failed to delete ......... continue as though file can be ignored
//      { Wxc(97); } EL { Rxc(97); } // cache cleared
  } // EL { Y1("nofile %s", this->pathstr); }
//Ys("Connected...");
  savescreenshot(this->x, this->y, this->w, this->h, this->pathstr);
//Gs("...SAVED");
//  CS tempstr = this->pathstr;
  CS tgtpathstr = (CS)malloc(sizeof(CH) * (this->pathlen + 1));
//  snprintf(tgtpathstr, this->pathlen + 1, "%s/=%d.png", STREAMSRCDIR, this->id);
  snprintf(tgtpathstr, this->pathlen + 1, "=%d.png", this->id);
//  this->pathstr = (CS)malloc(sizeof(CH) * (this->pathlen + 1));
//  snprintf(this->pathstr, this->pathlen + 1, "%s/=%d.png", STREAMSRCDIR, this->id);

//FS cmd = OPENCMD("ls -lh /dev/shm/ramba/?*.png");
//WI (1) { IN ch = GETFCH(cmd); BKEQEOF(ch); Wc(ch); }
//CLOSECMD(cmd);

  IF (rename(this->pathstr, tgtpathstr) EQ 0)
    { } // Gs("-="); } EL { Rs("-=!!"); } // if this fails, no file is available ... should probably return NULL
  UNMEM(tgtpathstr);
//cmd = OPENCMD("ls -lh /dev/shm/ramba/?*.png");
//WI (1) { IN ch = GETFCH(cmd); BKEQEOF(ch); Wc(ch); }
//CLOSECMD(cmd);

//  UNMEM(tempstr);
//  RT this->id;
  RT this;
}
//IN newstreamid = 1; // first new stream will be 2 (1 is full frame)
//IN newpasswordstream() {
  
//}



/*
LF gpsminylat = { -90, 0 };
LF gpsmaxylat = {  90, 0 };
LF gpsminlong = { -180, 0 };
LF gpsmaxlong = {  180, 0 };

#define USERBOXW  100
#define USERBOXH  20

IN numuserids = 0; // holds a count of how many ids have been created
User *addscreenstreamuser(Box *stream, CS username, CS usertime) { //, IN streamid) {
// userparams -> username, usertime, userpassword (later allow char-by-char)
  User *this = (User *)MEM(sizeof(User));
  this->userid = INC numuserids; // ID 1, 2, 3, 4 etc
  this->username = username;
//  ssu->streamid = INC newstreamid; // newpasswordstream();
// setstreamregion(ssu->streamid, x, y, w, h);
  this->stream = stream;
  this->streamx = stream->w - USERBOXW;
  this->streamy = stream->h - (USERBOXH * this->userid);
  this->streamw = USERBOXW;
  this->streamh = USERBOXH;
// calculate session code from input
//  this->sessioncode = "ABCDEFG"; // <--- user will be POSTing their time
//  this->sessionstatus = AP; // need to log in
// if password is correct, login else trigger clear/retry
  this->sessionstatus = AJ; // skip password checking for now
  this->sessioncode = CHMEM(10);
  this->sessioncode[0] = '$';
  token8(usertime, AT1(this->sessioncode));
  // use pwd to check we are in /dev/shm/ramba
  this->sessionfile = WRITEFILE(this->sessioncode); // file stays open
  fprintf(this->sessionfile, "MODE=%c", this->sessionstatus);
  fprintf(this->sessionfile, " USERNAME=%s\n", this->username);
// where to put the password buffer ... better to receive all at once
// if user included GPS and/or FILES -- process them
// consider special key file upload process
// tag gps location
// sessionfile:
// MODE=NPJSZL (New Pass Join Stream pauZe Leave) NAME=username
// GPS: LAT=123.456 LONG=12.345
// FILES: thisone.png thatone.png
//  this->needslogin = 1; // becomes 0 if password matches on enter
  // ^ sessionstatus = P implies it
//  this->passcount = 0; // abcdef => digits = 1-6
//  this->passstate = 1; // becomes 0 if any key does not match
// XXXXXB bad char -> state = AD Denied, XXXXXX
// how to pass passkeys via streamframe to here ... need non-stdin FD
// hash = username:passcode:when:salt -> code stored should be a login salt to point to profile info
// if you forget your passcode, what happens to profile info? it should be forgotten
// if you want to keep track of it, you can export it to / import it from email
//-----------------------------------
// Property: Value
// Property2: Value2
//----------------------------------- 5 dashes or more = section break / object delimeter
// File: "/Space Path/file.txt"
//-----------------------------------
// when needslogin is 0, streamid should become 1 or something else new (eg TL)
//  this->needslogin = 0; // LOGIN SKIPPED
//  this->streamid = 1; // FULLSCREEN ASSUMED
//  ssu->streamid = streamid; // starts in its own password stream, then gets the main stream once logged in
  this->gpslat = (LF){ 99, 999999 }; // latitude N-S
//  this->gpsxlat = { 99, 999999 };
  this->gpslong = (LF){ 99, 999999 }; // longitude E-W
  
//  ssu->gpsylatmin = {
//, gpsmaxx
  // gpsminy, gpsmaxy
  this->mousex = (LF){ 0, 0 };
  this->mousey = (LF){ 0, 0 };
  this->fontsize = (LF){ 0, 0 };
  this->next = NULL;
  IF (allusers) {
    User *su = allusers;
    WI (su->next) { su = su->next; }
    su->next = this;  // next (last) in list
  } EL { allusers = this; } // first in list

  // list users
  User *s = allusers;
  WI (s) {
    Ys(s->username);
    Mi(s->userid);
    BKEQNULL(s->next); // stop on last
    s = s->next;
  }
//  IF (s) {
//    ssu->id = s + 1;
//  } EL { ssu->id = 1; }
  // RT userid ... automatic ? retrieved from name in table .... need user table !
//  RT ssu->id;
  RT this;
}
*/
// frame writes to fifomux, fifomux writes to drawframe
// fifomux checks user fifos in a loop -- if it sees a [, it keeps
// waiting until it sees a ].... 

// (username: blaaaaa)

/*
IN addscreenstreamlogin(CS username) {
  // for quick login this should return a direct stream ID .... 
  // for password login this should return a password stream id
  IN userid = addscreenstreamuser(username, 1);
} all users have a corresponding login section on the screen */



// currently deprecated by /tau/web/http/index.html
CH savescreenreader(CS filename) {
  IF (!filename) { filename = "screenreader.html"; }
//  IF (!filename) { filename = STREAMSRCDIR "/screenreader.html"; }
  FS sr = WRITEFILE(filename);
  IF (!sr) { Rs("Failed to write screen reader."); RT -1; }
  fprintf(sr, "<html><head>\n");
  fprintf(sr, "  <title>hostname-0 screen reader</title>\n");
  fprintf(sr, "  <style id=\"imgzoom\" type=\"text/css\">\n");
  fprintf(sr, "    img { zoom: 0.5; }\n");
  fprintf(sr, "  </style>\n");
  fprintf(sr, "  <script type=\"text/javascript\">\n");
  fprintf(sr, "    function id(id) { return document.getElementById(id); }\n");
  fprintf(sr, "    function imgzoom() {\n");
  fprintf(sr, "      imgpct = id('imgpct').value;\n");
  fprintf(sr, "      imgmag = id('imgmag').value;\n");
  fprintf(sr, "      imgval = 'img { zoom: ' + (imgpct * 0.01 * imgmag) + '; }';\n");
  fprintf(sr, "      id('imgpctval').innerHTML = '' + imgpct;\n");
  fprintf(sr, "      id('imgmagval').innerHTML = '' + imgmag;\n");
  fprintf(sr, "      id('imgzoom').innerHTML = \"    \" + imgval + \"\\n\";\n");
  fprintf(sr, "    }\n");
  fprintf(sr, "  </script>\n");
  fprintf(sr, "</head><body>\n");
  fprintf(sr, "  <center><input id=\"imgpct\" type=\"range\" min=\"1\" max=\"50\" value=\"50\" oninput=\"imgzoom();\" onchange=\"imgzoom();\" /><span id=\"imgpctval\">50</span><b>%</b> \n");
  fprintf(sr, "  <input id=\"imgmag\" type=\"range\" min=\"1\" max=\"2\" step=\"0.01\" value=\"1\" oninput=\"imgzoom();\" onchange=\"imgzoom();\" /><span id=\"imgmagval\">1</span><b>x</b></center>\n");
  fprintf(sr, "  <br /><center><img id=\"img\" src=\"./hostname-0.png?\" /></center>\n");
  fprintf(sr, "  <script type=\"text/javascript\">\n");
  fprintf(sr, "    function updateimg() {\n");
  fprintf(sr, "      src = id('img').src.substr(0, id('img').src.indexOf('?'));\n");
  fprintf(sr, "      id('img').src = src + '?' + new Date().getTime();\n");
  fprintf(sr, "    }\n");
  fprintf(sr, "    imgtimer = setInterval(updateimg, 1000);\n");
  fprintf(sr, "  </script>\n");
  fprintf(sr, "</body></html>\n");
  CLOSEFILE(sr);
  RT 0;
}

