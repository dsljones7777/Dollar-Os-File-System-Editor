// DollarOsCreator.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <Windows.h>
#include "DollarFs.h"

using namespace std;
#ifndef MAX_ADDITIONS
#define MAX_ADDITIONS 25
#endif
inline bool checkParameterLength(int currentIndex, int requiredArgs, int totalIndexes)
{
	if (currentIndex + requiredArgs >= totalIndexes)
		return false;
	return true;
}
inline void addToFileAndDirectoryArray(DollarFs::FileAndDirectory *& arr,int & totalIn, char * directory, char * fileName)
{
	totalIn;
	void * temp = realloc((void*)arr, sizeof(DollarFs::FileAndDirectory) * (totalIn + 1));
	if (!temp)	throw bad_alloc();
	arr = (DollarFs::FileAndDirectory *)temp;
	arr[totalIn](directory, fileName);
	totalIn++;
}

struct Options
{
	unsigned int checkForFilesystem : 1;
	unsigned int createFileSystem : 1;
};



int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		//Return an error code
		cout << "Incorrect number of arguments. Use /h or /? to display help\n";
		return -1;
	}

	DollarFs * fs = nullptr;
	char * vhdFile = nullptr;
	DollarFs::FileAndDirectory * additions = nullptr;
	DollarFs::FileAndDirectory * removals = nullptr;
	int totalAdditions = 0;
	int totalRemovals = 0;
	char * mbr = nullptr;
	char * sboot = nullptr;
	Options options;
	try
	{
		//Interpret the command line
		vhdFile = argv[1];
		for (int i = 2; i < argc; i++)
		{
			if (argv[i][0] == '+')			addToFileAndDirectoryArray(additions, totalAdditions, nullptr, argv[i] + 1);
			else if (argv[i][0] == '-')		addToFileAndDirectoryArray(removals, totalRemovals, nullptr, argv[i] + 1);
			else if (argv[i][0] != '/')
			{
				cout << "Invalid options. Use /h or /? to display help\n";
				return -1;
			}
			else
				switch (argv[i][1])
				{
				case 'h':
				case '?':
					cout << "DollarOsCreator vhdFile /options \n"
						<< "Options:				Description\n"
						<< "---------------------------------------------------------------------------------------------------\n"
						<< "+filename               Adds the specified file to root directory\n"
						<< "-filename               Removes the specified file from the root directory\n"
						<< "/m filename             Places the specified file as the first boot loader. Must be less than 511 bytes\n"
						<< "/e filename				Places the specified file as the second boot loader. Must be less than 64k\n"
						<< "/i                      Create a new file system header\n"
						<< "/c                      Check for a file system header. If one does not exists one is created\n"
						<< "/+ directory filename   Add the specified file to the directory. Creates the directory if needed\n"
						<< "/- filename	            Removes the specified file/folder \n";
						return 0;
				case 'm':
					if (!checkParameterLength(i, 1, argc))
					{
						cout << "The first boot loader file is missing from the command line. Use /h or /? to display help\n";
						return -1;
					}
					mbr = argv[++i];
					break;
				case 'e':
					if (!checkParameterLength(i, 1, argc))
					{
						cout << "The second boot loader file is missing from the command line. Use /h or /? to display help\n";
						return -1;
					}
					sboot = argv[++i];
					break;
				case '+':
					if (!checkParameterLength(i, 2, argc))
					{
						cout << "The directory and/or file to add is missing from the command line. Use /h or /? to display help\n";
						return -1;
					}
					addToFileAndDirectoryArray(additions, totalAdditions, argv[i + 1], argv[i + 2]);
					i += 2;
					break;
				case '-':
					if (!checkParameterLength(i, 1, argc))
					{
						cout << "The file to remove is missing from the command line. Use /h or /? to display help\n";
						return -1;
					}
					addToFileAndDirectoryArray(removals, totalRemovals, argv[++i],nullptr);
					break;
				case 'i':
					options.checkForFilesystem = 1;
				case 'c':
					options.createFileSystem = 1;
					break;
				default:
					cout << "Invalid options. Use /h or /? to display help\n";
					return -1;
				}
		}
		//Check all the files to make sure they exist
		if (GetFileAttributesA(mbr) == INVALID_FILE_ATTRIBUTES)
		{
			cout << "The specified VHD file does not exist";
			return -1;
		}
		for (int i = 0; i < totalAdditions; i++)
		{
			if (GetFileAttributesA(additions[i].fileName) != INVALID_FILE_ATTRIBUTES)	continue;
			cout << "The specified file to add does not exist\n";
			return -1;
		}
		for (int i = 0; i < totalRemovals; i++)
		{
			if (GetFileAttributesA(removals[i].fileName) != INVALID_FILE_ATTRIBUTES)	continue;
			cout << "The specified file to remove does not exist\n";
			return -1;
		}
		if (mbr && GetFileAttributesA(mbr) == INVALID_FILE_ATTRIBUTES)
		{
			cout << "The specified file for the MBR is missing\n";
			return -1;
		}
		if (sboot && GetFileAttributesA(sboot) == INVALID_FILE_ATTRIBUTES)
		{
			cout << "The specified file for the second boot loader is missing\n";
			return -1;
		}
		fs = new DollarFs(vhdFile, options.createFileSystem);
		if (mbr)	fs->createMBR(mbr);
		if (sboot)	fs->writeExBoot(sboot);
		for (int i = 0; i < totalRemovals; i++)
			if (!fs->removeFile(&removals[i]))		cerr << "The specified file/folder ( " << removals[i].fileName << " could not be removed. It does not exist";
		for (int i = 0; i < totalAdditions; i++)
		{
			if (fs->addFile(&removals[i]))			continue;
			for (int j = i; j < totalAdditions; j++)
				cerr << "The specified file/folder ( " << additions[i].fileName << " was not be added";
			return -1;
		}
		fs->flush();
		return 0;
	}
	catch(DWORD err)
	{
		delete fs;
		if(additions)							free((void*)additions);
		if(removals)							free((void*)removals);
		cout << "A fatal error occurred: " << err << "\n";
		int i = 0;
		cin >> i;
		return -1;
	}
	catch (...)
	{
		if(fs)	delete fs;
		if (additions)							free((void*)additions);
		if (removals)							free((void*)removals);
		cout << "An unknown fatal error occurred\n";
		int i = 0;
		cin >> i;
		return -1;
	}
	delete fs;
	if (additions)							free((void*)additions);
	if (removals)							free((void*)removals);
	int z = 0;
	cin >> z;
	return 0;
}

