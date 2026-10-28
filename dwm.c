/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLEONTAG(C, T)    ((C)->tags & (T))
#define ISVISIBLE(C)            ISVISIBLEONTAG(C, C->mon->tagset[C->mon->seltags])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeUrg }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetDwmFrame,
       NetDwmHelper, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */

enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkTitleBar, ClkRootWin, ClkLast }; /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
typedef struct Pertag Pertag;

struct Client {
        char name[256];
        float mina, maxa;
        float cfact;
        int x, y, w, h;
        int oldx, oldy, oldw, oldh;
        int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
        int bw, oldbw;
        long oldmask;
        unsigned int tags;
        int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
        int is_scanned;
        int isshaded, shadevertical;
        int shx, shy, shw, shh;
        unsigned char kbdgrp;
        Client *next;
        Client *snext;
        Monitor *mon;
        Window win;      /* frame window */
        Window client;   /* real client window */
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
	Pertag *pertag;
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
} Rule;

typedef struct {
	int y;
	int show;
	Window win;
	char text[256];
} Bar;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void applytag(Monitor *m);
static int applytheme(const char *name);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachtop(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void titlebarclick(const Arg *arg);
static int oncfactbutton(Client *c, int x, int y);
static int ismaster(Client *c);
static int mastercount(Monitor *m);
static void cfactmouse(Client *c);
static void cfactscroll(Client *c, int dir);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawborder(Client *c, int focused);
static void drawframe(Client *c, int wintype);
static void drawshadeframe(Client *c);
static XftFont *rotatedfont(Fnt *base, const char *text, int maxwidth);
static XftFont *scaledfont(Fnt *base, int maxheight);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int hasatomprop(Client *c, Atom prop, Atom value);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static Client **tiledclients(Monitor *m, unsigned int *n);
static int canshade(Client *c);
static int layout_supports_shading(const Layout *lt);
static void toggleshade(Client *c);
static void unshade(Client *c);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizetile(Client *c, int x, int y, int w, int h, int interact);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static void cleanupstale(void);
static int hasprop(Window w, Atom a);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setcfact(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m);
static void togglebar(const Arg *arg);
static void toggleextrabar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);
static void killclient_direct(Client *c);
static void sigterm(int unused);

/* variables */
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar height */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static Client *prevzoom = NULL;
static const char *themename = NULL;
static int running = 1;
static int sigpipe[2] = { -1, -1 };
static Cur *cursor[CurLast];
static Clr **scheme;
static Clr btncloseclr, btnplusclr, btnfgclr;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
static Bar eb;

/* configuration, allows nested code to access above variables */
#include "config.h"

struct Pertag {
	unsigned int curtag, prevtag;
	int nmasters[LENGTH(tags) + 1];
	float mfacts[LENGTH(tags) + 1];
	unsigned int sellts[LENGTH(tags) + 1];
	const Layout *ltidxs[LENGTH(tags) + 1][2];
	int showbars[LENGTH(tags) + 1];
};


GC frame_gc;  // наш собственный graphics context


/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->client, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		int titlew = titlebar_width;
		if (!c->hintsvalid)
			updatesizehints(c);
		/* size hints describe the client, not the frame */
		*w = MAX(1, *w - titlew);
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
		*w += titlew;
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachtop(Client *c)
{
	int n;
	Monitor *m = c->mon;
	Client *below;

	for (n = 1, below = m->clients;
	     below && below->next
	     && (below->isfloating || !ISVISIBLEONTAG(below, c->tags)
	         || n != m->nmaster);
	     n += below->isfloating || !ISVISIBLEONTAG(below, c->tags) ? 0 : 1,
	     below = below->next);
	c->next = NULL;
	if (below) {
		c->next = below->next;
		below->next = c;
	} else
		m->clients = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}




void
titlebarclick(const Arg *arg)
{
        Client *c = selmon->sel;
        int rel_x, rel_y, btn_size;

        if (!c)
                return;

        rel_x = arg->ui & 0xFFFF;
        rel_y = arg->ui >> 16;
        btn_size = (c->isshaded && !c->shadevertical)
                ? MIN(c->h, c->w) : titlebar_width;
        if (rel_x < 0)
                return;
        if (!(c->isshaded && !c->shadevertical) && rel_x >= titlebar_width)
                return;

        /* close: top square (or full-height leading square when shaded horizontally) */
        if (rel_y >= 0 && rel_y < btn_size && rel_x < btn_size) {
                killclient_direct(c);
                return;
        }

        /* [o]: drag to change cfact/mfact or resize a floating window */
        if (oncfactbutton(c, rel_x, rel_y)) {
                cfactmouse(c);
                return;
        }

        /* everything else in the titlebar is a drag zone */
        tilemovemouse(NULL);
}



int
oncfactbutton(Client *c, int x, int y)
{
        int btn_size = titlebar_width;
        int button_y;

        if (!c || c->isshaded || c->h < 2 * btn_size + 4)
                return 0;
        button_y = c->h - btn_size;
        return x >= 0 && x < btn_size && y >= button_y && y < button_y + btn_size;
}

int
ismaster(Client *c)
{
        int i = 0;
        Client *t;

        if (!c || !c->mon || c->isfloating)
                return 0;
        for (t = nexttiled(c->mon->clients); t; t = nexttiled(t->next), i++)
                if (t == c)
                        return i < c->mon->nmaster;
        return 0;
}

int
mastercount(Monitor *m)
{
        unsigned int n = 0;
        Client *t;

        for (t = nexttiled(m->clients); t; t = nexttiled(t->next))
                n++;
        return MIN(n, (unsigned int)m->nmaster);
}

void
cfactmouse(Client *c)
{
        enum { DragFloat, DragMfact, DragCfact } mode;
        Monitor *m;
        XEvent ev;
        Time lasttime = 0;
        int dx, dy, x0, y0, ocw, och;
        float f0;

        if (!c || !(m = c->mon) || c->isshaded)
                return;

        if (c->isfloating || !m->lt[m->sellt]->arrange)
                mode = DragFloat;
        else if (ismaster(c) && mastercount(m) == 1)
                mode = DragMfact;
        else
                mode = DragCfact;

        if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
                return;
        if (!getrootptr(&x0, &y0)) {
                XUngrabPointer(dpy, CurrentTime);
                return;
        }

        ocw = c->w;
        och = c->h;
        f0 = mode == DragMfact ? m->mfact : c->cfact;

        do {
                XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
                switch (ev.type) {
                case ConfigureRequest:
                case Expose:
                case MapRequest:
                        handler[ev.type](&ev);
                        break;
                case MotionNotify:
                        if ((ev.xmotion.time - lasttime) <= (1000 / 60))
                                continue;
                        lasttime = ev.xmotion.time;
                        dx = ev.xmotion.x - x0;
                        dy = ev.xmotion.y - y0;

                        if (mode == DragFloat) {
                                resize(c, c->x, c->y,
                                        MAX(1, ocw + dx), MAX(1, och + dy), 1);
                        } else {
                                int use_y = mode == DragMfact
                                        ? m->lt[m->sellt]->arrange == bstack
                                        : m->lt[m->sellt]->arrange != bstack;
                                float f = f0 + (use_y ? dy : dx)
                                        * (mode == DragMfact
                                        ? mfact_drag_step : cfact_drag_step);

                                if (mode == DragMfact) {
                                        m->mfact = MAX(0.05, MIN(0.95, f));
                                        m->pertag->mfacts[m->pertag->curtag] = m->mfact;
                                } else {
                                        c->cfact = MAX(0.25, MIN(4.0, f));
                                }
                                arrange(m);
                        }
                        break;
                }
        } while (ev.type != ButtonRelease);

        XUngrabPointer(dpy, CurrentTime);
}

void
cfactscroll(Client *c, int dir)
{
        Monitor *m;

        if (!c || !(m = c->mon) || c->isshaded)
                return;

        if (c->isfloating || !m->lt[m->sellt]->arrange) {
                resize(c, c->x, c->y,
                        MAX(1, c->w + dir * 20), MAX(1, c->h + dir * 20), 1);
                return;
        }

        if (ismaster(c) && mastercount(m) == 1) {
                m->mfact = MAX(0.05, MIN(0.95,
                        m->mfact + dir * mfact_scroll_step));
                m->pertag->mfacts[m->pertag->curtag] = m->mfact;
        } else {
                c->cfact = MAX(0.25, MIN(4.0,
                        c->cfact + dir * cfact_scroll_step));
        }
        arrange(m);
}

void
buttonpress(XEvent *e)
{
	static Time lastclicktime = 0;
	static Window lastclickwin = None;
	static int lastclickx = 0, lastclicky = 0;
	unsigned int i, x, click;
	int dbl = 0;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + TEXTW(selmon->ltsymbol))
			click = ClkLtSymbol;
		else if (ev->x > selmon->ww - (int)TEXTW(stext))
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		if (ev->window == c->win) {
			int rx = ev->x, ry = ev->y;
			int bs = (c->isshaded && !c->shadevertical)
				? MIN(c->h, c->w) : titlebar_width;
			int inclose = rx < bs && ry < bs;
			int incfact = oncfactbutton(c, rx, ry);

			if ((ev->button == Button4 || ev->button == Button5)
			&& incfact) {
				cfactscroll(c, ev->button == Button4 ? +1 : -1);
				return;
			}

			if (ev->button == Button1 && canshade(c)
			&& !inclose && !incfact) {
				if (ev->time - lastclicktime < 250
				&& ev->window == lastclickwin
				&& abs(rx - lastclickx) < 6 && abs(ry - lastclicky) < 6) {
					dbl = 1;
					lastclicktime = 0;
				} else {
					lastclicktime = ev->time;
					lastclickwin = ev->window;
					lastclickx = rx;
					lastclicky = ry;
				}
			} else
				lastclicktime = 0;

			click = ClkTitleBar;
			arg.ui = (ev->y << 16) | ev->x;
		} else {
			click = ClkClientWin;
		}
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		if (dbl) {
			toggleshade(c);
			return;
		}
	}

	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func((click == ClkTagBar && buttons[i].arg.i == 0) || (click == ClkTitleBar) ? &arg : &buttons[i].arg);
}





void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}


void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	running = 0;
	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	if (eb.win) {
		XUnmapWindow(dpy, eb.win);
		XDestroyWindow(dpy, eb.win);
		eb.win = None;
	}
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		free(scheme[i]);
	free(scheme);
	XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &btncloseclr);
	XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &btnplusclr);
	XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &btnfgclr);
	XFreeGC(dpy, frame_gc);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	if (sigpipe[0] != -1) {
		close(sigpipe[0]);
		sigpipe[0] = -1;
	}
	if (sigpipe[1] != -1) {
		close(sigpipe[1]);
		sigpipe[1] = -1;
	}
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}









void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon->pertag);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}


void
configure(Client *c)
{
        XConfigureEvent ce;

        if (c->isshaded)
                return;
        ce.type = ConfigureNotify;
        ce.display = dpy;
        ce.event = c->client;
        ce.window = c->client;
        if (c->isfullscreen) {
                ce.x = c->x + c->bw;
                ce.y = c->y + c->bw;
                ce.width = MAX(1, c->w);
                ce.height = MAX(1, c->h);
        } else {
                ce.x = c->x + c->bw + titlebar_width;
                ce.y = c->y + c->bw;
                ce.width = MAX(1, c->w - titlebar_width);
                ce.height = MAX(1, c->h);
        }
        ce.border_width = 0;
        ce.above = None;
        ce.override_redirect = False;
        XSendEvent(dpy, c->client, False, StructureNotifyMask, (XEvent *)&ce);
}


void
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth) {
			c->bw = ev->border_width;
			XSetWindowBorderWidth(dpy, c->win, c->bw);
		}
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width + titlebar_width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight))) {
				XMoveWindow(dpy, c->win, c->x, c->y);
				configure(c);
			} else if (ISVISIBLE(c))
				resizeclient(c, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	Monitor *m;
	unsigned int i;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);

	m->pertag = ecalloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;
	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;
		m->pertag->sellts[i] = m->sellt;
		m->pertag->ltidxs[i][0] = m->lt[0];
		m->pertag->ltidxs[i][1] = m->lt[1];
		m->pertag->showbars[i] = m->showbar;
	}
	return m;
}




void
destroynotify(XEvent *e)
{
        Client *c;
        XDestroyWindowEvent *ev = &e->xdestroywindow;

        if ((c = wintoclient(ev->window))) {
                /* Удаляем ТОЛЬКО если окно реально уничтожено */
                unmanage(c, 1);
        }
}


void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void
drawborder(Client *c, int focused)
{
	unsigned int s;

	if (!c || !c->win)
		return;
	s = c->isurgent ? SchemeUrg : (focused ? SchemeSel : SchemeNorm);
	XSetWindowBorder(dpy, c->win, scheme[s][ColBorder].pixel);
}

void
drawbar(Monitor *m)
{
	int x, w, tw = 0;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	int showextra = m == mons && eb.show && eb.win;
	Client *c;

	if (!m->showbar && !showextra)
		return;

	if (m->showbar) {
	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon) { /* status is only drawn on selected monitor */
		drw_setscheme(drw, scheme[SchemeNorm]);
		tw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
		drw_text(drw, m->ww - tw, 0, tw, bh, 0, stext, 0);
	}

	for (c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i)
			drw_rect(drw, x + boxs, boxs, boxw, boxw,
				m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
				urg & 1 << i);
		x += w;
	}
	w = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

	if ((w = m->ww - tw - x) > bh) {
		if (m->sel) {
			drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
			drw_text(drw, x, 0, w, bh, lrpad / 2, m->sel->name, 0);
			if (m->sel->isfloating)
				drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
		} else {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w, bh, 1, 1);
		}
	}
	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
	}

	if (showextra) {
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, 0, 0, m->ww, bh, 1, 1);
		drw_text(drw, 0, 0, m->ww, bh, 0, eb.text, 0);
		drw_map(drw, eb.win, 0, 0, m->ww, bh);
	}
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}


void
expose(XEvent *e)
{
        Monitor *m;
        Client *c;
        XExposeEvent *ev = &e->xexpose;

        if (ev->count != 0)
                return;
        if ((m = wintomon(ev->window)) && ev->window == m->barwin) {
                drawbar(m);
                return;
        }
        if ((c = wintoclient(ev->window)) && ev->window == c->win)
                drawframe(c, c == selmon->sel);
}




void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && (!ISVISIBLE(c) || c->isshaded); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		XkbLockGroup(dpy, XkbUseCoreKbd, c->kbdgrp);
		drawborder(c, 1);
                drawframe(c,1);  // <- перерисовать titlebar активного окна
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		if (c->isshaded) {
			XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
			XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
		} else
			setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->client)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
		return;
	if (arg->i > 0) {
		for (c = selmon->sel->next; c && (!ISVISIBLE(c) || c->isshaded); c = c->next);
		if (!c)
			for (c = selmon->clients; c && (!ISVISIBLE(c) || c->isshaded); c = c->next);
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i) && !i->isshaded)
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i) && !i->isshaded)
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, c->client, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

int
hasatomprop(Client *c, Atom prop, Atom value)
{
	int di, found = 0;
	unsigned long i, n, extra;
	unsigned char *p = NULL;
	Atom da, *atoms;

	if (XGetWindowProperty(dpy, c->client, prop, 0L, 32L, False, XA_ATOM,
		&da, &di, &n, &extra, &p) == Success && p) {
		atoms = (Atom *)p;
		for (i = 0; i < n; i++)
			if (atoms[i] == value) {
				found = 1;
				break;
			}
		XFree(p);
	}
	return found;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING) {
		strncpy(text, (char *)name.value, size - 1);
	} else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
		strncpy(text, *list, size - 1);
		XFreeStringList(list);
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->client);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->client, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->client, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j, k;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		int start, end, skip;
		KeySym *syms;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		XDisplayKeycodes(dpy, &start, &end);
		syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
		if (!syms)
			return;
		for (k = start; k <= end; k++)
			for (i = 0; i < LENGTH(keys); i++)
				/* skip modifier codes, we do that ourselves */
				if (keys[i].keysym == syms[(k - start) * skip])
					for (j = 0; j < LENGTH(modifiers); j++)
						XGrabKey(dpy, k,
							 keys[i].mod | modifiers[j],
							 root, True,
							 GrabModeAsync, GrabModeAsync);
		XFree(syms);
	}
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	selmon->pertag->nmasters[selmon->pertag->curtag] = selmon->nmaster;
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}



void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->client);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}






void
manage(Window w, XWindowAttributes *wa)
{
        Client *c, *t = NULL;
        Window trans = None;

        c = ecalloc(1, sizeof(Client));
        c->client = w;

        /* geometry */
        c->x = c->oldx = wa->x;
        c->y = c->oldy = wa->y;
        c->w = c->oldw = wa->width;
        c->h = c->oldh = wa->height;
        c->oldbw = wa->border_width;
        c->oldmask = wa->your_event_mask;
        c->cfact = 1.0;

        updatetitle(c);
        if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
                c->mon = t->mon;
                c->tags = t->tags;
        } else {
                c->mon = selmon;
                applyrules(c);
        }

        if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
                c->x = c->mon->wx + c->mon->ww - WIDTH(c);
        if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
                c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
        c->x = MAX(c->x, c->mon->wx);
        c->y = MAX(c->y, c->mon->wy);
        c->bw = borderpx;

        /* CREATE FRAME */
        c->win = XCreateWindow(dpy, root, c->x, c->y, c->w, c->h,
                               c->bw, DefaultDepth(dpy, screen), InputOutput,
                               DefaultVisual(dpy, screen),
                               CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
                               &(XSetWindowAttributes){
                                       .override_redirect = True,
                                       .background_pixel = scheme[SchemeNorm][ColBg].pixel,
                                       .border_pixel = scheme[SchemeNorm][ColBorder].pixel,
                                       .event_mask = ButtonPressMask | ButtonReleaseMask |
                                                     PointerMotionMask | ExposureMask | EnterWindowMask |
                                                     SubstructureRedirectMask
                               });

        XChangeProperty(dpy, c->win, netatom[NetDwmFrame], XA_WINDOW, 32,
                       PropModeReplace, (unsigned char *)&c->win, 1);

        /* REPARENT IMMEDIATELY, THE FRAME OWNS THE BORDER */
        XAddToSaveSet(dpy, c->client);
        XSetWindowBorderWidth(dpy, c->client, 0);
        XReparentWindow(dpy, c->client, c->win, titlebar_width, 0);

        /* SET INPUT */
        XSelectInput(dpy, c->client, EnterWindowMask | FocusChangeMask |
                     PropertyChangeMask | StructureNotifyMask);

        /* CONFIGURE */
        configure(c);
        updatewindowtype(c);
        updatesizehints(c);
        updatewmhints(c);
        grabbuttons(c, 0);
        if (!c->isfloating)
                c->isfloating = c->oldstate = trans != None || c->isfixed;
        if (!c->isfullscreen
        && (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange)) {
                c->w = MAX(1, MIN(c->w, c->mon->ww - 2 * c->bw));
                c->h = MAX(1, MIN(c->h, c->mon->wh - 2 * c->bw));
                c->x = c->mon->wx + (c->mon->ww - WIDTH(c)) / 2;
                c->y = c->mon->wy + (c->mon->wh - HEIGHT(c)) / 2;
                XMoveWindow(dpy, c->win, c->x, c->y);
        }
        if (c->isfloating)
                XRaiseWindow(dpy, c->win);
        attachtop(c);
        attachstack(c);
        XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
                       (unsigned char *) &(c->client), 1);

        /* MAP BOTH WINDOWS */
        XMapWindow(dpy, c->win);
        XMapWindow(dpy, c->client);

        setclientstate(c, NormalState);
        if (c->mon == selmon)
                unfocus(selmon->sel, 0);
        c->mon->sel = c;
        {
                XkbStateRec kbd_state;
                XkbGetState(dpy, XkbUseCoreKbd, &kbd_state);
                c->kbdgrp = kbd_state.group;
        }
        arrange(c->mon);
        if (!c->isfullscreen
        && (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange)) {
                c->x = c->mon->wx + (c->mon->ww - WIDTH(c)) / 2;
                c->y = c->mon->wy + (c->mon->wh - HEIGHT(c)) / 2;
                XMoveWindow(dpy, c->win, c->x, c->y);
                configure(c);
        }
        focus(NULL);
}




void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;
	Client *c;

	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;
	if ((c = wintoclient(ev->window))) {
		/* already managed: map the real client unless it is shaded */
		if (ev->window == c->client && !c->isshaded)
			XMapWindow(dpy, c->client);
		return;
	}
	manage(ev->window, &wa);
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

Client **
tiledclients(Monitor *m, unsigned int *n)
{
	unsigned int i;
	Client *c, **cl;

	for (*n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next))
		(*n)++;
	if (!*n)
		return NULL;
	cl = ecalloc(*n, sizeof(Client *));
	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next))
		cl[i++] = c;
	return cl;
}

int
layout_supports_shading(const Layout *lt)
{
	return lt && lt->arrange
		&& (lt->arrange == tile || lt->arrange == bstack || lt->arrange == grid);
}

int
canshade(Client *c)
{
	if (!c || c->isfloating || c->isfullscreen || !c->mon)
		return 0;
	return layout_supports_shading(c->mon->lt[c->mon->sellt]);
}

void
unshade(Client *c)
{
	if (!c || !c->isshaded)
		return;
	c->isshaded = 0;
	c->shadevertical = 0;
	c->x = c->shx;
	c->y = c->shy;
	c->w = MAX(1, c->shw);
	c->h = MAX(1, c->shh);
	XMapWindow(dpy, c->client);
	resizeclient(c, c->x, c->y, c->w, c->h);
}

void
toggleshade(Client *c)
{
	Monitor *m;

	if (!canshade(c))
		return;
	m = c->mon;
	if (c->isshaded)
		unshade(c);
	else {
		c->shx = c->x;
		c->shy = c->y;
		c->shw = c->w;
		c->shh = c->h;
		c->isshaded = 1;
		XUnmapWindow(dpy, c->client);
	}
	arrange(m);
	if (c->isshaded) {
		if (m->sel == c)
			focus(NULL);
	} else
		focus(c);
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->client, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			c->hintsvalid = 0;
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			drawframe(c, c == selmon->sel);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType]
		|| ev->atom == netatom[NetWMState])
			updatewindowtype(c);
	}
}

void
quit(const Arg *arg)
{
	running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (c->isshaded) {
		resizeclient(c, x, y, w, h);
		return;
	}
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

/* tiled clients must fill their cell even if size increments are smaller */
void
resizetile(Client *c, int x, int y, int w, int h, int interact)
{
	x += gappx;
	y += gappx;
	w = MAX(1, w - 2 * (int)gappx);
	h = MAX(1, h - 2 * (int)gappx);

	resize(c, x, y, w, h, interact);
	if (!c->isshaded && (c->w < w || c->h < h))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
        int content_w, content_h;

        if (!c->isfullscreen && !c->isshaded
        && (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange)) {
                w = MAX(1, MIN(w, c->mon->ww - 2 * c->bw));
                h = MAX(1, MIN(h, c->mon->wh - 2 * c->bw));
                if (x + w + 2 * c->bw > c->mon->wx + c->mon->ww)
                        x = c->mon->wx + c->mon->ww - w - 2 * c->bw;
                if (y + h + 2 * c->bw > c->mon->wy + c->mon->wh)
                        y = c->mon->wy + c->mon->wh - h - 2 * c->bw;
                x = MAX(x, c->mon->wx);
                y = MAX(y, c->mon->wy);
        }

        c->oldx = c->x; c->x = x;
        c->oldy = c->y; c->y = y;
        c->oldw = c->w; c->w = w;
        c->oldh = c->h; c->h = h;

        /* frame interior is the whole tile, its border is c->bw */
        XMoveResizeWindow(dpy, c->win, x, y, w, h);

        if (c->isfullscreen) {
                content_w = MAX(1, w);
                content_h = MAX(1, h);
                XMoveResizeWindow(dpy, c->client, 0, 0, content_w, content_h);
                configure(c);
                XSync(dpy, False);
                drawframe(c, c == selmon->sel);
                return;
        }

        if (c->isshaded) {
                XSync(dpy, False);
                drawframe(c, c == selmon->sel);
                return;
        }

        /* client lives inside the titlebar, without its own border */
        content_w = MAX(1, w - titlebar_width);
        content_h = MAX(1, h);
        XMoveResizeWindow(dpy, c->client, titlebar_width, 0, content_w, content_h);

        configure(c);
        XSync(dpy, False);
        drawframe(c, c == selmon->sel);
}



void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void)
{
	XEvent ev;
	int xfd = ConnectionNumber(dpy);
	fd_set fds;
	int maxfd;

	/* main event loop */
	XSync(dpy, False);

	while (running) {
		while (running && XPending(dpy)) {
			XNextEvent(dpy, &ev);
			if (handler[ev.type])
				handler[ev.type](&ev);
		}
		if (!running)
			break;

		FD_ZERO(&fds);
		FD_SET(xfd, &fds);
		maxfd = xfd;
		if (sigpipe[0] != -1) {
			FD_SET(sigpipe[0], &fds);
			maxfd = MAX(maxfd, sigpipe[0]);
		}
		if (select(maxfd + 1, &fds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;
			die("dwm: select:");
		}
		if (sigpipe[0] != -1 && FD_ISSET(sigpipe[0], &fds)) {
			char buf[64];
			while (read(sigpipe[0], buf, sizeof buf) > 0);
			running = 0;
			break;
		}
	}
}


int
hasprop(Window w, Atom a)
{
        Atom da;
        int di;
        unsigned long n, extra;
        unsigned char *p = NULL;

        if (XGetWindowProperty(dpy, w, a, 0L, 1L, False, AnyPropertyType,
                               &da, &di, &n, &extra, &p) != Success)
                return 0;
        if (p)
                XFree(p);
        return n > 0;
}

/* Release clients trapped in frames left behind by a previous dwm. */
void
cleanupstale(void)
{
        unsigned int i, j, num, nchildren;
        Window d1, d2, parent, *wins = NULL, *children = NULL;
        Monitor *m;
        int released = 0;

        if (!XQueryTree(dpy, root, &d1, &d2, &wins, &num))
                return;

        XSetErrorHandler(xerrordummy);
        for (i = 0; i < num; i++) {
                if (wins[i] == wmcheckwin || wins[i] == eb.win)
                        continue;
                for (m = mons; m; m = m->next)
                        if (wins[i] == m->barwin)
                                break;
                if (m)
                        continue;

                if (hasprop(wins[i], netatom[NetDwmFrame])) {
                        if (XQueryTree(dpy, wins[i], &d1, &parent,
                                       &children, &nchildren)) {
                                for (j = 0; j < nchildren; j++) {
                                        int sx, sy;
                                        Window child;

                                        if (!XTranslateCoordinates(dpy, children[j], root,
                                                                   0, 0, &sx, &sy, &child))
                                                continue;
                                        XSelectInput(dpy, children[j], NoEventMask);
                                        XUngrabButton(dpy, AnyButton, AnyModifier, children[j]);
                                        XReparentWindow(dpy, children[j], root, sx, sy);
                                        XSetWindowBorderWidth(dpy, children[j], 0);
                                        XMapWindow(dpy, children[j]);
                                }
                                XFree(children);
                                children = NULL;
                        }
                        XDestroyWindow(dpy, wins[i]);
                        released = 1;
                } else if (hasprop(wins[i], netatom[NetDwmHelper])) {
                        XDestroyWindow(dpy, wins[i]);
                        released = 1;
                }
        }
        XSetErrorHandler(xerror);
        XFree(wins);
        if (released)
                XSync(dpy, False);
}

void
scan(void)
{
        unsigned int i, num;
        Window d1, d2, *wins = NULL;
        XWindowAttributes wa;

        cleanupstale();
        if (!XQueryTree(dpy, root, &d1, &d2, &wins, &num))
                return;

        for (i = 0; i < num; i++) {
                if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect)
                        continue;
                if (wa.map_state != IsViewable && getstate(wins[i]) != IconicState)
                        continue;
                /* do not adopt our own (possibly stale) frames */
                if (wintoclient(wins[i]))
                        continue;
                if (!XGetTransientForHint(dpy, wins[i], &d1))
                        manage(wins[i], &wa);
        }

        /* transient clients after their parents, as in dwm */
        for (i = 0; i < num; i++) {
                if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect)
                        continue;
                if (wintoclient(wins[i]))
                        continue;
                if (XGetTransientForHint(dpy, wins[i], &d1)
                && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
                        manage(wins[i], &wa);
        }

        XFree(wins);
}




void
scan_TEST(void)
{
        unsigned int i, num;
        Window d1, d2, *wins = NULL;
        XWindowAttributes wa;
        char name[256];

        fprintf(stderr, "=== SCAN START ===\n");

        if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
                fprintf(stderr, "Found %u windows in root\n", num);
                for (i = 0; i < num; i++) {
                        if (!XGetWindowAttributes(dpy, wins[i], &wa)) {
                                fprintf(stderr, "  [%u] 0x%lx: XGetWindowAttributes failed\n", i, wins[i]);
                                continue;
                        }
                        if (wa.map_state != IsViewable) {
                                fprintf(stderr, "  [%u] 0x%lx: not viewable\n", i, wins[i]);
                                continue;
                        }
                        if (!gettextprop(wins[i], XA_WM_NAME, name, sizeof name)) {
                                strcpy(name, "(no name)");
                        }
                        fprintf(stderr, "  [%u] 0x%lx: '%s' %dx%d (override_redirect=%d)\n",
                                i, wins[i], name, wa.width, wa.height, wa.override_redirect);
                        manage(wins[i], &wa);
                }
                XFree(wins);
        }

        fprintf(stderr, "=== SCAN END ===\n");
}




void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	if (!layout_supports_shading(m->lt[m->sellt]))
		unshade(c);
	attachtop(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->client, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->client, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->client;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->client, False, NoEventMask, &ev);
	}
	return exists;
}

void
setfocus(Client *c)
{
        if (c->isshaded)
                return;
        if (!c->neverfocus) {
                XSetInputFocus(dpy, c->client, RevertToPointerRoot, CurrentTime);
                XChangeProperty(dpy, root, netatom[NetActiveWindow],
                                XA_WINDOW, 32, PropModeReplace,
                                (unsigned char *) &(c->client), 1);
        }
        sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		unshade(c);
		XChangeProperty(dpy, c->client, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		XSetWindowBorderWidth(dpy, c->win, c->bw);
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->client, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		XSetWindowBorderWidth(dpy, c->win, c->bw);
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void
setlayout(const Arg *arg)
{
	Client *c;

	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	selmon->pertag->sellts[selmon->pertag->curtag] = selmon->sellt;
	if (arg && arg->v) {
		selmon->lt[selmon->sellt] = (Layout *)arg->v;
		selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt]
			= (Layout *)arg->v;
	}
	if (!layout_supports_shading(selmon->lt[selmon->sellt]))
		for (c = selmon->clients; c; c = c->next)
			unshade(c);
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void
setcfact(const Arg *arg)
{
	float f;
	Client *c = selmon->sel;

	if (!arg || !c || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f + c->cfact;
	if (arg->f == 0.0)
		f = 1.0;
	else if (f < 0.25 || f > 4.0)
		return;
	c->cfact = f;
	arrange(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = f;
	selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon);
}

int
applytheme(const char *name)
{
	int i;

	if (!name)
		return 0;
	for (i = 0; themes[i].name; i++)
		if (!strcmp(themes[i].name, name))
			break;
	if (!themes[i].name)
		return 0;

	colors[SchemeNorm][ColFg] = themes[i].fg;
	colors[SchemeNorm][ColBg] = themes[i].bg;
	colors[SchemeNorm][ColBorder] = themes[i].color[8];

	colors[SchemeSel][ColFg] = themes[i].bg;
	colors[SchemeSel][ColBg] = themes[i].cursor;
	colors[SchemeSel][ColBorder] = themes[i].cursor;

	colors[SchemeUrg][ColFg] = themes[i].bg;
	colors[SchemeUrg][ColBg] = themes[i].cursor;
	colors[SchemeUrg][ColBorder] = themes[i].color[1];

	titlebarclosecolor = themes[i].color[1];
	titlebarpluscolor = themes[i].color[2];
	titlebarbtnfgcolor = themes[i].color[15];

	floatnormbordercolor = themes[i].color[3];
	floatselbordercolor = themes[i].color[11];
	return 1;
}

void
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;
	struct sigaction sa;

	/* do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* clean up any zombies (inherited from .xinitrc etc) immediately */
	while (waitpid(-1, NULL, WNOHANG) > 0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = drw->fonts->h + 2;
	titlebar_width = MAX(titlebar_width, bh);
	eb.show = extrabar;
	updategeom();

frame_gc = XCreateGC(dpy, root, 0, NULL);


	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netatom[NetDwmFrame] = XInternAtom(dpy, "_DWM_FRAME", False);
	netatom[NetDwmHelper] = XInternAtom(dpy, "_DWM_HELPER", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	drw_clr_create(drw, &btncloseclr, titlebarclosecolor);
	drw_clr_create(drw, &btnplusclr, titlebarpluscolor);
	drw_clr_create(drw, &btnfgclr, titlebarbtnfgcolor);
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetDwmHelper], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *)&wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	XSetCloseDownMode(dpy, RetainPermanent);
	grabkeys();
	focus(NULL);
}

void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	drawborder(c, c == selmon->sel);
	if (!(wmh = XGetWMHints(dpy, c->client)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->client, wmh);
	XFree(wmh);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
spawn(const Arg *arg)
{
	const char **argv = (const char **)arg->v;
	const char **extra = NULL;
	struct sigaction sa;
	int n;

	if (themename && (arg->v == termcmd || arg->v == dmenucmd)) {
		for (n = 0; argv[n]; n++);
		extra = ecalloc(n + 3, sizeof(char *));
		memcpy(extra, argv, n * sizeof(char *));
		extra[n] = "-S";
		extra[n + 1] = themename;
		extra[n + 2] = NULL;
		argv = extra;
	}

	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		execvp(argv[0], (char **)argv);
		die("dwm: execvp '%s' failed:", argv[0]);
	}
	free(extra);
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
tile(Monitor *m)
{
	unsigned int i, j, n, nm, shm, shs, promote;
	float mfacts = 0, sfacts = 0;
	int barh, mw, y, ch, h, rem;
	Client *c, **cl;
	unsigned char *used;

	if (!(cl = tiledclients(m, &n)))
		return;

	nm = MIN(n, m->nmaster);
	mw = n > m->nmaster ? (m->nmaster ? m->ww * m->mfact : 0) : m->ww;
	used = ecalloc(n, sizeof(unsigned char));
	barh = bh + 2 * (int)borderpx + 2 * (int)gappx;
	shm = 0;

	for (i = 0; i < nm; i++) {
		mfacts += cl[i]->cfact;
		if (cl[i]->isshaded)
			shm++;
	}
	promote = 0;
	for (i = nm; i < n; i++)
		if (!cl[i]->isshaded)
			promote++;
	promote = MIN(promote, shm);

	/* master column */
	rem = m->wh - (int)shm * barh;
	y = m->wy;
	for (i = 0; i < nm; i++) {
		c = cl[i];
		c->shadevertical = 0;
		ch = mfacts > 0 ? (int)(rem * (c->cfact / mfacts)) : 0;
		if (c->isshaded) {
			resizetile(c, m->wx, y, mw - 2 * c->bw, barh - 2 * c->bw, 0);
			y += barh;
			if (promote > 0) {
				for (j = nm; j < n; j++)
					if (!used[j] && !cl[j]->isshaded)
						break;
				if (j < n) {
					Client *p = cl[j];
					used[j] = 1;
					p->shadevertical = 0;
					resizetile(p, m->wx, y, mw - 2 * p->bw,
						MAX(1, ch) - 2 * p->bw, 0);
					y += MAX(1, ch);
					promote--;
				}
			}
		} else {
			resizetile(c, m->wx, y, mw - 2 * c->bw,
				MAX(1, ch) - 2 * c->bw, 0);
			y += MAX(1, ch);
		}
		mfacts -= c->cfact;
		rem -= ch;
	}

	/* stack column */
	shs = 0;
	for (i = nm; i < n; i++) {
		if (used[i])
			continue;
		if (cl[i]->isshaded)
			shs++;
		else
			sfacts += cl[i]->cfact;
	}
	rem = m->wh - (int)shs * barh;
	y = m->wy;
	for (i = nm; i < n; i++) {
		c = cl[i];
		if (used[i])
			continue;
		if (c->isshaded) {
			c->shadevertical = 0;
			resizetile(c, m->wx + mw, y, m->ww - mw - 2 * c->bw,
				barh - 2 * c->bw, 0);
			y += barh;
		} else {
			h = sfacts > 0 ? (int)(rem * (c->cfact / sfacts)) : 0;
			resizetile(c, m->wx + mw, y, m->ww - mw - 2 * c->bw,
				MAX(1, h) - 2 * c->bw, 0);
			y += MAX(1, h);
			rem -= h;
			sfacts -= c->cfact;
		}
	}

	free(used);
	free(cl);
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	selmon->pertag->showbars[selmon->pertag->curtag] = selmon->showbar;
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
	arrange(selmon);
}

void
toggleextrabar(const Arg *arg)
{
	if (selmon != mons)
		return;
	eb.show = !eb.show;
	updatebarpos(selmon);
	arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	if (selmon->sel->isshaded)
		unshade(selmon->sel);
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
			selmon->sel->w, selmon->sel->h, 0);
	arrange(selmon);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	unsigned int i;
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		if (newtagset == TAGMASK) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			selmon->pertag->curtag = 0;
		} else if (selmon->pertag->curtag == 0
		|| !(newtagset & (1u << (selmon->pertag->curtag - 1)))) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			for (i = 0; !(newtagset & (1u << i)); i++);
			selmon->pertag->curtag = i + 1;
		}
		applytag(selmon);
		focus(NULL);
		arrange(selmon);
	}
}

void
unfocus(Client *c, int setfocus)
{
	XkbStateRec kbd_state;

	if (!c)
		return;
	grabbuttons(c, 0);
	drawborder(c, 0);
	drawframe(c,0);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	XkbGetState(dpy, XkbUseCoreKbd, &kbd_state);
	c->kbdgrp = kbd_state.group;
}



void
unmanage(Client *c, int destroyed)
{
        Monitor *m = c->mon;
        XWindowChanges wc;

        detach(c);
        detachstack(c);
        if (!destroyed) {
                XWindowAttributes wa;
                int viewable;

                viewable = XGetWindowAttributes(dpy, c->client, &wa)
                        && wa.map_state == IsViewable;
                wc.border_width = c->oldbw;
                XGrabServer(dpy); /* avoid race conditions */
                XSetErrorHandler(xerrordummy);
                XSelectInput(dpy, c->client, NoEventMask);
                XUngrabButton(dpy, AnyButton, AnyModifier, c->client);
                XRemoveFromSaveSet(dpy, c->client);
                XReparentWindow(dpy, c->client, root,
                                c->x + c->bw + titlebar_width, c->y + c->bw);
                if (viewable || c->isshaded)
                        XMapWindow(dpy, c->client);
                XConfigureWindow(dpy, c->client, CWBorderWidth, &wc);
                XSelectInput(dpy, c->client, c->oldmask);
                setclientstate(c, WithdrawnState);
                XSync(dpy, False);
                XSetErrorHandler(xerror);
                XUngrabServer(dpy);
        }
        XDestroyWindow(dpy, c->win);
        if (prevzoom == c)
                prevzoom = NULL;
        free(c);
        if (running) {
                focus(NULL);
                updateclientlist();
                arrange(m);
        }
}





void
unmapnotify(XEvent *e)
{
        Client *c;
        XUnmapEvent *ev = &e->xunmap;

        if ((c = wintoclient(ev->window))) {
                XWindowAttributes wa;
                /* unmap is part of our own shading, not a withdrawal */
                if (c->isshaded)
                        return;
                /* synthetic unmap (withdraw) must release the frame too */
                if (XGetWindowAttributes(dpy, c->client, &wa) && wa.map_state == IsUnmapped)
                        unmanage(c, 0);
                /* If the client is still viewable this is not a withdrawal. */
        }
}



void
updatebars(void)
{
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
				CopyFromParent, DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
		XChangeProperty(dpy, m->barwin, netatom[NetDwmHelper], XA_WINDOW, 32,
			PropModeReplace, (unsigned char *)&m->barwin, 1);
	}
	if (mons && !eb.win) {
		eb.win = XCreateWindow(dpy, root, mons->wx, eb.y, mons->ww, bh, 0,
			DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
			CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, eb.win, cursor[CurNormal]->cursor);
		XMapRaised(dpy, eb.win);
		XSetClassHint(dpy, eb.win, &ch);
		XChangeProperty(dpy, eb.win, netatom[NetDwmHelper], XA_WINDOW, 32,
			PropModeReplace, (unsigned char *)&eb.win, 1);
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	} else
		m->by = -bh;

	if (m == mons) {
		if (eb.show) {
			m->wh -= bh;
			eb.y = m->topbar ? m->wy + m->wh : m->wy;
			m->wy = m->topbar ? m->wy : m->wy + bh;
		} else
			eb.y = -bh;
		if (eb.win)
			XMoveResizeWindow(dpy, eb.win, m->wx, eb.y, m->ww, bh);
	}
}

void
updateclientlist(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *) &(c->client), 1);
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;

		/* new monitors if nn > n */
		for (i = n; i < nn; i++) {
			for (m = mons; m && m->next; m = m->next);
			if (m)
				m->next = createmon();
			else
				mons = createmon();
		}
		for (i = 0, m = mons; i < nn && m; m = m->next, i++)
			if (i >= n
			|| unique[i].x_org != m->mx || unique[i].y_org != m->my
			|| unique[i].width != m->mw || unique[i].height != m->mh)
			{
				dirty = 1;
				m->num = i;
				m->mx = m->wx = unique[i].x_org;
				m->my = m->wy = unique[i].y_org;
				m->mw = m->ww = unique[i].width;
				m->mh = m->wh = unique[i].height;
				updatebarpos(m);
			}
		/* removed monitors if n > nn */
		for (i = nn; i < n; i++) {
			for (m = mons; m && m->next; m = m->next);
			while ((c = m->clients)) {
				dirty = 1;
				m->clients = c->next;
				detachstack(c);
				c->mon = mons;
				attachtop(c);
				attachstack(c);
			}
			if (m == selmon)
				selmon = mons;
			cleanupmon(m);
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->client, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void
updatestatus(void)
{
	char text[512];
	char *e;

	if (!gettextprop(root, XA_WM_NAME, text, sizeof(text))) {
		strcpy(stext, "dwm-"VERSION);
		eb.text[0] = '\0';
	} else {
		if ((e = strchr(text, ';')) != NULL) {
			*e++ = '\0';
			strncpy(eb.text, e, sizeof(eb.text) - 1);
			eb.text[sizeof(eb.text) - 1] = '\0';
		} else
			eb.text[0] = '\0';
		strncpy(stext, text, sizeof(stext) - 1);
		stext[sizeof(stext) - 1] = '\0';
	}
	drawbar(selmon);
}



void
updatetitle(Client *c)
{
	if (!gettextprop(c->client, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->client, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updatewindowtype(Client *c)
{
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (hasatomprop(c, netatom[NetWMState], netatom[NetWMFullscreen]))
		setfullscreen(c, 1);
	else if (c->isfullscreen)
		setfullscreen(c, 0);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->client))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->client, wmh);
			c->isurgent = 0;
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		drawborder(c, c == selmon->sel);
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
applytag(Monitor *m)
{
	Pertag *p = m->pertag;
	Client *c;

	m->nmaster = p->nmasters[p->curtag];
	m->mfact = p->mfacts[p->curtag];
	m->sellt = p->sellts[p->curtag];
	m->lt[0] = p->ltidxs[p->curtag][0];
	m->lt[1] = p->ltidxs[p->curtag][1];

	if (!layout_supports_shading(m->lt[m->sellt]))
		for (c = m->clients; c; c = c->next)
			unshade(c);

	if (m->showbar != p->showbars[p->curtag]) {
		m->showbar = p->showbars[p->curtag];
		updatebarpos(m);
		XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
	}
}

void
view(const Arg *arg)
{
	unsigned int i, tagset, tmptag;

	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		tagset = arg->ui & TAGMASK;
		selmon->tagset[selmon->seltags] = tagset;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		if (tagset == TAGMASK)
			selmon->pertag->curtag = 0;
		else {
			for (i = 0; !(tagset & (1u << i)); i++);
			selmon->pertag->curtag = i + 1;
		}
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}
	applytag(selmon);
	focus(NULL);
	arrange(selmon);
}

/*
Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}
*/

Client *
wintoclient(Window w)
{
        Client *c;
        Monitor *m;

        for (m = mons; m; m = m->next)
                for (c = m->clients; c; c = c->next)
                        if (c->win == w || c->client == w)
                                return c;
        return NULL;
}




Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;
	Client *at, *tmp;

	if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
		return;
	if (c == nexttiled(selmon->clients)) {
		for (tmp = selmon->clients; tmp && tmp != prevzoom; tmp = tmp->next);
		if (tmp != prevzoom)
			prevzoom = NULL;
		if (!(c = nexttiled(prevzoom))) {
			c = selmon->sel;
			if (!c || !(c = nexttiled(c->next)))
				return;
		}
	}
	for (at = selmon->clients; at && at->next && at != c && at->next != c;
	     at = nexttiled(at->next));
	pop(c);
	/* swap windows instead of pushing the previous one down */
	if (at && at != c) {
		tmp = prevzoom = c->next;
		if (c->next != at) {
			c->next = tmp->next;
			tmp->next = at->next;
			at->next = tmp;
			arrange(c->mon);
		}
	}
}

XftFont *
rotatedfont(Fnt *base, const char *text, int maxwidth)
{
	FcPattern *pattern;
	FcMatrix matrix;
	XftFont *font;
	XGlyphInfo ext;
	double px;
	int avail, target, next;
	size_t len = text ? strlen(text) : 0;

	if (!base || !base->xfont)
		return NULL;
	avail = MAX(1, maxwidth - 2);
	target = avail;

	if (FcPatternGetDouble(base->xfont->pattern, FC_PIXEL_SIZE, 0, &px) != FcResultMatch)
		px = base->xfont->height;
	if (px < target)
		target = MAX(6, (int)px);

	for (;;) {
		pattern = FcPatternDuplicate(base->xfont->pattern);
		if (!pattern)
			return NULL;

		FcMatrixInit(&matrix);
		FcMatrixRotate(&matrix, 0.0, 1.0);
		FcPatternAddMatrix(pattern, FC_MATRIX, &matrix);
		FcPatternDel(pattern, FC_PIXEL_SIZE);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, target);

		font = XftFontOpenPattern(dpy, pattern);
		FcPatternDestroy(pattern);
		if (!font)
			return NULL;
		if (!len)
			return font;

		XftTextExtentsUtf8(dpy, font, (const FcChar8 *)text, len, &ext);
		if ((int)ext.width <= avail || target <= 6)
			return font;

		next = (int)((double)target * avail / (double)ext.width);
		next = MIN(next, target - 1);
		next = MAX(next, 6);
		XftFontClose(dpy, font);
		if (next == target)
			return NULL;
		target = next;
	}
}

XftFont *
scaledfont(Fnt *base, int maxheight)
{
	FcPattern *pattern;
	XftFont *font;
	double px;
	int target;

	if (!base || !base->xfont)
		return NULL;
	pattern = FcPatternDuplicate(base->xfont->pattern);
	if (!pattern)
		return NULL;

	target = MAX(6, maxheight - 2);
	if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &px) == FcResultMatch
	&& px > target) {
		FcPatternDel(pattern, FC_PIXEL_SIZE);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, target);
	}

	font = XftFontOpenPattern(dpy, pattern);
	FcPatternDestroy(pattern);
	return font;
}

void
drawshadeframe(Client *c)
{
	XftColor *color;
	XftDraw *draw;
	XftFont *font;
	XGlyphInfo ext;
	XRectangle clip;
	int bs, tx, baseline, top, bottom, available, text_h, pen_x, pen_y;

	XSetForeground(dpy, frame_gc, scheme[SchemeNorm][ColBg].pixel);
	XFillRectangle(dpy, c->win, frame_gc, 0, 0, c->w, c->h);

	bs = c->shadevertical ? MIN(titlebar_width, c->h) : MIN(c->h, c->w);
	if (bs < 1)
		bs = 1;
	XSetForeground(dpy, frame_gc, btncloseclr.pixel);
	XFillRectangle(dpy, c->win, frame_gc, 0, 0, bs, bs);
	XSetForeground(dpy, frame_gc, btnfgclr.pixel);
	if (bs > 6) {
		int m = MAX(2, bs / 5);
		XDrawLine(dpy, c->win, frame_gc, m, m, bs - m - 1, bs - m - 1);
		XDrawLine(dpy, c->win, frame_gc, bs - m - 1, m, m, bs - m - 1);
	}

	if (c->name[0] == '\0' || !drw || !drw->fonts || !drw->fonts->xfont)
		return;
	color = &scheme[SchemeNorm][ColFg];
	draw = XftDrawCreate(dpy, c->win, DefaultVisual(dpy, screen),
		DefaultColormap(dpy, screen));
	if (!draw)
		return;

	if (c->shadevertical) {
		font = rotatedfont(drw->fonts, c->name, titlebar_width);
		if (!font)
			goto done;

		XftTextExtentsUtf8(dpy, font, (const FcChar8 *)c->name,
			strlen(c->name), &ext);
		text_h = (int)ext.height;
		top = bs + 3;
		bottom = c->h - 3;
		available = bottom - top;
		if (available > 0 && text_h > 0) {
			pen_y = text_h > available ? bottom
				: top + (available + text_h) / 2;
			pen_x = (c->w - (int)ext.width) / 2 + (int)ext.x;
			clip.x = 0;
			clip.y = top;
			clip.width = c->w;
			clip.height = available;
			XftDrawSetClipRectangles(draw, 0, 0, &clip, 1);
			XftDrawStringUtf8(draw, color, font, pen_x, pen_y,
				(const FcChar8 *)c->name, strlen(c->name));
		}
		XftFontClose(dpy, font);
	} else {
		font = scaledfont(drw->fonts, c->h);
		if (!font)
			goto done;
		tx = bs + 4;
		if (tx < c->w) {
			XftTextExtentsUtf8(dpy, font, (const FcChar8 *)c->name,
				strlen(c->name), &ext);
			baseline = (c->h - (int)ext.height) / 2 + (int)ext.y;
			clip.x = tx;
			clip.y = 0;
			clip.width = c->w - tx;
			clip.height = c->h;
			XftDrawSetClipRectangles(draw, 0, 0, &clip, 1);
			XftDrawStringUtf8(draw, color, font, tx, baseline,
				(const FcChar8 *)c->name, strlen(c->name));
		}
		XftFontClose(dpy, font);
	}

done:
	XftDrawDestroy(draw);
}

void
drawframe(Client *c, int wintype)
{
	unsigned int sidx;
	int btn_size = titlebar_width;
	int top, bottom, available;
	int button_y;
	int has_button;
	XRectangle clip;
	XftFont *font;
	XftDraw *xftdraw;
	XftColor *xftcolor;
	XGlyphInfo ext;
	int pen_x, pen_y, text_h;

	if (c->isfullscreen)
		return;
	if (c->isshaded) {
		drawshadeframe(c);
		return;
	}

	sidx = c->isurgent ? SchemeUrg : (wintype == 1 ? SchemeSel : SchemeNorm);

	/* background */
	XSetForeground(dpy, frame_gc, scheme[sidx][ColBg].pixel);
	XFillRectangle(dpy, c->win, frame_gc, 0, 0, titlebar_width, c->h);

	/* close button: the whole titlebar width, at the top */
	XSetForeground(dpy, frame_gc, btncloseclr.pixel);
	XFillRectangle(dpy, c->win, frame_gc, 0, 0, btn_size, btn_size);
	XSetForeground(dpy, frame_gc, btnfgclr.pixel);
	{
		int m = MAX(2, btn_size / 5);
		XDrawLine(dpy, c->win, frame_gc, m, m, btn_size - m - 1, btn_size - m - 1);
		XDrawLine(dpy, c->win, frame_gc, btn_size - m - 1, m, m, btn_size - m - 1);
	}

	has_button = c->h >= 2 * btn_size + 4;
	button_y = c->h - btn_size;

	if (has_button) {
		int cx = btn_size / 2;
		int cy = button_y + btn_size / 2;
		int r = MAX(2, btn_size / 3);

		XSetForeground(dpy, frame_gc, btnplusclr.pixel);
		XFillRectangle(dpy, c->win, frame_gc, 0, button_y, btn_size, btn_size);
		XSetForeground(dpy, frame_gc, btnfgclr.pixel);
		XDrawArc(dpy, c->win, frame_gc, cx - r, cy - r,
			2 * r, 2 * r, 0, 360 * 64);
	}

	if (c->name[0] == '\0' || !drw || !drw->fonts || !drw->fonts->xfont)
		return;

	font = rotatedfont(drw->fonts, c->name, titlebar_width);
	if (!font)
		return;

	xftdraw = XftDrawCreate(dpy, c->win, DefaultVisual(dpy, screen),
		DefaultColormap(dpy, screen));
	if (!xftdraw) {
		XftFontClose(dpy, font);
		return;
	}

	xftcolor = &scheme[sidx][ColFg];

	/*
	 * With the rotated font XGlyphInfo is rotated too:
	 * ext.width is the thickness of the text (X axis), ext.height is
	 * its real length along the titlebar (Y axis).  The text is drawn
	 * upwards from the baseline, so anchor it at the bottom.
	 */
	XftTextExtentsUtf8(dpy, font, (const FcChar8 *)c->name, strlen(c->name), &ext);
	text_h = (int)ext.height;

	top = btn_size + 4;
	bottom = (has_button ? button_y : c->h) - 4;
	available = bottom - top;
	if (available > 0 && text_h > 0) {
		if (text_h > available)
			pen_y = bottom;
		else
			pen_y = top + (available + text_h) / 2;

		/* Rotated glyphs extend to the left of the baseline. */
		pen_x = (titlebar_width - (int)ext.width) / 2 + (int)ext.x;

		clip.x = 0;
		clip.y = top;
		clip.width = titlebar_width;
		clip.height = available;
		XftDrawSetClipRectangles(xftdraw, 0, 0, &clip, 1);
		XftDrawStringUtf8(xftdraw, xftcolor, font, pen_x, pen_y,
			(const FcChar8 *)c->name, strlen(c->name));
	}

	XftDrawDestroy(xftdraw);
	XftFontClose(dpy, font);
}


void
killclient_direct(Client *c)
{
        XKillClient(dpy, c->client);
        unmanage(c, 1);
}


void
sigterm(int unused)
{
        int saved_errno = errno;
        char c = 0;

        running = 0;
        if (sigpipe[1] != -1)
                (void)write(sigpipe[1], &c, 1);
        errno = saved_errno;
}



int
main(int argc, char *argv[])
{
        int flags, i;

        for (i = 1; i < argc; i++) {
                if (!strcmp(argv[i], "-v"))
                        die("dwm-"VERSION);
                else if (!strcmp(argv[i], "-S")) {
                        if (++i >= argc)
                                die("usage: dwm [-v] [-S theme]");
                        themename = argv[i];
                } else
                        die("usage: dwm [-v] [-S theme]");
        }
        if (themename && !applytheme(themename)) {
                fprintf(stderr, "dwm: theme '%s' not found, using default instead\n",
                        themename);
                themename = "default";
                if (!applytheme(themename))
                        die("dwm: default theme is missing from themes.h");
        }

        if (pipe(sigpipe) == -1)
                die("dwm: pipe:");
        flags = fcntl(sigpipe[0], F_GETFL);
        if (flags != -1)
                fcntl(sigpipe[0], F_SETFL, flags | O_NONBLOCK);
        flags = fcntl(sigpipe[1], F_GETFL);
        if (flags != -1)
                fcntl(sigpipe[1], F_SETFL, flags | O_NONBLOCK);

        signal(SIGTERM, sigterm);
        signal(SIGINT, sigterm);

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	checkotherwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
