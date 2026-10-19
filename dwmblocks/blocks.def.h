//Modify this file to change what commands output to your statusbar, and recompile using the make command.
static const Block blocks[] = {
	/*Icon*/	/*Command*/	/*Update Interval*/	/*Update Signal*/
	{"󰍛 ", "~/.config/dwmblocks/scripts/ram_usage.sh",	2,	0},
	{"󰔄 ", "~/.config/dwmblocks/scripts/ram_temp.sh",	5,	0},
	{"󰋛 ", "~/.config/dwmblocks/scripts/cpu_temp.sh",	2,	0},
	{"󰢮 ", "~/.config/dwmblocks/scripts/gpu_temp.sh",	2,	0},
	{"󰾲 ", "~/.config/dwmblocks/scripts/vram_usage.sh",	2,	0},
	{"󰾴 ", "~/.config/dwmblocks/scripts/vram_temp.sh",	5,	0},
	{" ", "~/.config/dwmblocks/scripts/volume",	1,	0},
	{" ", "~/.config/dwmblocks/scripts/battery",	30,	0},
	{" ", "~/.config/dwmblocks/scripts/date_time",	1,	0},
};

//sets delimiter between status commands. NULL character ('\0') means no delimiter.
static char delim[] = " | ";
static unsigned int delimLen = 5;
