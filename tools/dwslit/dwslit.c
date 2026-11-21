/*
 * dwslit - classic *box-style dockapp slit for dwm-dworf.
 *
 * This is an independent implementation of the Slit idea.  It does not
 * contain code copied from Openbox or Blackbox; the protocol behaviour is
 * based on the classic WM_HINTS/WithdrawnState and WM_CLASS "DockApp"
 * conventions.
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#define LENGTH(X) (sizeof X / sizeof X[0])
#define MAX(A, B) ((A) > (B) ? (A) : (B))

typedef struct DockApp DockApp;
struct DockApp {
	Window client;   /* original top-level window */
	Window win;      /* visible window (client or icon window) */
	int w, h;
	int ignore_unmap;
	DockApp *next;
};

static Display *dpy;
static int screen;
static Window root, slit;
static GC gc;
static int slitw, slith;
static int running = 1;
static int sigpipe[2] = { -1, -1 };
static Atom wmDelete, wmProtocols, netWmType, netWmTypeDock;
static unsigned long bgpixel, borderpixel;
static DockApp *apps;
static int xerror(Display *d, XErrorEvent *ee);
static void sigterm(int sig);
static void defaultplacement(int *x, int *y);
static void scan(void);
static void cleanup(void);
static void run(void);

static void (*handler[LASTEvent])(XEvent *) = {
	[ClientMessage] = NULL,
	[ConfigureNotify] = NULL,
	[DestroyNotify] = NULL,
	[Expose] = NULL,
	[MapNotify] = NULL,
	[ReparentNotify] = NULL,
	[UnmapNotify] = NULL,
};

#include "config.h"

static void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

static unsigned long
getcolor(const char *name)
{
	XColor color;

	if (!XAllocNamedColor(dpy, DefaultColormap(dpy, screen), name,
	    &color, &color))
		return WhitePixel(dpy, screen);
	return color.pixel;
}

static void
defaultplacement(int *x, int *y)
{
	XineramaScreenInfo *info;
	Window r, child;
	int rx, ry, wx, wy, n, i;
	unsigned int mask;

	if (!XQueryPointer(dpy, root, &r, &child, &rx, &ry, &wx, &wy, &mask)) {
		rx = DisplayWidth(dpy, screen) - 1;
		ry = 0;
	}

	if (XineramaIsActive(dpy) && (info = XineramaQueryScreens(dpy, &n))) {
		for (i = 0; i < n; i++) {
			if (rx >= info[i].x_org &&
			    rx < info[i].x_org + info[i].width &&
			    ry >= info[i].y_org &&
			    ry < info[i].y_org + info[i].height) {
				*x = info[i].x_org + info[i].width - default_width;
				*y = info[i].y_org;
				XFree(info);
				return;
			}
		}
		*x = info[0].x_org + info[0].width - default_width;
		*y = info[0].y_org;
		XFree(info);
		return;
	}

	*x = DisplayWidth(dpy, screen) - default_width;
	*y = 0;
}

static int
isdockapp(Window w, Window *icon)
{
	XWMHints *h;
	XClassHint ch = { NULL, NULL };
	int found = 0;

	if (icon)
		*icon = None;

	if ((h = XGetWMHints(dpy, w))) {
		if ((h->flags & StateHint) &&
		    h->initial_state == WithdrawnState) {
			found = 1;
			if (icon && (h->flags & IconWindowHint) &&
			    h->icon_window != None)
				*icon = h->icon_window;
		}
		XFree(h);
	}

	if (!found && XGetClassHint(dpy, w, &ch)) {
		if (ch.res_class && !strcmp(ch.res_class, "DockApp"))
			found = 1;
		if (ch.res_name)
			XFree(ch.res_name);
		if (ch.res_class)
			XFree(ch.res_class);
	}

	return found;
}

static DockApp *
findapp(Window w)
{
	DockApp *a;

	for (a = apps; a; a = a->next)
		if (a->client == w || a->win == w)
			return a;
	return NULL;
}

static void
layout(void)
{
	DockApp *a;
	int x = margin, y = margin;

	for (a = apps; a; a = a->next) {
		if (slitw <= slith) {
			x = MAX(0, (slitw - a->w) / 2);
			XMoveResizeWindow(dpy, a->win, x, y, a->w, a->h);
			y += a->h + spacing;
		} else {
			y = MAX(0, (slith - a->h) / 2);
			XMoveResizeWindow(dpy, a->win, x, y, a->w, a->h);
			x += a->w + spacing;
		}
	}
	XClearWindow(dpy, slit);
}

static void
adopt(Window w, Window icon)
{
	DockApp *a;
	XWindowAttributes wa;

	if (w == slit || icon == slit || findapp(w) || (icon && findapp(icon)))
		return;

	a = calloc(1, sizeof(*a));
	if (!a)
		return;

	a->client = w;
	a->win = (icon != None) ? icon : w;
	if (!XGetWindowAttributes(dpy, a->win, &wa)) {
		free(a);
		return;
	}
	a->w = MAX(1, wa.width);
	a->h = MAX(1, wa.height);

	XSetWindowBorderWidth(dpy, a->win, 0);
	XSelectInput(dpy, a->win, StructureNotifyMask | PropertyChangeMask);

	if (a->win != a->client) {
		XMoveWindow(dpy, a->client,
			DisplayWidth(dpy, screen) + 10,
			DisplayHeight(dpy, screen) + 10);
		XMapWindow(dpy, a->client);
		XAddToSaveSet(dpy, a->client);
		XSelectInput(dpy, a->client, StructureNotifyMask);
	}

	XAddToSaveSet(dpy, a->win);
	XReparentWindow(dpy, a->win, slit, 0, 0);
	XMapWindow(dpy, a->win);
	a->ignore_unmap = 2;

	a->next = apps;
	apps = a;
	layout();
	XSync(dpy, False);
}

static void
removeapp(DockApp *a, int remap)
{
	DockApp **p;

	if (!a)
		return;
	for (p = &apps; *p && *p != a; p = &(*p)->next);
	if (*p)
		*p = a->next;

	XSelectInput(dpy, a->win, NoEventMask);
	XRemoveFromSaveSet(dpy, a->win);
	if (remap) {
		XReparentWindow(dpy, a->win, root, 0, 0);
		XMapWindow(dpy, a->win);
	}

	if (a->win != a->client) {
		XSelectInput(dpy, a->client, NoEventMask);
		XRemoveFromSaveSet(dpy, a->client);
		if (remap) {
			XUnmapWindow(dpy, a->client);
			XReparentWindow(dpy, a->client, root, 0, 0);
		}
	}

	free(a);
	layout();
}

static void
scan(void)
{
	Window d1, d2, *wins = NULL, icon;
	unsigned int n, i;

	if (!XQueryTree(dpy, root, &d1, &d2, &wins, &n))
		return;
	for (i = 0; i < n; i++) {
		if (wins[i] == slit)
			continue;
		if (isdockapp(wins[i], &icon))
			adopt(wins[i], icon);
	}
	if (wins)
		XFree(wins);
}

static void
expose(XEvent *e)
{
	if (e->xexpose.window == slit && e->xexpose.count == 0) {
		XSetForeground(dpy, gc, bgpixel);
		XFillRectangle(dpy, slit, gc, 0, 0, slitw, slith);
	}
}

static void
configurenotify(XEvent *e)
{
	DockApp *a;
	XWindowAttributes wa;

	if (e->xconfigure.window == slit) {
		slitw = MAX(1, e->xconfigure.width);
		slith = MAX(1, e->xconfigure.height);
		layout();
		return;
	}

	a = findapp(e->xconfigure.window);
	if (a && XGetWindowAttributes(dpy, a->win, &wa)) {
		if (a->w != wa.width || a->h != wa.height) {
			a->w = MAX(1, wa.width);
			a->h = MAX(1, wa.height);
			layout();
		}
	}
}

static void
mapnotify(XEvent *e)
{
	Window icon;

	if (e->xmap.window == slit || findapp(e->xmap.window))
		return;
	if (isdockapp(e->xmap.window, &icon))
		adopt(e->xmap.window, icon);
}

static void
unmapnotify(XEvent *e)
{
	DockApp *a;
	XWindowAttributes wa;

	a = findapp(e->xunmap.window);
	if (!a)
		return;
	if (a->ignore_unmap) {
		a->ignore_unmap--;
		return;
	}
	if (XGetWindowAttributes(dpy, a->win, &wa) &&
	    wa.map_state == IsUnmapped)
		removeapp(a, 0);
}

static void
reparentnotify(XEvent *e)
{
	DockApp *a;

	if (e->xreparent.parent == slit)
		return;
	a = findapp(e->xreparent.window);
	if (a)
		removeapp(a, 0);
}

static void
destroynotify(XEvent *e)
{
	DockApp *a = findapp(e->xdestroywindow.window);

	if (a)
		removeapp(a, 0);
}

static void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;

	if (cme->message_type == wmProtocols &&
	    (Atom)cme->data.l[0] == wmDelete)
		running = 0;
}

static int
xerror(Display *d, XErrorEvent *ee)
{
	(void)d;
	(void)ee;
	return 0;
}

static void
sigterm(int sig)
{
	(void)sig;
	running = 0;
	if (sigpipe[1] >= 0)
		(void)write(sigpipe[1], "", 1);
}

static void
run(void)
{
	fd_set rfds;
	int xfd = ConnectionNumber(dpy);
	int maxfd = xfd > sigpipe[0] ? xfd : sigpipe[0];
	XEvent ev;

	while (running) {
		FD_ZERO(&rfds);
		FD_SET(xfd, &rfds);
		FD_SET(sigpipe[0], &rfds);
		if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (FD_ISSET(sigpipe[0], &rfds))
			break;
		while (XPending(dpy)) {
			XNextEvent(dpy, &ev);
			if (handler[ev.type])
				handler[ev.type](&ev);
		}
	}
}

static void
cleanup(void)
{
	while (apps)
		removeapp(apps, 1);
	if (slit)
		XDestroyWindow(dpy, slit);
	if (gc)
		XFreeGC(dpy, gc);
	XSync(dpy, False);
}

int
main(int argc, char *argv[])
{
	XSetWindowAttributes wa;
	XClassHint ch = { NULL, NULL };
	XWMHints *wmh;
	XSizeHints *sizeh;
	int i, x = 0, y = 0;
	unsigned int w = default_width, h = default_height;
	int geo = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			printf("dwslit-"VERSION"\n");
			return 0;
		} else if (!strcmp(argv[i], "-h")) {
			fprintf(stderr, "usage: %s [-g WxH+X+Y] [-v]\n", argv[0]);
			return 1;
		} else if (!strcmp(argv[i], "-g") && i + 1 < argc) {
			int mask = XParseGeometry(argv[++i], &x, &y, &w, &h);
			geo = 1;
			if (!(mask & WidthValue))
				w = default_width;
			if (!(mask & HeightValue))
				h = default_height;
		} else {
			fprintf(stderr, "%s: unknown option %s\n", argv[0], argv[i]);
			return 1;
		}
	}
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwslit: cannot open display\n");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	if (!geo)
		defaultplacement(&x, &y);
	slitw = MAX(1, (int)w);
	slith = MAX(1, (int)h);

	if (pipe(sigpipe) < 0)
		die("dwslit: pipe: %s\n", strerror(errno));
	fcntl(sigpipe[0], F_SETFL, O_NONBLOCK);
	fcntl(sigpipe[1], F_SETFL, O_NONBLOCK);
	signal(SIGINT, sigterm);
	signal(SIGTERM, sigterm);
	XSetErrorHandler(xerror);

	wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmProtocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	netWmType = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netWmTypeDock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);

	gc = XCreateGC(dpy, root, 0, NULL);
	bgpixel = getcolor(bgcolor);
	borderpixel = getcolor(bordercolor);

	wa.override_redirect = False;
	wa.border_pixel = borderpixel;
	wa.background_pixel = bgpixel;
	wa.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask;
	slit = XCreateWindow(dpy, root, x, y, slitw, slith, border,
		DefaultDepth(dpy, screen), InputOutput,
		DefaultVisual(dpy, screen),
		CWOverrideRedirect | CWBorderPixel | CWBackPixel | CWEventMask,
		&wa);
	XStoreName(dpy, slit, "dwslit");
	ch.res_name = "dwslit";
	ch.res_class = "dwslit";
	XSetClassHint(dpy, slit, &ch);
	XSetWMProtocols(dpy, slit, &wmDelete, 1);

	wmh = XAllocWMHints();
	if (wmh) {
		wmh->flags = InputHint;
		wmh->input = False;
		XSetWMHints(dpy, slit, wmh);
		XFree(wmh);
	}
	sizeh = XAllocSizeHints();
	if (sizeh) {
		sizeh->flags = PSize;
		sizeh->width = slitw;
		sizeh->height = slith;
		XSetWMNormalHints(dpy, slit, sizeh);
		XFree(sizeh);
	}
	XChangeProperty(dpy, slit, netWmType, XA_ATOM, 32, PropModeReplace,
		(unsigned char *)&netWmTypeDock, 1);

	handler[ClientMessage] = clientmessage;
	handler[ConfigureNotify] = configurenotify;
	handler[DestroyNotify] = destroynotify;
	handler[Expose] = expose;
	handler[MapNotify] = mapnotify;
	handler[ReparentNotify] = reparentnotify;
	handler[UnmapNotify] = unmapnotify;

	XSelectInput(dpy, root, SubstructureNotifyMask);
	XMapRaised(dpy, slit);
	XSetForeground(dpy, gc, bgpixel);
	XFillRectangle(dpy, slit, gc, 0, 0, slitw, slith);
	XSync(dpy, False);

	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return 0;
}
