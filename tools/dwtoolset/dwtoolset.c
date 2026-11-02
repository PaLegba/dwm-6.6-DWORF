#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#define LENGTH(X) (sizeof X / sizeof X[0])
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define SYSTEM_TRAY_REQUEST_DOCK 0
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_FOCUS_IN 4
#define XEMBED_MAPPED (1 << 0)

enum {
	NetSystemTray,
	NetSystemTrayOP,
	NetSystemTrayOrientation,
	NetLast
};

enum {
	XEmbed,
	XEmbedInfo,
	Manager,
	XLast
};

typedef struct TrayIcon TrayIcon;
struct TrayIcon {
	Window win;
	int w, h;
	int mapped;
	TrayIcon *next;
};

static Display *dpy;
static int screen;
static Window root, panel, traywin;
static GC gc;
static XFontStruct *font;
static int panelw, panelh;
static int tray_active = 0;
static int running = 1;
static int sigpipe[2] = { -1, -1 };
static Atom netatom[NetLast], xatom[XLast];
static TrayIcon *icons = NULL;

static void die(const char *fmt, ...);
static void spawn(const char *const *argv);
static void sendkeys(unsigned int mod, KeySym sym);
static void run_tool(int index);
static unsigned long getcolor(const char *name);
static void draw_tool(int index, int x, int y, int w, int h);
static void draw_tools(void);
static void layout(void);
static void add_icon(Window win);
static void remove_icon(TrayIcon *icon);
static void send_xembed(Window win, long message, long detail, long data1, long data2);
static void setup_systray(void);
static void cleanup(void);
static void run(void);
static int xerror(Display *d, XErrorEvent *ee);
static void configurenotify(XEvent *e);
static void expose(XEvent *e);
static void buttonpress(XEvent *e);
static void clientmessage(XEvent *e);
static void destroynotify(XEvent *e);
static void selectionclear(XEvent *e);
static void sigterm(int sig);

static void (*handler[LASTEvent])(XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[Expose] = expose,
	[SelectionClear] = selectionclear,
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

static void
spawn(const char *const *argv)
{
	if (!argv || !argv[0])
		return;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(argv[0], (char *const *)argv);
		perror("dwtoolset: execvp");
		exit(1);
	}
}

static void
sendkeys(unsigned int mod, KeySym sym)
{
	KeyCode mods[4];
	KeyCode key;
	int i, n = 0;

	if ((key = XKeysymToKeycode(dpy, sym)) == 0)
		return;
	if (mod & ShiftMask)
		mods[n++] = XKeysymToKeycode(dpy, XK_Shift_L);
	if (mod & ControlMask)
		mods[n++] = XKeysymToKeycode(dpy, XK_Control_L);
	if (mod & Mod1Mask)
		mods[n++] = XKeysymToKeycode(dpy, XK_Alt_L);
	if (mod & Mod4Mask)
		mods[n++] = XKeysymToKeycode(dpy, XK_Super_L);

	for (i = 0; i < n; i++)
		if (mods[i])
			XTestFakeKeyEvent(dpy, mods[i], True, CurrentTime);
	XTestFakeKeyEvent(dpy, key, True, CurrentTime);
	XTestFakeKeyEvent(dpy, key, False, CurrentTime);
	for (i = n - 1; i >= 0; i--)
		if (mods[i])
			XTestFakeKeyEvent(dpy, mods[i], False, CurrentTime);
	XFlush(dpy);
}

static void
run_tool(int index)
{
	const Tool *t = &tools[index];

	if (t->action == A_SPAWN)
		spawn(t->argv);
	else if (t->action == A_KEYS)
		sendkeys(t->mod, t->keysym);
}

static void
draw_tool(int index, int x, int y, int w, int h)
{
	const Tool *t = &tools[index];
	Pixmap pm;
	unsigned int pw, ph;
	int px, py, ix, iy, tw;
	char path[512];

	XSetForeground(dpy, gc, getcolor(bgcolor));
	XFillRectangle(dpy, panel, gc, x, y, w, h);
	XSetForeground(dpy, gc, getcolor(fgcolor));

	if (t->icon && t->icon[0]) {
		snprintf(path, sizeof path, "%s/%s", iconpath, t->icon);
		if (XReadBitmapFile(dpy, panel, path, &pw, &ph, &pm, &px, &py) == BitmapSuccess) {
			ix = x + (w - (int)pw) / 2;
			iy = y + (h - (int)ph) / 2;
			XSetBackground(dpy, gc, getcolor(bgcolor));
			XCopyPlane(dpy, pm, panel, gc, px, py, pw, ph, ix, iy, 1);
			XFreePixmap(dpy, pm);
			return;
		}
	}

	if (t->label && font) {
		tw = XTextWidth(font, t->label, strlen(t->label));
		XDrawString(dpy, panel, gc, x + (w - tw) / 2,
		           y + (h + font->ascent - font->descent) / 2,
		           t->label, strlen(t->label));
	}
}

static unsigned long
getcolor(const char *name)
{
	XColor c;
	Colormap cmap = DefaultColormap(dpy, screen);

	if (!XAllocNamedColor(dpy, cmap, name, &c, &c))
		die("dwtoolset: cannot allocate color '%s'\n", name);
	return c.pixel;
}

static void
draw_tools(void)
{
	int i, cellh = icon_size + icon_spacing;
	int y;

	XSetForeground(dpy, gc, getcolor(bgcolor));
	XFillRectangle(dpy, panel, gc, 0, 0, panelw, panelh);
	for (i = 0; i < (int)LENGTH(tools); i++) {
		y = icon_margin + i * cellh;
		draw_tool(i, icon_margin, y, panelw - 2 * icon_margin, icon_size);
	}
}

static void
layout(void)
{
	int iconw = MIN((int)systray_size, panelw);
	int cellh = iconw + systray_spacing;
	int toolh = icon_margin + (int)LENGTH(tools) * (icon_size + icon_spacing);
	int tray_top = toolh + icon_margin;
	int maxh = panelh - icon_margin - tray_top;
	int n = 0, th, tw, tx, ty, i, iy;
	TrayIcon *ic;

	for (ic = icons; ic; ic = ic->next)
		n++;

	tw = MAX(1, MIN(panelw, iconw + 2 * icon_margin));
	tx = (panelw - tw) / 2;
	th = n ? n * cellh : 1;
	if (maxh < 1)
		maxh = 1;
	if (th > maxh)
		th = maxh;
	ty = panelh - icon_margin - th;

	if (traywin) {
		XMoveResizeWindow(dpy, traywin, tx, ty, tw, th);
		XRaiseWindow(dpy, traywin);
	}

	i = 0;
	for (ic = icons; ic; ic = ic->next, i++) {
		iy = th - (i + 1) * cellh + systray_spacing / 2;
		XMoveResizeWindow(dpy, ic->win, (tw - iconw) / 2, iy,
		                  iconw, iconw);
	}

	draw_tools();
	if (traywin) {
		XClearWindow(dpy, traywin);
		XSync(dpy, False);
	}
}

static void
send_xembed(Window win, long message, long detail, long data1, long data2)
{
	XEvent ev;

	memset(&ev, 0, sizeof ev);
	ev.xclient.type = ClientMessage;
	ev.xclient.window = win;
	ev.xclient.message_type = xatom[XEmbed];
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = CurrentTime;
	ev.xclient.data.l[1] = message;
	ev.xclient.data.l[2] = detail;
	ev.xclient.data.l[3] = data1;
	ev.xclient.data.l[4] = data2;
	XSendEvent(dpy, win, False, NoEventMask, &ev);
}

static void
add_icon(Window win)
{
	TrayIcon *icon, **tail;
	XWindowAttributes wa;

	if (!win || !tray_active || !traywin)
		return;
	if (!XGetWindowAttributes(dpy, win, &wa))
		return;
	for (icon = icons; icon; icon = icon->next)
		if (icon->win == win)
			return;

	if (!(icon = calloc(1, sizeof *icon)))
		return;
	icon->win = win;
	icon->w = wa.width;
	icon->h = wa.height;
	icon->mapped = 1;

	XAddToSaveSet(dpy, win);
	XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask | ExposureMask);
	XSetWindowBorderWidth(dpy, win, 0);
	XReparentWindow(dpy, win, traywin, 0, 0);
	send_xembed(win, XEMBED_EMBEDDED_NOTIFY, 0, traywin, 0);
	send_xembed(win, XEMBED_WINDOW_ACTIVATE, 0, traywin, 0);
	send_xembed(win, XEMBED_FOCUS_IN, 0, traywin, 0);
	XMapRaised(dpy, win);

	for (tail = &icons; *tail; tail = &(*tail)->next)
		;
	*tail = icon;
	layout();
}

static void
remove_icon(TrayIcon *icon)
{
	TrayIcon **pp;

	for (pp = &icons; *pp && *pp != icon; pp = &(*pp)->next)
		;
	if (*pp)
		*pp = icon->next;
	/* The window is already destroyed here; touching it would be BadWindow. */
	free(icon);
	layout();
}

static int
xerror(Display *d, XErrorEvent *ee)
{
	(void)d;
	if (ee->error_code != BadWindow && ee->error_code != BadDrawable)
		fprintf(stderr, "dwtoolset: X error %d (request %d)\n",
		        ee->error_code, ee->request_code);
	return 0;
}

static void
setup_systray(void)
{
	long vert = 1;
	XSetWindowAttributes wa;
	XClientMessageEvent ev;
	Window owner;

	if (!showsystray)
		return;

	wa.background_pixel = getcolor(sysbgcolor);
	wa.event_mask = StructureNotifyMask | SubstructureNotifyMask | ExposureMask |
	                PropertyChangeMask;
	traywin = XCreateWindow(dpy, panel, 0, 0, 1, 1, 0,
	                        CopyFromParent, InputOutput, CopyFromParent,
	                        CWBackPixel | CWEventMask, &wa);
	XMapRaised(dpy, traywin);

	XSetSelectionOwner(dpy, netatom[NetSystemTray], traywin, CurrentTime);
	owner = XGetSelectionOwner(dpy, netatom[NetSystemTray]);
	if (owner != traywin) {
		fprintf(stderr, "dwtoolset: another systray owner is already running\n");
		XDestroyWindow(dpy, traywin);
		traywin = 0;
		return;
	}

	XChangeProperty(dpy, traywin, netatom[NetSystemTrayOrientation],
	                XA_CARDINAL, 32, PropModeReplace,
	                (unsigned char *)&vert, 1);

	memset(&ev, 0, sizeof ev);
	ev.type = ClientMessage;
	ev.window = root;
	ev.message_type = xatom[Manager];
	ev.format = 32;
	ev.data.l[0] = CurrentTime;
	ev.data.l[1] = netatom[NetSystemTray];
	ev.data.l[2] = traywin;
	XSendEvent(dpy, root, False, StructureNotifyMask, (XEvent *)&ev);
	tray_active = 1;
	XSync(dpy, False);
}

static void
configurenotify(XEvent *e)
{
	XConfigureEvent *ev = &e->xconfigure;

	if (ev->window == panel && (ev->width != panelw || ev->height != panelh)) {
		panelw = ev->width;
		panelh = ev->height;
		layout();
	}
}

static void
expose(XEvent *e)
{
	XExposeEvent *ev = &e->xexpose;

	if (ev->count != 0)
		return;
	if (ev->window == panel)
		draw_tools();
	else if (ev->window == traywin) {
		XSetForeground(dpy, gc, getcolor(sysbgcolor));
		XFillRectangle(dpy, traywin, gc, 0, 0, ev->width, ev->height);
	}
}

static void
buttonpress(XEvent *e)
{
	XButtonPressedEvent *ev = &e->xbutton;
	int cellh = icon_size + icon_spacing;
	int i, y;

	if (ev->window != panel || ev->button != Button1)
		return;
	y = ev->y - icon_margin;
	if (y < 0 || y >= (int)LENGTH(tools) * cellh)
		return;
	i = y / cellh;
	if (i >= 0 && i < (int)LENGTH(tools))
		run_tool(i);
}

static void
clientmessage(XEvent *e)
{
	XClientMessageEvent *ev = &e->xclient;

	if (!traywin || ev->window != traywin)
		return;
	if (ev->message_type != netatom[NetSystemTrayOP])
		return;
	if (ev->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK)
		add_icon((Window)ev->data.l[2]);
}

static void
destroynotify(XEvent *e)
{
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	TrayIcon *icon;

	for (icon = icons; icon; icon = icon->next)
		if (icon->win == ev->window) {
			remove_icon(icon);
			return;
		}
}

static void
selectionclear(XEvent *e)
{
	if (e->xselectionclear.window == traywin) {
		fprintf(stderr, "dwtoolset: lost systray selection\n");
		tray_active = 0;
	}
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
	TrayIcon *icon, *next;

	if (traywin && XGetSelectionOwner(dpy, netatom[NetSystemTray]) == traywin)
		XSetSelectionOwner(dpy, netatom[NetSystemTray], None, CurrentTime);
	for (icon = icons; icon; icon = next) {
		next = icon->next;
		if (traywin)
			XReparentWindow(dpy, icon->win, root, 0, 0);
		XRemoveFromSaveSet(dpy, icon->win);
		XMapWindow(dpy, icon->win);
		free(icon);
	}
	if (traywin)
		XDestroyWindow(dpy, traywin);
	if (panel)
		XDestroyWindow(dpy, panel);
	if (gc)
		XFreeGC(dpy, gc);
	if (font)
		XFreeFont(dpy, font);
	XSync(dpy, False);
}

int
main(int argc, char *argv[])
{
	XSetWindowAttributes wa;
	XClassHint ch = { NULL, NULL };
	XWMHints *wmh;
	XSizeHints *sizeh;
	Atom wtype;
	int i, xr, yr, bitm;
	unsigned int wr, hr;
	int panelx = 0, panely = 0;

	panelw = panel_width;
	panelh = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			printf("dwtoolset-"VERSION"\n");
			return 0;
		} else if (!strcmp(argv[i], "-h")) {
			fprintf(stderr, "usage: %s [-g geometry] [-i iconpath] [-s systray_size] [-v]\n", argv[0]);
			return 1;
		} else if (!strcmp(argv[i], "-s")) {
			int size;
			if (i + 1 >= argc)
				die("dwtoolset: -s needs a size\n");
			size = atoi(argv[++i]);
			if (size < 8)
				size = 8;
			if (size > 512)
				size = 512;
			systray_size = (unsigned int)size;
		} else if (!strcmp(argv[i], "-i")) {
			if (i + 1 >= argc)
				die("dwtoolset: -i needs a path\n");
			iconpath = argv[++i];
		} else if (!strcmp(argv[i], "-g")) {
			if (i + 1 >= argc)
				die("dwtoolset: -g needs a geometry\n");
			bitm = XParseGeometry(argv[++i], &xr, &yr, &wr, &hr);
			if (bitm & XValue)
				panelx = xr;
			if (bitm & YValue)
				panely = yr;
			if (bitm & WidthValue)
				panelw = (int)wr;
			if (bitm & HeightValue)
				panelh = (int)hr;
			if ((bitm & XNegative) && panelx == 0)
				panelx = -1;
			if ((bitm & YNegative) && panely == 0)
				panely = -1;
		} else {
			die("dwtoolset: unknown option %s\n", argv[i]);
		}
	}

	if (!(dpy = XOpenDisplay(NULL)))
		die("dwtoolset: cannot open display\n");
	XSetErrorHandler(xerror);
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	if (panelh <= 0)
		panelh = DisplayHeight(dpy, screen);
	if (panelx < 0)
		panelx = DisplayWidth(dpy, screen) + panelx - panelw;
	if (panely < 0)
		panely = DisplayHeight(dpy, screen) + panely - panelh;

	if (pipe(sigpipe) < 0)
		die("dwtoolset: pipe: %s\n", strerror(errno));
	fcntl(sigpipe[0], F_SETFL, O_NONBLOCK);
	fcntl(sigpipe[1], F_SETFL, O_NONBLOCK);
	signal(SIGINT, sigterm);
	signal(SIGTERM, sigterm);

	gc = XCreateGC(dpy, root, 0, NULL);
	if (!(font = XLoadQueryFont(dpy, "fixed")))
		font = NULL;
	else
		XSetFont(dpy, gc, font->fid);

	xatom[XEmbed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XEmbedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	wtype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);

	wa.override_redirect = False;
	wa.border_pixel = getcolor(bgcolor);
	wa.background_pixel = getcolor(bgcolor);
	wa.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask;
	panel = XCreateWindow(dpy, root, panelx, panely, panelw, panelh, 0,
	                      DefaultDepth(dpy, screen), InputOutput,
	                      DefaultVisual(dpy, screen),
	                      CWOverrideRedirect | CWBorderPixel | CWBackPixel | CWEventMask,
	                      &wa);

	XStoreName(dpy, panel, "dwtoolset");
	ch.res_name = "dwtoolset";
	ch.res_class = "dwtoolset";
	XSetClassHint(dpy, panel, &ch);

	wmh = XAllocWMHints();
	if (wmh) {
		wmh->flags = InputHint;
		wmh->input = False;
		XSetWMHints(dpy, panel, wmh);
		XFree(wmh);
	}
	sizeh = XAllocSizeHints();
	if (sizeh) {
		sizeh->flags = PSize;
		sizeh->width = panelw;
		sizeh->height = panelh;
		XSetWMNormalHints(dpy, panel, sizeh);
		XFree(sizeh);
	}
	{
		Atom dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
		XChangeProperty(dpy, panel, wtype, XA_ATOM, 32, PropModeReplace,
		                (unsigned char *)&dock, 1);
	}

	setup_systray();
	XMapRaised(dpy, panel);
	if (traywin)
		XMapRaised(dpy, traywin);
	layout();
	XSync(dpy, False);

	run();
	cleanup();
	XCloseDisplay(dpy);
	return 0;
}
