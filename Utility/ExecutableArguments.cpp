#include "ExecutableArguments.h"

ExecutableArguments::ExecutableArguments(int argc, char** argv)
{
	for (int a = 0; a < argc; a++)
	{
		//Lot of redundancy here in case someone doesn't know what to use
		if (strcmp(argv[a],"dedi") == 0)
		{
			dedicated = true;
			continue;
		}
		if (strcmp(argv[a], "-dedi") == 0)
		{
			dedicated = true;
			continue;
		}
		if (strcmp(argv[a], "dedicated") == 0)
		{
			dedicated = true;
			continue;
		}
		if (strcmp(argv[a], "-dedicated") == 0)
		{
			dedicated = true;
			continue;
		}


		if (strcmp(argv[a], "silent") == 0)
		{
			silentMode = true;
			continue;
		}
		if (strcmp(argv[a], "-silent") == 0)
		{
			silentMode = true;
			continue;
		}

		if (strcmp(argv[a], "singleplayer") == 0)
		{
			autoSinglePlayer = true;
			continue;
		}
		if (strcmp(argv[a], "-singleplayer") == 0)
		{
			autoSinglePlayer = true;
			continue;
		}
	}
}

