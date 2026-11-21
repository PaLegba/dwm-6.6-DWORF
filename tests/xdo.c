/* small XTest helper used by tests/smoke.sh */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *dpy;

static void
click(int x, int y, int count)
{
	XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);
	XFlush(dpy);
	usleep(20000);
	for (int i = 0; i < count; i++) {
		XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
		XFlush(dpy);
		usleep(10000);
		XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
		XFlush(dpy);
		usleep(20000);
	}
}

static void
key(const char *mods, const char *name)
{
	KeySym ks = XStringToKeysym(name);
	KeyCode kc = XKeysymToKeycode(dpy, ks);
	KeyCode alt = XKeysymToKeycode(dpy, XK_Alt_L);
	KeyCode shift = XKeysymToKeycode(dpy, XK_Shift_L);
	KeyCode ctrl = XKeysymToKeycode(dpy, XK_Control_L);
	int down[3], nd = 0;

	if (!kc) {
		fprintf(stderr, "bad key %s\n", name);
		exit(1);
	}
	if (strstr(mods, "alt"))
		down[nd++] = alt;
	if (strstr(mods, "shift"))
		down[nd++] = shift;
	if (strstr(mods, "ctrl"))
		down[nd++] = ctrl;
	for (int i = 0; i < nd; i++)
		XTestFakeKeyEvent(dpy, down[i], True, CurrentTime);
	XTestFakeKeyEvent(dpy, kc, True, CurrentTime);
	XTestFakeKeyEvent(dpy, kc, False, CurrentTime);
	for (int i = nd - 1; i >= 0; i--)
		XTestFakeKeyEvent(dpy, down[i], False, CurrentTime);
	XFlush(dpy);
	usleep(100000);
}

static Window
findbytitle(Window root, const char *title)
{
	Window d1, d2, *wins = NULL, ret = None;
	unsigned int n;

	if (!XQueryTree(dpy, root, &d1, &d2, &wins, &n))
		return None;
	for (unsigned int i = 0; i < n; i++) {
		char *name = NULL;
		if (XFetchName(dpy, wins[i], &name) && name) {
			if (!strcmp(name, title))
				ret = wins[i];
			XFree(name);
		}
		if (!ret) {
			Window c = findbytitle(wins[i], title);
			if (c)
				ret = c;
		}
	}
	if (wins)
		XFree(wins);
	return ret;
}

static Window
frameof(Window w)
{
	Window root, parent, *children = NULL;
	unsigned int n;

	if (!XQueryTree(dpy, w, &root, &parent, &children, &n))
		return None;
	if (children)
		XFree(children);
	return parent;
}

static int
geom(const char *title, Window *fw, XWindowAttributes *wa)
{
	Window w = findbytitle(DefaultRootWindow(dpy), title);

	if (!w) {
		fprintf(stderr, "window %s not found\n", title);
		return 0;
	}
	*fw = frameof(w);
	if (*fw == DefaultRootWindow(dpy))
		*fw = w;
	if (*fw == None || !XGetWindowAttributes(dpy, *fw, wa)) {
		fprintf(stderr, "frame for %s not found\n", title);
		return 0;
	}
	return 1;
}

static void
shade(const char *title)
{
	Window fw;
	XWindowAttributes wa;
	int x, y;

	if (!geom(title, &fw, &wa))
		exit(1);
	if (wa.height <= 24) {
		x = wa.x + wa.width / 2;
		y = wa.y + wa.height / 2;
	} else {
		x = wa.x + 7;
		y = wa.y + wa.height / 2;
	}
	click(x, y, 2);
	usleep(200000);
}

int
main(int argc, char **argv)
{
	XWindowAttributes wa;
	Window fw;

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "no display\n");
		return 1;
	}
	if (argc >= 4 && !strcmp(argv[1], "click")) {
		click(atoi(argv[2]), atoi(argv[3]), argc > 4 ? atoi(argv[4]) : 1);
	} else if (argc >= 4 && !strcmp(argv[1], "key")) {
		key(argv[2], argv[3]);
	} else if (argc >= 3 && !strcmp(argv[1], "shade")) {
		shade(argv[2]);
	} else if (argc >= 3 && !strcmp(argv[1], "geom")) {
		if (geom(argv[2], &fw, &wa))
			printf("0x%lx %d %d %d %d\n", fw, wa.x, wa.y,
				wa.width, wa.height);
	} else {
		fprintf(stderr,
			"usage: %s click X Y [N] | key MODS KEY | shade TITLE | geom TITLE\n",
			argv[0]);
		return 1;
	}
	XCloseDisplay(dpy);
	return 0;
}
