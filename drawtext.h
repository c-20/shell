#include "drawchar.h"
typedef struct {
  IN screenwidth, screenheight;
  IN windowx, windowy;
  IN windowwidth, windowheight;
  DISPMANX_ELEMENT_HANDLE_T element;
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
} STATE_T;
static STATE_T _state, *state = &_state;
typedef struct {
  IN x, y, w, h;
} initsize_;
initsize_ initsize;
VD initSize(IN x, IN y, IN w, IN h) {
  initsize.x = x;
  initsize.y = y;
  initsize.w = w;
  initsize.h = h;
}
VD setwindow(IN x, IN y, VC_RECT_T *srcrect, VC_RECT_T *dstrect) {
  uint32_t dx, dy, w, h, sx, sy;
  dx = x; dy = y;
  sx = 0; sy = 0;
  w = state->windowwidth;
  h = state->windowheight;
  state->windowx = x;
  state->windowy = y;
  vc_dispmanx_rect_set(dstrect, dx, dy, w, h);
  vc_dispmanx_rect_set(srcrect, sx, sy, w, h);
//  vc_dispmanx_rect_set(srcrect, sx << 16, sy << 16, w << 16, h << 16); // wut?
}
VD oglinit(STATE_T *state, IN layer, CUCH opacity) {
  static EGL_DISPMANX_WINDOW_T window;
  DISPMANX_ELEMENT_HANDLE_T element;
  DISPMANX_DISPLAY_HANDLE_T display;
  DISPMANX_UPDATE_HANDLE_T update;
  VC_RECT_T dstrect, srcrect;
  ST VC_DISPMANX_ALPHA_T alpha =
    { DISPMANX_FLAGS_ALPHA_FIXED_NON_ZERO, 255, 0 };
  alpha.opacity = (IN)opacity;
//    { DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 255, 0 };
  ST CN EGLint attributes[] =
    { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
  EGLint numconfig;
  EGLConfig config;
  EGLBoolean result;
  IN success;
  state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  assert(state->display NQ EGL_NO_DISPLAY);
  result = eglInitialize(state->display, NULL, NULL);
  assert(EGL_FALSE NQ result);
  eglBindAPI(EGL_OPENVG_API);
  result = eglChooseConfig(state->display, attributes, &config, 1, &numconfig);
  assert(EGL_FALSE NQ result);
  state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, NULL);
  assert(state->context NQ EGL_NO_CONTEXT);
  success = graphics_get_display_size(0, &state->screenwidth, &state->screenheight);
  assert(success GQ 0);
  setwindow(state->windowx, state->windowy, &srcrect, &dstrect);
  display = vc_dispmanx_display_open(0); // LCD
  update = vc_dispmanx_update_start(0);
  element = vc_dispmanx_element_add(update, display, layer, &dstrect, 0, &srcrect,
                                    DISPMANX_PROTECTION_NONE, &alpha, 0, 0);
  state->element = element;
  window.element = element;
  window.width = state->windowwidth;
  window.height = state->windowheight;
  vc_dispmanx_update_submit_sync(update);
  state->surface = eglCreateWindowSurface(state->display, config, &window, NULL);
  assert(state->surface NQ EGL_NO_SURFACE);
  result = eglSurfaceAttrib(state->display, state->surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
  assert(EGL_FALSE NQ result);
  result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
  assert(EGL_FALSE NQ result);
}
VD init(IN layer, INP w, INP h, VGfloat opacity) {
  bcm_host_init();
  memset(state, 0, sizeof(*state));
  state->windowx = initsize.x;
  state->windowy = initsize.y;
  state->windowwidth = initsize.w;
  state->windowheight = initsize.h;
  oglinit(state, layer, opacity);
  *w = state->windowwidth;
  *h = state->windowheight;
}
VD finish() {
  eglSwapBuffers(state->display, state->surface);
  eglMakeCurrent(state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(state->display, state->surface);
  eglDestroyContext(state->display, state->context);
  eglTerminate(state->display);
}
VD opacity(IN a) {
  IF (a GT 255) { a = 255; }
  IF (a LT   0) { a =   0; }
  DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
  vc_dispmanx_element_change_attributes(update, state->element, 1 << 1, 0, a, 0, 0, 0, DISPMANX_NO_ROTATE);
  vc_dispmanx_update_submit_sync(update);
}
VD move(IN x, IN y) {
  VC_RECT_T srcrect, dstrect;
  setwindow(x, y, &srcrect, &dstrect);
  DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
  vc_dispmanx_element_change_attributes(update, state->element, 0, 0, 0, &dstrect, &srcrect, 0, DISPMANX_NO_ROTATE);
  vc_dispmanx_update_submit_sync(update);
}
VD start() {
  vgLoadIdentity();
}
VD end() {
  assert(vgGetError() == VG_NO_ERROR);
  eglSwapBuffers(state->display, state->surface);
  assert(eglGetError() == EGL_SUCCESS);
}
VD setbackground(IN r, IN g, IN b, VGfloat a) {
  VGfloat colour[] = { (VGfloat)r / 255.0f, (VGfloat)g / 255.0f, (VGfloat)b / 255.0f, a };
  vgSetfv(VG_CLEAR_COLOR, 4, colour);
  vgClear(state->windowx, state->windowy, state->windowwidth, state->windowheight);
}
VD setfill(IN r, IN g, IN b, VGfloat a) {
  VGfloat colour[] = { (VGfloat)r / 255.0f, (VGfloat)g / 255.0f, (VGfloat)b / 255.0f, a };
  VGPaint fp = vgCreatePaint();
  vgSetParameteri(fp, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
  vgSetParameterfv(fp, VG_PAINT_COLOR, 4, colour);
  vgSetPaint(fp, VG_FILL_PATH);
  vgDestroyPaint(fp);
}
VD setstroke(IN r, IN g, IN b, VGfloat a) {
  VGfloat colour[] = { (VGfloat)r / 255.0f, (VGfloat)g / 255.0f, (VGfloat)b / 255.0f, a };
  VGPaint sp = vgCreatePaint();
  vgSetParameteri(sp, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
  vgSetParameterfv(sp, VG_PAINT_COLOR, 4, colour);
  vgSetPaint(sp, VG_STROKE_PATH);
  vgDestroyPaint(sp);
}
VD setstrokefill(CH sv, RGB sc, FP sa, CH fv, RGB fc, FP fa) { // colour.rgb alpha0-1opacity       IN r, IN g, IN b, VGfloat a) {
  VGfloat stroke4[] = { sc.r / 255.0f, sc.g / 255.0f, sc.b / 255.0f, sa };
  VGfloat fill4[] = { fc.r / 255.0f, fc.g / 255.0f, fc.b / 255.0f, fa };
  VGPaint sfp = vgCreatePaint();
  vgSetParameteri(sfp, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
  IF (sv) {
    vgSetParameterfv(sfp, VG_PAINT_COLOR, 4, stroke4);
    vgSetPaint(sfp, VG_STROKE_PATH);
  }
  IF (fv) {
    vgSetParameterfv(sfp, VG_PAINT_COLOR, 4, fill4);
    vgSetPaint(sfp, VG_FILL_PATH);
  }
  vgDestroyPaint(sfp);
}
VD setstrokewidth(FP w) {
  vgSetf(VG_STROKE_LINE_WIDTH, w);
  vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_BUTT);
  vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_MITER);
}
VGPath newpath() {
  RT vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f,
                  0.0f, 0, 0, VG_PATH_CAPABILITY_APPEND_TO);
}
VD drawroundrect(VGfloat x, VGfloat y, VGfloat w, VGfloat h, VGfloat rw, VGfloat rh) {
  VGPath path = newpath();
  vguRoundRect(path, x, y, w, h, rw, rh);
  vgDrawPath(path, VG_FILL_PATH | VG_STROKE_PATH);
  vgDestroyPath(path);
}
VD rgbaroundrect(XYWH box, XY curve, CH sv, RGB sc, FP sa, FP sw, CH fv, RGB fc, FP fa) {
  setstrokefill(sv, sc, sa, fv, fc, fa);
  setstrokewidth(sw);
  VGPath path = newpath();
  vguRoundRect(path, box.x, box.y, box.w, box.h, curve.x, curve.y);
  IF (sv AND fv) {
    vgDrawPath(path, VG_FILL_PATH | VG_STROKE_PATH);
  } EF (sv) {
    vgDrawPath(path, VG_STROKE_PATH);
  } EF (fv) {
    vgDrawPath(path, VG_FILL_PATH);
  }
  vgDestroyPath(path);
}
VD rgbatextlines(CS fontpath, IN fontsize, CS text, XYWH *box, CH visible, RGB colour, FP opacity) {
  IF (visible) { // draw lines, recalc box->w,h
    setfill(colour.r, colour.g, colour.b, opacity);
    FP boxw = 0.0f;
    box->h = (IN)drawlinesw(fontpath, fontsize, box->x, box->y, text, &boxw);
//    box->w = (IN)(boxw + 1.0f); // add 1 in case of part-pixels (could use ceil)
    box->w = (boxw GT (IN)boxw) ? (IN)boxw + 1 : boxw; // instead of math.ceil()
  } EL { // recalculate box width and height only
    FP boxw = 0.0f;
    FP boxh = 0.0f; // was NULL fontpath..
    box->h = drawlineswh(fontpath, fontsize, box->x, box->y, text, &boxw, &boxh);
//    box->w = (IN)(boxw + 1.0f); // add 1 in case of part-pixels (could use ceil)
    box->w = (boxw GT (IN)boxw) ? (IN)boxw + 1 : boxw; // instead of math.ceil()
    // box->h and boxh should match
  }
}
VD rgbatextpoint(CS fontpath, IN fontsize, CS label, XYWH *box, CH visible, RGB colour, FP opacity) {
  IF (visible) { // draw label centred at box x,y, recalc box w,h (should have been set) - could warn on mismatch
    setfill(colour.r, colour.g, colour.b, opacity);
    FP boxw = 0.0f;
    FP halfw = box->w / 2;
    FP halfh = box->h / 2;
    box->h = (IN)drawlinesw(fontpath, fontsize, box->x - halfw, box->y + halfh, label, &boxw);
    box->w = (boxw GT (IN)boxw) ? (IN)boxw + 1 : boxw; // instead of math.ceil()
  } EL {
    FP boxw = 0.0f;
    FP boxh = 0.0f; // rly NULL fontpath?
    box->h = (IN)drawlineswh(fontpath, fontsize, box->x, box->y, label, &boxw, &boxh);
    box->w = (boxw GT (IN)boxw) ? (IN)boxw + 1 : boxw; // instead of math.ceil()
 }    
}

VD drawellipse(VGfloat x, VGfloat y, VGfloat w, VGfloat h) {
  VGPath path = newpath();
  vguEllipse(path, x, y, w, h);
  vgDrawPath(path, VG_FILL_PATH | VG_STROKE_PATH);
  vgDestroyPath(path);
}
VD drawcircle(VGfloat x, VGfloat y, VGfloat w) {
  drawellipse(x, y, w, w);
}
