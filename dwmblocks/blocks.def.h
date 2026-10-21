//Modify this file to change what commands output to your statusbar, and recompile using the make command.
static const Block blocks[] = {
	/*Icon*/	/*Command*/	/*Update Interval*/	/*Update Signal*/
	{"cpu ", "~/.config/dwmblocks/scripts/cpu_temp.sh",	2,	0},
	{"mem ", "~/.config/dwmblocks/scripts/ram_usage.sh",	2,	0},
	{"vol ", "~/.config/dwmblocks/scripts/volume",	1,	0},
	{"bat ", "~/.config/dwmblocks/scripts/battery",	30,	0},
	{"kb ", "~/.config/dwmblocks/scripts/keyboard",	1,	0},
	{"", "~/.config/dwmblocks/scripts/date_time",	1,	0},
};

//sets delimiter between status commands. NULL character ('\0') means no delimiter.
static char delim[] = " | ";
static unsigned int delimLen = 5;
