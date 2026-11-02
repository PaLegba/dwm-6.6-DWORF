#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXICONS 64
#define XEMBED_MAPPED (1 << 0)

typedef struct {
	Window win;
	int index;
	unsigned long pixel;
} Icon;

static Display *dpy;
static Window root;
static Atom trayop, xembedinfo;
static XFontStruct *font;
static Icon icons[MAXICONS];
static int nicons;
static int running = 1;
static int screen;

static void
die(const char *msg)
{
	fprintf(stderr, "traytest: %s\n", msg);
	exit(1);
}

static unsigned long
getcolor(const char *name)
{
	XColor c;
	Colormap cmap = DefaultColormap(dpy, screen);

	if (!XAllocNamedColor(dpy, cmap, name, &c, &c))
		return WhitePixel(dpy, screen);
	return c.pixel;
}

static void
draw_icon(Icon *icon)
{
	XWindowAttributes wa;
	char label[16];
	int tw, th;

	if (!XGetWindowAttributes(dpy, icon->win, &wa))
		return;
	XSetForeground(dpy, DefaultGC(dpy, screen), icon->pixel);
	XFillRectangle(dpy, icon->win, DefaultGC(dpy, screen), 0, 0, wa.width, wa.height);
	XSetForeground(dpy, DefaultGC(dpy, screen), BlackPixel(dpy, screen));
	XDrawRectangle(dpy, icon->win, DefaultGC(dpy, screen), 0, 0,
	               wa.width > 0 ? wa.width - 1 : 0,
	               wa.height > 0 ? wa.height - 1 : 0);
	if (font) {
		snprintf(label, sizeof label, "%d", icon->index + 1);
		tw = XTextWidth(font, label, strlen(label));
		th = font->ascent + font->descent;
		XSetFont(dpy, DefaultGC(dpy, screen), font->fid);
		XDrawString(dpy, icon->win, DefaultGC(dpy, screen),
		            (wa.width - tw) / 2,
		            (wa.height + font->ascent - font->descent) / 2,
		            label, strlen(label));
		(void)th;
	}
	XFlush(dpy);
}

static void
send_dock(Window owner, Window win)
{
	XClientMessageEvent ev;

	memset(&ev, 0, sizeof ev);
	ev.type = ClientMessage;
	ev.window = owner;
	ev.message_type = trayop;
	ev.format = 32;
	ev.data.l[0] = CurrentTime;
	ev.data.l[1] = 0; /* SYSTEM_TRAY_REQUEST_DOCK */
	ev.data.l[2] = win;
	XSendEvent(dpy, owner, False, NoEventMask, (XEvent *)&ev);
	XFlush(dpy);
}

static void
remove_icon(int index)
{
	int i;

	XDestroyWindow(dpy, icons[index].win);
	for (i = index; i < nicons - 1; i++)
		icons[i] = icons[i + 1];
	nicons--;
}

int
main(int argc, char *argv[])
{
	const char *colors[] = {
		"#c04040", "#40a040", "#4060c0", "#c0a040",
		"#a040a0", "#40a0a0", "#c06020", "#808080",
		"#c08080", "#80c080", "#8080c0", "#c0c080"
	};
	int n = 8, i, sizes[4] = { 16, 24, 32, 40 };
	Window owner;
	XSetWindowAttributes wa;
	XEvent ev;
	long info[2] = { 0, XEMBED_MAPPED };

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-n") && i + 1 < argc) {
			n = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-h")) {
			printf("usage: %s [-n count]\n", argv[0]);
			return 0;
		} else {
			die("unknown option");
		}
	}
	if (n < 1)
		n = 1;
	if (n > MAXICONS)
		n = MAXICONS;

	if (!(dpy = XOpenDisplay(NULL)))
		die("cannot open display");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	font = XLoadQueryFont(dpy, "fixed");

	trayop = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	xembedinfo = XInternAtom(dpy, "_XEMBED_INFO", False);
	owner = XGetSelectionOwner(dpy, XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False));
	if (!owner)
		die("no XEmbed systray owner; start dwtoolset first");

	for (i = 0; i < n; i++) {
		wa.override_redirect = True;
		wa.background_pixel = getcolor(colors[i % (int)(sizeof colors / sizeof colors[0])]);
		wa.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask;
		icons[i].win = XCreateWindow(dpy, root, 0, 0,
		                             sizes[i % 4], sizes[i % 4], 1,
		                             DefaultDepth(dpy, screen), InputOutput,
		                             DefaultVisual(dpy, screen),
		                             CWOverrideRedirect | CWBackPixel | CWEventMask,
		                             &wa);
		icons[i].index = i;
		icons[i].pixel = wa.background_pixel;
		XSelectInput(dpy, icons[i].win,
		             ExposureMask | StructureNotifyMask | ButtonPressMask);
		XChangeProperty(dpy, icons[i].win, xembedinfo, xembedinfo, 32,
		                PropModeReplace, (unsigned char *)info, 2);
		XStoreName(dpy, icons[i].win, "traytest");
		XMapWindow(dpy, icons[i].win);
		send_dock(owner, icons[i].win);
	}
	nicons = n;
	XSync(dpy, False);

	while (running) {
		XNextEvent(dpy, &ev);
		if (ev.type == Expose && ev.xexpose.count == 0) {
			for (i = 0; i < nicons; i++)
				if (icons[i].win == ev.xexpose.window)
					draw_icon(&icons[i]);
		} else if (ev.type == ConfigureNotify) {
			for (i = 0; i < nicons; i++)
				if (icons[i].win == ev.xconfigure.window)
					draw_icon(&icons[i]);
		} else if (ev.type == ButtonPress) {
			for (i = 0; i < nicons; i++)
				if (icons[i].win == ev.xbutton.window) {
					remove_icon(i);
					break;
				}
			if (!nicons)
				running = 0;
		}
	}

	for (i = 0; i < nicons; i++)
		XDestroyWindow(dpy, icons[i].win);
	XCloseDisplay(dpy);
	return 0;
}
