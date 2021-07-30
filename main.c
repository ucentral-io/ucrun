#include "urun.h"

static struct urun urun;

int main(int argc, const char **argv)
{
	if (argc < 2)
		return -1;

	uloop_init();
	ucode_init(&urun, argv[1]);
	uloop_run();

	return 0;
}
