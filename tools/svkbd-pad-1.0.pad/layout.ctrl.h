#define NO_REPEAT 1
static Key keys[] = {
	{ "↑", XK_Up, 1 },
	{ "↓", XK_Down, 1 },
	{ "←", XK_Left, 1 },
	{ "→", XK_Right, 1 },
	{ "menu", XK_F19, 1 },
	{ "term", XK_F20, 1 },
	{ "kill", XK_F17, 1 },
	{ "prev", XK_F14, 1 },
	{ "next", XK_F15, 1 },
	{ "pushUP", XK_F18,  1 },
	{ "pushDOWN", XK_F16, 1 },
	{ "swap", XK_F13, 1 },
	{ "Esc", XK_Escape, 1 },
	{ "⌧", XK_Cancel, 1},
};

Buttonmod buttonmods[] = {
	{ XK_Super_L, Button2 },
	{ XK_Control_L, Button3 },
};

