/* See LICENSE file for copyright and license details.
 *
 * To understand svkbd, start reading main().
 */
#define _XOPEN_SOURCE 700
#include <locale.h>
#include <langinfo.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/XTest.h>

/* macros */
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define LENGTH(x)       (sizeof x / sizeof x[0])

#define KEYGAP_X 4
#define KEYGAP_Y KEYGAP_X

/* enums */
enum { ColFG, ColBG, ColLast };
enum { NetWMWindowType, NetLast };

/* typedefs */
typedef unsigned int uint;

typedef struct {
	XftColor back[ColLast];
	XftColor norm[ColLast];
	XftColor press[ColLast];
	XftColor high[ColLast];
	XftColor mod[ColLast];
	XftColor cur[ColLast];
	XftColor pg[ColLast];
	XftColor ret[ColLast];
	XftColor esc[ColLast];
	XftColor del[ColLast];
	XftColor spc[ColLast];
	XftColor close[ColLast];

	Drawable drawable;
	GC gc;
	XftDraw *xftdraw;
	struct {
		int ascent;
		int descent;
		int height;
		XftFont *xftfont;
	} font;
} DC; /* draw context */

typedef struct {
	char *label;
	KeySym keysym;
	uint width;
	int x, y, w, h;
	Bool pressed;
	Bool highlighted;
} Key;

typedef struct {
	KeySym mod;
	uint button;
} Buttonmod;

struct Theme;

/* function declarations */
static void motionnotify(XEvent *e);
static void buttonpress(XEvent *e);
static void buttonrelease(XEvent *e);
static void cleanup(void);
static void configurenotify(XEvent *e);
static void countrows(void);
static void die(const char *errstr, ...);
static void drawkeyboard(void);
static void drawkey(Key *k);
static void expose(XEvent *e);
static Key *findkey(int x, int y);
static XftColor getcolor(const char *colstr);
static void initfont(const char *fontstr);
static void leavenotify(XEvent *e);
static void press(Key *k, KeySym mod);
static void run(void);
static void setup(void);
static int textnw(const char *text, uint len);
static void unpress(Key *k, KeySym mod);
static void updatekeys(void);
static void initlocale(void);
static void initxkb(void);
static int applytheme(const char *name);
static const char **theme_keys(const struct Theme *t);
static int layout_index(const char *name);
static int layout_for_group(int group, const char *variant);
static void releaseall(void);
static void select_layout(int idx, int sync);
static void statechanged(XkbStateNotifyEvent *e);
static void switch_locale(void);

/* variables */
static int screen;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ButtonRelease] = buttonrelease,
	[ConfigureNotify] = configurenotify,
	[Expose] = expose,
	[LeaveNotify] = leavenotify,
	[MotionNotify] = motionnotify
};
static Atom netatom[NetLast];
static Display *dpy;
static DC dc;
static Window root, win;
static Bool running = True, isdock = False, fixed_layout = False;
static KeySym pressedmod = 0;
static int rows = 0, ww = 0, wh = 0, wx = 0, wy = 0;
static char *name = "svkbd";
static Key keys_buffer[512];
static Key *keys = keys_buffer;
static int nkeys;
static Buttonmod *buttonmods;
static int nbuttonmods;
static const char *start_layout = NULL;
static char layoutlabel[16];
static int xkb_available, xkb_event_base, xkb_opcode, xkb_error_base;
static int xkb_num_groups = 1, xkb_group = 0;

Bool ispressing = False;
static Key *pressedkey;

/* configuration, allows nested code to access above variables */
#include "config.h"
#include "themes.h"
#include "layouts.h"

static const Layout *current;
static int current_index = -1;
static const struct Theme *current_theme;

static int
applytheme(const char *name)
{
	int i;

	if (!name)
		return 0;
	for (i = 0; themes[i].name; i++)
		if (strcmp(themes[i].name, name) == 0) {
			current_theme = &themes[i];
			return 1;
		}
	return 0;
}

static const char **
theme_keys(const struct Theme *t)
{
	static const char *k[24];

	if (t->keys[0])
		return (const char **)t->keys;

	k[0] = t->color[7];  k[1] = t->color[0];
	k[2] = t->bg;        k[3] = t->fg;
	k[4] = t->cursor;    k[5] = t->bg;
	k[6] = t->color[3];  k[7] = t->color[1];
	k[8] = t->color[2];  k[9] = t->color[10];
	k[10] = t->color[4]; k[11] = t->color[6];
	k[12] = t->color[1]; k[13] = t->color[3];
	k[14] = t->color[0]; k[15] = t->color[9];
	k[16] = t->color[3]; k[17] = t->color[0];
	k[18] = t->color[8]; k[19] = t->color[11];
	k[20] = t->color[4]; k[21] = t->color[6];
	k[22] = t->color[1]; k[23] = t->color[3];
	return k;
}

static void
initlocale(void)
{
	static const char *cand[] = {
		"", "C.UTF-8", "C.utf8", "en_US.UTF-8", "en_US.utf8", NULL
	};
	const char *cs;
	int i;

	for (i = 0; cand[i]; i++) {
		if (!setlocale(LC_CTYPE, cand[i]) || !XSupportsLocale())
			continue;
		cs = nl_langinfo(CODESET);
		if (cs && (strcmp(cs, "UTF-8") == 0 || strcmp(cs, "UTF8") == 0))
			return;
	}
	setlocale(LC_CTYPE, "");
	fprintf(stderr, "svkbd: warning: no UTF-8 locale; labels may render incorrectly\n");
}

static int
layout_index(const char *name)
{
	int i;

	if (!name)
		return -1;
	for (i = 0; i < (int)NLAYOUTS; i++)
		if (strcmp(layouts[i].name, name) == 0)
			return i;
	return -1;
}

static int
layout_for_group(int group, const char *variant)
{
	int i;

	if (!variant)
		variant = "";
	for (i = 0; i < (int)NLAYOUTS; i++)
		if (layouts[i].group == group && strcmp(layouts[i].variant, variant) == 0)
			return i;
	for (i = 0; i < (int)NLAYOUTS; i++)
		if (layouts[i].group == group)
			return i;
	return -1;
}

static void
releaseall(void)
{
	int i;
	KeyCode kc;

	pressedkey = NULL;
	if (!dpy)
		return;
	for (i = 0; i < nkeys; i++) {
		if (keys[i].pressed) {
			kc = XKeysymToKeycode(dpy, keys[i].keysym);
			if (kc)
				XTestFakeKeyEvent(dpy, kc, False, 0);
			keys[i].pressed = 0;
		}
	}
	if (pressedmod) {
		kc = XKeysymToKeycode(dpy, pressedmod);
		if (kc)
			XTestFakeKeyEvent(dpy, kc, False, 0);
		pressedmod = 0;
	}
	XFlush(dpy);
}

static void
select_layout(int idx, int sync)
{
	const Layout *l;
	int i, space = -1, insert;

	if (idx < 0 || idx >= (int)NLAYOUTS)
		return;
	if (idx == current_index && current && current->group == layouts[idx].group)
		return;
	releaseall();
	l = &layouts[idx];
	current = l;
	current_index = idx;
	nkeys = l->nkeys;
	if (nkeys > (int)LENGTH(keys_buffer))
		nkeys = (int)LENGTH(keys_buffer);
	memcpy(keys_buffer, l->keys, nkeys * sizeof(Key));
	buttonmods = l->buttonmods;
	nbuttonmods = l->nbuttonmods;
	for (i = 0; i < nkeys; i++) {
		keys_buffer[i].pressed = 0;
		keys_buffer[i].highlighted = 0;
		if (keys_buffer[i].keysym == XK_space) {
			space = i;
			break;
		}
	}
	if (xkb_num_groups >= 2) {
		int target = (current->group + 1) % xkb_num_groups;
		int tidx = layout_for_group(target, current->variant);
		snprintf(layoutlabel, sizeof layoutlabel, "%s",
		         tidx >= 0 ? layouts[tidx].shortname : "??");
	} else {
		snprintf(layoutlabel, sizeof layoutlabel, "%s", current->shortname);
	}
	insert = !fixed_layout && layoutbutton && xkb_num_groups >= 2 &&
	         space >= 0 && nkeys < (int)LENGTH(keys_buffer);
	if (insert) {
		if (keys_buffer[space].width > 1)
			keys_buffer[space].width--;
		memmove(&keys_buffer[space + 2], &keys_buffer[space + 1],
		        (nkeys - space - 1) * sizeof(Key));
		keys_buffer[space + 1].label = layoutlabel;
		keys_buffer[space + 1].keysym = XK_ISO_Next_Group;
		keys_buffer[space + 1].width = 1;
		keys_buffer[space + 1].x = 0;
		keys_buffer[space + 1].y = 0;
		keys_buffer[space + 1].w = 0;
		keys_buffer[space + 1].h = 0;
		keys_buffer[space + 1].pressed = 0;
		keys_buffer[space + 1].highlighted = 0;
		nkeys++;
	}
	countrows();
	if (win) {
		updatekeys();
		drawkeyboard();
	}
	if (sync && !fixed_layout && xkb_available && current->group < xkb_num_groups) {
		XkbLockGroup(dpy, XkbUseCoreKbd, current->group);
		xkb_group = current->group;
	}
}

static void
switch_locale(void)
{
	int target, idx;

	if (fixed_layout || !xkb_available || xkb_num_groups < 2 || !current)
		return;
	target = (current->group + 1) % xkb_num_groups;
	idx = layout_for_group(target, current->variant);
	if (idx < 0)
		idx = layout_for_group(target, "");
	if (idx >= 0)
		select_layout(idx, 1);
}

static void
initxkb(void)
{
	int major = XkbMajorVersion, minor = XkbMinorVersion;
	XkbStateRec state;
	XkbDescPtr xkb;

	xkb_available = 0;
	xkb_num_groups = 1;
	xkb_group = 0;
	if (!XkbQueryExtension(dpy, &xkb_opcode, &xkb_event_base,
	                       &xkb_error_base, &major, &minor))
		return;
	xkb_available = 1;
	XkbSelectEvents(dpy, XkbUseCoreKbd, XkbStateNotifyMask, XkbStateNotifyMask);
	if (XkbGetState(dpy, XkbUseCoreKbd, &state) == Success)
		xkb_group = state.group;
	xkb = XkbGetMap(dpy, 0, XkbUseCoreKbd);
	if (xkb) {
		if (XkbGetControls(dpy, XkbAllControlsMask, xkb) == Success && xkb->ctrls)
			xkb_num_groups = xkb->ctrls->num_groups;
		XkbFreeKeyboard(xkb, 0, True);
	}
	if (xkb_num_groups < 1)
		xkb_num_groups = 1;
	if (xkb_group < 0 || xkb_group >= xkb_num_groups)
		xkb_group = 0;
}

static void
statechanged(XkbStateNotifyEvent *e)
{
	int idx;

	if (!e || !(e->changed & XkbGroupStateMask))
		return;
	if (e->group < 0 || e->group >= xkb_num_groups)
		return;
	xkb_group = e->group;
	if (fixed_layout)
		return;
	if (!current || current->group == e->group)
		return;
	idx = layout_for_group(e->group, current->variant);
	if (idx < 0)
		idx = layout_for_group(e->group, "");
	if (idx >= 0 && idx != current_index)
		select_layout(idx, 0);
}

void
motionnotify(XEvent *e)
{
	XPointerMovedEvent *ev = &e->xmotion;
	int i;

	for(i = 0; i < nkeys; i++) {
		if(keys[i].keysym && ev->x > keys[i].x
				&& ev->x < keys[i].x + keys[i].w
				&& ev->y > keys[i].y
				&& ev->y < keys[i].y + keys[i].h) {
			if(keys[i].highlighted != True) {
				if(ispressing) {
					keys[i].pressed = True;
				} else {
					keys[i].highlighted = True;
				}
				drawkey(&keys[i]);
			}
			continue;
		}

		if(!IsModifierKey(keys[i].keysym) && keys[i].pressed == True) {
			unpress(&keys[i], 0);

			drawkey(&keys[i]);
		}
		if(keys[i].highlighted == True) {
			keys[i].highlighted = False;
			drawkey(&keys[i]);
		}
	}
}

void
buttonpress(XEvent *e) {
	int i;
	XButtonPressedEvent *ev = &e->xbutton;
	Key *k;
	KeySym mod = 0;

	ispressing = True;
	XGrabPointer(dpy, win, True, ButtonReleaseMask|PointerMotionMask,
	             GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

	for(i = 0; i < nbuttonmods; i++) {
		if(ev->button == buttonmods[i].button) {
			mod = buttonmods[i].mod;
			break;
		}
	}
	if((k = findkey(ev->x, ev->y)))
		press(k, mod);
}

void
buttonrelease(XEvent *e) {
	int i;
	XButtonPressedEvent *ev = &e->xbutton;
	KeySym mod = 0;

	ispressing = False;

	for(i = 0; i < nbuttonmods; i++) {
		if(ev->button == buttonmods[i].button) {
			mod = buttonmods[i].mod;
			break;
		}
	}

	unpress(NULL, mod);
	XUngrabPointer(dpy, CurrentTime);
}

void
cleanup(void) {
	int i;

	if(dc.xftdraw)
		XftDrawDestroy(dc.xftdraw);
	if(dc.font.xftfont)
		XftFontClose(dpy, dc.font.xftfont);
#define FREECOLORS(a) \
	for(i = 0; i < ColLast; i++) \
		XftColorFree(dpy, DefaultVisual(dpy, screen), \
		             DefaultColormap(dpy, screen), &(a)[i])
	FREECOLORS(dc.back);
	FREECOLORS(dc.norm);
	FREECOLORS(dc.press);
	FREECOLORS(dc.high);
	FREECOLORS(dc.mod);
	FREECOLORS(dc.cur);
	FREECOLORS(dc.pg);
	FREECOLORS(dc.ret);
	FREECOLORS(dc.esc);
	FREECOLORS(dc.del);
	FREECOLORS(dc.spc);
	FREECOLORS(dc.close);
#undef FREECOLORS
	XFreePixmap(dpy, dc.drawable);
	XFreeGC(dpy, dc.gc);

	XDestroyWindow(dpy, win);

	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void
configurenotify(XEvent *e) {
	XConfigureEvent *ev = &e->xconfigure;

	if(ev->window == win && (ev->width != ww || ev->height != wh)) {
		ww = ev->width;
		wh = ev->height;
		if(dc.xftdraw)
			XftDrawDestroy(dc.xftdraw);
		XFreePixmap(dpy, dc.drawable);
		dc.drawable = XCreatePixmap(dpy, root, ww, wh,
				DefaultDepth(dpy, screen));
		dc.xftdraw = XftDrawCreate(dpy, dc.drawable,
		                           DefaultVisual(dpy, screen),
		                           DefaultColormap(dpy, screen));
		if(!dc.xftdraw)
			die("error, cannot create Xft draw context\n");
		updatekeys();
		drawkeyboard();
	}
}

void
countrows(void) {
	int i = 0;

	for(i = 0, rows = 1; i < nkeys; i++) {
		if(keys[i].keysym == 0)
			rows++;
	}
}

void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
drawkeyboard(void) {
	int i;

	XSetForeground(dpy, dc.gc, dc.back[ColFG].pixel);
	XSetBackground(dpy, dc.gc, dc.back[ColBG].pixel);
	XFillRectangle(dpy, win, dc.gc, 0, 0, ww, wh);

	for(i = 0; i < nkeys; i++) {
		if(keys[i].keysym != 0)
			drawkey(&keys[i]);
	}
	XSync(dpy, False);

}

void
drawkey(Key *k) {
	int x, y, h, len;
	XRectangle r = { k->x, k->y, k->w, k->h};
	const char *l;
	XftColor *col;

	if(k->pressed)
		col = dc.press;
	else if(k->highlighted)
		col = dc.high;
	else
		if(!IsModifierKey(k->keysym))
		{
		    if(k->keysym == XK_Return)
			col = dc.ret;
		    else if(XK_Page_Up == k->keysym || XK_Page_Down == k->keysym)
			col = dc.pg;
		    else if(XK_Up == k->keysym || XK_Down == k->keysym || XK_Left == k->keysym || XK_Right == k->keysym)
			col = dc.cur;
		    else if(XK_BackSpace == k->keysym)
			col = dc.del;
		    else if(XK_Delete == k->keysym)
			col = dc.del;
		    else if(XK_space == k->keysym)
			col = dc.spc;
		    else if(XK_Tab == k->keysym)
			col = dc.spc;
		    else if(XK_Escape == k->keysym)
			col = dc.esc;
		    else if(XK_Return == k->keysym)
			col = dc.ret;
		    else if(XK_Cancel == k->keysym)
			col = dc.close;
		    else
			col = dc.norm;
		}
		else
		    col = dc.mod;

	XSetForeground(dpy, dc.gc, col[ColBG].pixel);
	XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);
	XSetForeground(dpy, dc.gc, dc.norm[ColFG].pixel);
	r.height -= 1;
	r.width -= 1;

	XDrawRectangles(dpy, dc.drawable, dc.gc, &r, 1);

    	if(k->label) {
		l = k->label;
	} else {
		l = XKeysymToString(k->keysym);
	}
	len = strlen(l);
	h = dc.font.ascent + dc.font.descent;
	y = k->y + (k->h / 2) - (h / 2) + dc.font.ascent;
	x = k->x + (k->w / 2) - (textnw(l, len) / 2);
	XftDrawStringUtf8(dc.xftdraw, &col[ColFG], dc.font.xftfont, x, y,
	                  (const FcChar8 *)l, len);
	XCopyArea(dpy, dc.drawable, win, dc.gc, k->x, k->y, k->w, k->h,
			k->x, k->y);
}

void
expose(XEvent *e) {
	XExposeEvent *ev = &e->xexpose;

	if(ev->count == 0 && (ev->window == win))
		drawkeyboard();
}

Key *
findkey(int x, int y) {
	int i;

	for(i = 0; i < nkeys; i++) {
		if(keys[i].keysym && x > keys[i].x &&
				x < keys[i].x + keys[i].w &&
				y > keys[i].y && y < keys[i].y + keys[i].h) {
			return &keys[i];
		}
	}
	return NULL;
}

XftColor
getcolor(const char *colstr) {
	XftColor color;

	if(!XftColorAllocName(dpy, DefaultVisual(dpy, screen),
	                      DefaultColormap(dpy, screen), colstr, &color))
		die("error, cannot allocate color '%s'\n", colstr);
	return color;
}

void
initfont(const char *fontstr) {
	if(dc.font.xftfont)
		XftFontClose(dpy, dc.font.xftfont);
	if(!(dc.font.xftfont = XftFontOpenName(dpy, screen, fontstr)))
		die("error, cannot load font: '%s'\n", fontstr);
	dc.font.ascent = dc.font.xftfont->ascent;
	dc.font.descent = dc.font.xftfont->descent;
	dc.font.height = dc.font.xftfont->height;
}

void
leavenotify(XEvent *e) {
	unpress(NULL, 0);
}

void
press(Key *k, KeySym mod) {
	int i;
	KeyCode kc, mc;

	if (k->keysym == XK_ISO_Next_Group && !fixed_layout) {
		switch_locale();
		return;
	}
	kc = XKeysymToKeycode(dpy, k->keysym);
	if (!kc)
		return;
	if (!fixed_layout && xkb_available && xkb_num_groups > 1 && current
	&& current->group < xkb_num_groups && xkb_group != current->group) {
		XkbLockGroup(dpy, XkbUseCoreKbd, current->group);
		xkb_group = current->group;
	}
	k->pressed = !k->pressed;

	if(!IsModifierKey(k->keysym)) {
		for(i = 0; i < nkeys; i++) {
			if(keys[i].pressed && IsModifierKey(keys[i].keysym)) {
				mc = XKeysymToKeycode(dpy, keys[i].keysym);
				if (mc)
					XTestFakeKeyEvent(dpy, mc, True, 0);
			}
		}
		pressedmod = mod;
		if(pressedmod) {
			mc = XKeysymToKeycode(dpy, mod);
			if (mc)
				XTestFakeKeyEvent(dpy, mc, True, 0);
			else
				pressedmod = 0;
		}
		XTestFakeKeyEvent(dpy, kc, True, 0);
		pressedkey = k;

		for(i = 0; i < nkeys; i++) {
			if(keys[i].pressed && IsModifierKey(keys[i].keysym)) {
				mc = XKeysymToKeycode(dpy, keys[i].keysym);
				if (mc)
					XTestFakeKeyEvent(dpy, mc, False, 0);
			}
		}
	}
	drawkey(k);
}

void
unpress(Key *k, KeySym mod) {
	int i;
	KeyCode kc;
	Key *pk = pressedkey ? pressedkey : k;

	if (pk != NULL && pk->keysym == XK_Cancel) {
		kc = XKeysymToKeycode(dpy, pk->keysym);
		if (kc)
			XTestFakeKeyEvent(dpy, kc, False, 0);
		XFlush(dpy);
		exit(0);
	}

	for(i = 0; i < nkeys; i++) {
		if(keys[i].pressed && !IsModifierKey(keys[i].keysym)) {
			kc = XKeysymToKeycode(dpy, keys[i].keysym);
			if (kc)
				XTestFakeKeyEvent(dpy, kc, False, 0);
			keys[i].pressed = 0;
			drawkey(&keys[i]);
			break;
		}
	}
	if(i != nkeys) {
		if(pressedmod) {
			kc = XKeysymToKeycode(dpy, pressedmod);
			if (kc)
				XTestFakeKeyEvent(dpy, kc, False, 0);
		}
		pressedmod = 0;

		for(i = 0; i < nkeys; i++) {
			if(keys[i].pressed) {
				kc = XKeysymToKeycode(dpy, keys[i].keysym);
				if (kc)
					XTestFakeKeyEvent(dpy, kc, False, 0);
				keys[i].pressed = 0;
				drawkey(&keys[i]);
			}
		}
	}
	pressedkey = NULL;
}

void
run(void) {
	XkbEvent ev;
	/* main event loop */
	XSync(dpy, False);

	while(running) {
		XNextEvent(dpy, &ev.core);
		if (xkb_event_base && ev.core.type == xkb_event_base) {
			if (ev.any.xkb_type == XkbStateNotify)
				statechanged(&ev.state);
		} else if(handler[ev.core.type])
			(handler[ev.core.type])(&ev.core); /* call handler */
	}
}

void
setup(void) {
	XSetWindowAttributes wa;
	XTextProperty str;
	XSizeHints *sizeh = NULL;
	XClassHint *ch;
	Atom atype = -1;
	int i, sh, sw, initial;
	const char **tc;
	XWMHints *wmh;


	/* init screen */
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	initfont(font);
	/* init atoms */
	if(isdock) {
		netatom[NetWMWindowType] = XInternAtom(dpy,
				"_NET_WM_WINDOW_TYPE", False);
		atype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	}



	/* init appearance */
	initial = -1;
	if (start_layout)
		initial = layout_index(start_layout);
	if (initial < 0)
		initial = layout_for_group(xkb_group, "full");
	if (initial < 0)
		initial = 0;
	select_layout(initial, 1);

	if(!ww)
		ww = sw;
	if(!wh)
		wh = sh * rows / 32;

	if(!wx)
		wx = 0;
	if(wx < 0)
		wx = sw + wx - ww;
	if(!wy)
		wy = sh - wh;
	if(wy < 0)
		wy = sh + wy - wh;

	tc = theme_keys(current_theme);
	dc.back[ColBG] = getcolor(tc[0]);
	dc.back[ColFG] = getcolor(tc[1]);
	dc.norm[ColBG] = getcolor(tc[2]);
	dc.norm[ColFG] = getcolor(tc[3]);
	dc.press[ColBG] = getcolor(tc[4]);
	dc.press[ColFG] = getcolor(tc[5]);
	dc.high[ColBG] = getcolor(tc[6]);
	dc.high[ColFG] = getcolor(tc[7]);
	dc.mod[ColBG] = getcolor(tc[8]);
	dc.mod[ColFG] = getcolor(tc[9]);
	dc.ret[ColBG] = getcolor(tc[10]);
	dc.ret[ColFG] = getcolor(tc[11]);
	dc.esc[ColBG] = getcolor(tc[12]);
	dc.esc[ColFG] = getcolor(tc[13]);
	dc.del[ColBG] = getcolor(tc[14]);
	dc.del[ColFG] = getcolor(tc[15]);
	dc.spc[ColBG] = getcolor(tc[16]);
	dc.spc[ColFG] = getcolor(tc[17]);
	dc.cur[ColBG] = getcolor(tc[18]);
	dc.cur[ColFG] = getcolor(tc[19]);
	dc.pg[ColBG] = getcolor(tc[20]);
	dc.pg[ColFG] = getcolor(tc[21]);
	dc.close[ColBG] = getcolor(tc[22]);
	dc.close[ColFG] = getcolor(tc[23]);
	
	dc.drawable = XCreatePixmap(dpy, root, ww, wh,
			DefaultDepth(dpy, screen));
	dc.gc = XCreateGC(dpy, root, 0, 0);
	dc.xftdraw = XftDrawCreate(dpy, dc.drawable, DefaultVisual(dpy, screen),
	                           DefaultColormap(dpy, screen));
	if(!dc.xftdraw)
		die("error, cannot create Xft draw context\n");
	for(i = 0; i < nkeys; i++)
		keys[i].pressed = 0;

	wa.override_redirect = !wmborder;
	wa.border_pixel = dc.norm[ColFG].pixel;
	wa.background_pixel = dc.norm[ColBG].pixel;
	win = XCreateWindow(dpy, root, wx, wy, ww-10, wh-10, 0,
			    CopyFromParent, CopyFromParent, CopyFromParent,
			    CWOverrideRedirect | CWBorderPixel |
			    CWBackingPixel, &wa);

	XSelectInput(dpy, win, StructureNotifyMask|ButtonReleaseMask|
			ButtonPressMask|ExposureMask|LeaveWindowMask|
			PointerMotionMask);


	wmh = XAllocWMHints();
	wmh->input = False;
	wmh->flags = InputHint;
	if(!isdock) {
		sizeh = XAllocSizeHints();
		sizeh->flags = PMaxSize | PMinSize;
		sizeh->min_width = sizeh->max_width = ww;
		sizeh->min_height = sizeh->max_height = wh;
	}
	XStringListToTextProperty(&name, 1, &str);
	ch = XAllocClassHint();
	ch->res_class = name;
	ch->res_name = name;

	XSetWMProperties(dpy, win, &str, &str, NULL, 0, sizeh, wmh,
			ch);

XSetBackground(dpy, dc.gc, dc.norm[ColBG].pixel);

	XFree(ch);
	XFree(wmh);
	XFree(str.value);
	if(sizeh != NULL)
		XFree(sizeh);

	if(isdock) {
		XChangeProperty(dpy, win, netatom[NetWMWindowType], XA_ATOM,
				32, PropModeReplace,
				(unsigned char *)&atype, 1);
	}

	XMapRaised(dpy, win);
	updatekeys();

	drawkeyboard();
}

int
textnw(const char *text, uint len) {
	XGlyphInfo ext;

	XftTextExtentsUtf8(dpy, dc.font.xftfont, (const FcChar8 *)text, len, &ext);
	return ext.xOff;
}

void
updatekeys(void) {
	int i, j;
	int x = 0, y = 0, h, base, r = rows;

	h = (wh - 1) / rows;
	for(i = 0; i < nkeys; i++, r--) {
		for(j = i, base = 0; j < nkeys && keys[j].keysym != 0; j++)
			base += keys[j].width;
		for(x = 0; i < nkeys && keys[i].keysym != 0; i++) {
			keys[i].x = x + KEYGAP_X/3;
			keys[i].y = y + KEYGAP_Y;
			keys[i].w = keys[i].width * (ww - 1) / base;
			keys[i].h = r == 1 ? wh - y - 1 - KEYGAP_Y : h - KEYGAP_Y;
			x += keys[i].w + KEYGAP_X;
		}
		if(base != 0)
			keys[i - 1].w = ww - 1 - keys[i - 1].x;
		y += h;
	}
}

void
usage(char *argv0) {
	fprintf(stderr, "usage: %s [-fhdv] [-g geometry] [-l layout] [-L] [-S theme]\n", argv0);
	exit(1);
}

int
main(int argc, char *argv[]) {
	int i, j, xr, yr, bitm;
	unsigned int wr, hr;

	for (i = 1; argv[i]; i++) {
		if(!strcmp(argv[i], "-v")) {
			die("svkbd-"VERSION", © 2006-2016 svkbd engineers,"
				       " see LICENSE for details\n");
		} else if(!strcmp(argv[i], "-d")) {
			isdock = True;
			continue;
		} else if(!strcmp(argv[i], "-f")) {
			fixed_layout = True;
			continue;
		} else if(!strncmp(argv[i], "-g", 2)) {
			if(i >= argc - 1)
				continue;

			bitm = XParseGeometry(argv[i+1], &xr, &yr, &wr, &hr);
			if(bitm & XValue)
				wx = xr;
			if(bitm & YValue)
				wy = yr;
			if(bitm & WidthValue)
				ww = (int)wr;
			if(bitm & HeightValue)
				wh = (int)hr;
			if(bitm & XNegative && wx == 0)
				wx = -1;
			if(bitm & YNegative && wy == 0)
				wy = -1;
			i++;
		} else if(!strcmp(argv[i], "-S")) {
			if (i >= argc - 1)
				usage(argv[0]);
			if (!applytheme(argv[++i]))
				die("svkbd: unknown theme '%s'\n", argv[i]);
		} else if(!strcmp(argv[i], "-l")) {
			if (i >= argc - 1)
				usage(argv[0]);
			start_layout = argv[++i];
		} else if(!strcmp(argv[i], "-L")) {
			for (j = 0; j < (int)NLAYOUTS; j++)
				printf("%s\n", layouts[j].name);
			return 0;
		} else if(!strcmp(argv[i], "-h")) {
			usage(argv[0]);
		}
	}

	if (!current_theme)
		current_theme = &themes[0];
	initlocale();

	if(!(dpy = XOpenDisplay(0)))
		die("svkbd: cannot open display\n");

	if (start_layout && layout_index(start_layout) < 0)
		die("svkbd: unknown layout: %s\n", start_layout);

	initxkb();
	setup();
	run();

	cleanup();
	XCloseDisplay(dpy);
	return 0;
}

