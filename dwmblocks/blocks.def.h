//Modify this file to change what commands output to your statusbar, and recompile using the make command.
#define KBSIGNAL 11

static const Block blocks[] = {
	/*Icon*/	/*Command*/	/*Update Interval*/	/*Update Signal*/
	{"cpu ", "~/.config/dwmblocks/scripts/cpu_temp.sh",	2,	0},
	{"mem ", "~/.config/dwmblocks/scripts/ram_usage.sh",	2,	0},
	{"vol ", "~/.config/dwmblocks/scripts/volume",	30,	10},
	{"bat ", "~/.config/dwmblocks/scripts/battery",	30,	0},
	{"kb ", "~/.config/dwmblocks/scripts/keyboard",	30,	KBSIGNAL},
	{"", "~/.config/dwmblocks/scripts/date_time",	60,	0},
};

//sets delimiter between status commands. NULL character ('\0') means no delimiter.
static char delim[] = " | ";
static unsigned int delimLen = 5;
