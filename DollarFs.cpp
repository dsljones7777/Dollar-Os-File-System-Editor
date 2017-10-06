#include "stdafx.h"
#include "DollarFs.h"



DollarFs::~DollarFs()
{
	CloseHandle(file);
	if (rootDescriptor)
	{
		for (unsigned long long i = 0; i < totalRootDescriptors; i++)
			if (rootDescriptor[i])	free((void*)rootDescriptor[i]);
		free((void*)rootDescriptor);
	}
	if (sectorDescriptors)	free((void*)sectorDescriptors);
}
