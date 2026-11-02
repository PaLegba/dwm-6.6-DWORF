#include <X11/keysym.h>

#define MODKEY Mod1Mask

static const unsigned int panel_width  = 24;
static const unsigned int icon_size    = 32;
static const unsigned int icon_margin  = 4;
static const unsigned int icon_spacing = 4;
static unsigned int systray_size       = 16;

static const char *iconpath = ICONPATH;

static const char bgcolor[]    = "#00101f";
static const char fgcolor[]    = "#00F0FF";
static const char presscolor[] = "#005f7f";
static const char sysbgcolor[] = "#00101f";

static const char *termcmd[] = { "st", "-R", "on", NULL };
static const char *svkbdcall[] = {
	"svkbd", "-d", "-S", "default", "-l", "en.full", NULL
};

/*
 * External commands are the same idea: add a tool with A_SPAWN and an argv.
 * Examples:
 * static const char *volup[] = { "sh", "-c", "pamixer -i 5", NULL };
 * static const char *voldown[] = { "sh", "-c", "pamixer -d 5", NULL };
 */

typedef enum {
	A_SPAWN,
	A_KEYS
} Action;

typedef struct {
	const char *icon;
	const char *label;
	Action action;
	const char *const *argv;
	unsigned int mod;
	KeySym keysym;
} Tool;

static const Tool tools[] = {
	{ "dwm.term.new.xbm", "term", A_SPAWN, termcmd, 0, 0 },
	{ "dwm.win.zoom.xbm", "zoom", A_KEYS, NULL, MODKEY, XK_Return },
	{ "dwm.stack.up.xbm", "up", A_KEYS, NULL, MODKEY|ShiftMask, XK_k },
	{ "dwm.stack.down.xbm", "down", A_KEYS, NULL, MODKEY|ShiftMask, XK_j },
	{ "dwm.main.dec.xbm", "m-", A_KEYS, NULL, MODKEY, XK_h },
	{ "dwm.main.inc.xbm", "m+", A_KEYS, NULL, MODKEY, XK_l },
	{ "dwm.svkbd.xbm", "kbd", A_SPAWN, svkbdcall, 0, 0 },
};

static const Bool showsystray = True;
static const unsigned int systray_spacing = 4;
