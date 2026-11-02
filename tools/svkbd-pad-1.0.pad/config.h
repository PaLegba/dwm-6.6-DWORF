#include <X11/XF86keysym.h>

static const Bool wmborder = True;
static const Bool layoutbutton = True;
static const char font[] = "monospace:size=14";

static const char kbdbgcolor[]     = "#E0E0E0";
static const char kbdfgcolor[]     = "#0f0f0f";
static const char normbgcolor[]     = "#00101f";
static const char normfgcolor[]     = "#00F0FF";
static const char pressbgcolor[]      = "#00F0FF";
static const char pressfgcolor[]      = "#222222";
static const char highlightfgcolor[] = "#A03500";
static const char highlightbgcolor[] = "#E08500";

static const char modbgcolor[]     = "#00501f";
static const char modfgcolor[]     = "#00F01F";

static const char escbgcolor[]     = "#A00000";
static const char escfgcolor[]     = "#FFFFA0";

static const char retbgcolor[]     = "#000060";
static const char retfgcolor[]     = "#0090FF";

static const char delbgcolor[]     = "#300000";
static const char delfgcolor[]     = "#F03030";

static const char spcbgcolor[]     = "#F0F030";
static const char spcfgcolor[]     = "#000000";

static const char curbgcolor[]     = "#101F00";
static const char curfgcolor[]     = "#F0F000";

static const char pgbgcolor[] = "#001030";
static const char pgfgcolor[] = "#406080";

static const char closebgcolor[] = "#802500";
static const char closefgcolor[] = "#E08500";

#undef sys_switch
