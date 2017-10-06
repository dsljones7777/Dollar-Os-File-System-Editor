#pragma once
#include <Windows.h>
#include <string.h>
#include <time.h>

/*
		The layout of DollarFs is as follows.

		

		-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	0	|		MBR				|		Header			|			Null		|		Null			|		Null			|		Null			|		Null			|		Null			|
		-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	1	|																Reserved Ex-Boot 1,152 Sectors (Reserved for Ex-Boot 144 4096)																	|
	?	|																						.........																								|
		-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
? - 144	|																					SectorDescriptors																							|
		-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
? + 144	|																				File and Data Sectors																							|
	?	|																						.........																								|
		-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
		
		
		
		4096 byte layout





*/

class DollarFs
{
private:
	char const * DollarOsSig = "DollarFs";
	void createFileSystem()
	{
		//Initialize the header signature
		memset((void*)&header, 0, sizeof(FileSystemHeader));
		memcpy((void*)&header.signature, (void const *)DollarOsSig, 8);
		LARGE_INTEGER fileSize;

		//Calculate how many descriptors are needed
		if (!GetFileSizeEx(file, &fileSize))	throw GetLastError();
		header.totalDescriptorSectors = fileSize.QuadPart / 4096;

		//Calculate the number of sectors that are in the sector descriptors
		unsigned long long const numberOfDescriptorsPerSector = 4096 * 2 / sizeof(SectorDescriptor);
		unsigned long long totalSectorsUsedForDescriptors = header.totalDescriptorSectors / numberOfDescriptorsPerSector;
		if (header.totalDescriptorSectors % numberOfDescriptorsPerSector)
			totalSectorsUsedForDescriptors++;
		if (header.totalDescriptorSectors + 145 >= header.totalDescriptorSectors)		//144 for ex-boot 1 for MBR/Header
			throw ERROR_DISK_QUOTA_EXCEEDED;
		header.totalFreeSectors = header.totalDescriptorSectors - totalSectorsUsedForDescriptors - 145;

		//Allocate space for the sector descriptors
		sectorDescriptors = (SectorDescriptor*)malloc(4096 * totalSectorsUsedForDescriptors);
		if (!sectorDescriptors)
			throw ERROR_OUTOFMEMORY;
		memset((void*)sectorDescriptors, 0, 4096 * totalSectorsUsedForDescriptors);
		

		//Initialize the first MBR/Header sector, ex-boot sectors and sector descriptors
		for (unsigned long long i = 0; i < (145  + totalSectorsUsedForDescriptors) / 2; i++)
		{
			sectorDescriptors[i].sectorAInUse = 1;
			sectorDescriptors[i].isAFull = 1;
			sectorDescriptors[i].sectorBInUse = 1;
			sectorDescriptors[i].isBFull = 1;
		}
		if ((145 + totalSectorsUsedForDescriptors) % 2)
		{
			sectorDescriptors[(145 + totalSectorsUsedForDescriptors) / 2].isAFull = 1;
			sectorDescriptors[(145 + totalSectorsUsedForDescriptors) / 2].sectorAIsData = 1;
		}

	
	}
	void loadDescriptors()
	{
		LARGE_INTEGER pos;
		pos.QuadPart = (145 * 4096);
		if (!SetFilePointerEx(file, pos, &pos, FILE_BEGIN))	
			throw GetLastError();
		unsigned long long const numberOfDescriptorsPerSector = 4096 * 2 / sizeof(SectorDescriptor);
		unsigned long long totalSectorsUsedForDescriptors = header.totalDescriptorSectors / numberOfDescriptorsPerSector;
		if (header.totalDescriptorSectors % numberOfDescriptorsPerSector)
			totalSectorsUsedForDescriptors++;
		this->sectorDescriptors = (SectorDescriptor *)malloc(totalSectorsUsedForDescriptors * 4096);
		if (!this->sectorDescriptors) throw ERROR_OUTOFMEMORY;
		DWORD ttl;
		if (!ReadFile(file, (void*)this->sectorDescriptors, (unsigned int)(totalSectorsUsedForDescriptors * 4096), &ttl, nullptr))	throw GetLastError();
		if (ttl != totalSectorsUsedForDescriptors * 4096)
			throw ERROR_END_OF_MEDIA;
		//Check the crc
		//unsigned int crc = 0;
		//unsigned int * crcPtr = (unsigned int *)sectorDescriptors;
		//for (int i = 0; i < header.totalDescriptorSectors; i++)
			//crc += crcPtr[i];
		//if (crc != header.sectorDescriptorsCRC)
			//throw ERROR_CORRUPT_SYSTEM_FILE;
	}
	void loadRootDescriptors()
	{

	}
	void writeDescriptors()
	{
		LARGE_INTEGER pos;
		pos.QuadPart = 145 * 4096;
		if (!SetFilePointerEx(file, pos, &pos, FILE_BEGIN))	throw GetLastError();
		unsigned int crc = 0;
		unsigned int * crcPtr = (unsigned int *)sectorDescriptors;
		for (int i = 0; i < header.totalDescriptorSectors / 8; i++)
			crc += crcPtr[i];
		header.sectorDescriptorsCRC = crc;
		DWORD ttl;
		unsigned long long const numberOfDescriptorsPerSector = 4096 * 2 / sizeof(SectorDescriptor);
		unsigned long long totalSectorsUsedForDescriptors = header.totalDescriptorSectors / numberOfDescriptorsPerSector;
		if (header.totalDescriptorSectors % numberOfDescriptorsPerSector)
			totalSectorsUsedForDescriptors++;
		if (!WriteFile(file, (void*)sectorDescriptors,(unsigned int)( totalSectorsUsedForDescriptors * 4096), &ttl, nullptr))	throw GetLastError();
		if (ttl != totalSectorsUsedForDescriptors * 4096) throw ERROR_END_OF_MEDIA;
	}
	void writeHeader()
	{
		LARGE_INTEGER pos;
		pos.QuadPart = 512;
		if (!SetFilePointerEx(file, pos, &pos, FILE_BEGIN))	throw GetLastError();
		header.headerCRC = 0;
		unsigned int crc = 0;
		unsigned int * crcPtr = (unsigned int *)&header;
		for (int i = 0; i < 128; i++)
			crc += crcPtr[i];
		header.headerCRC = crc;
		DWORD ttl;
		if (!WriteFile(file, (void*)&this->header, sizeof(FileSystemHeader), &ttl, nullptr)) throw GetLastError();
		if (ttl != sizeof(FileSystemHeader))
			throw ERROR_END_OF_MEDIA;
	}
	void writeRootDescriptors()
	{

	}
protected:
	struct FileSystemHeader
	{
		unsigned long long signature;					//Should be 'DollarFs'
		unsigned int headerCRC;

		//First boot loader info
		unsigned int exbootSector;						//To 512 based sector that contains the second boot loader
		unsigned int exbootTotalSectors;				//The total number of 512 based sectors of the second boot loader
		unsigned int exbootSectorCRC;					//The crc of the second boot loader

		unsigned long long totalDescriptorSectors;      //The total number of descriptors present
		unsigned long long numberOfFolders;				
		unsigned long long numberOfFiles;
		unsigned long long totalFreeSectors;
		unsigned long long reserved[56];
		unsigned int sectorDescriptorsCRC;
		unsigned int padding;
		
	
	};
	
	struct SectorDescriptor
	{
		unsigned char sectorAInUse : 1;
		unsigned char sectorAIsData : 1;	//If (1) then the sector contains file data. If (0) then the sector contains file descriptor info
		unsigned char isAFull : 1;			//Used when sectorAIsData is (0). Tells if there are no more 512 byte sectors
		unsigned char writingA : 1;			//Marked before the sector is written to. Done to ensure writes occur properly. Set to 0 when done writing
		unsigned char sectorBInUse : 1;
		unsigned char sectorBIsData : 1;
		unsigned char isBFull : 1;
		unsigned char writingB : 1;
	};
	enum class FileType : unsigned short
	{
		Folder,
		File
	};
	struct Descriptor	//304 bytes
	{
		unsigned short type;
		unsigned short nameSize;
		unsigned int flags;
		unsigned int creationDate;
		unsigned int modifiedDate;
		unsigned int accessDate;
		unsigned int headerCRC;
		unsigned long long parentsSector;
		unsigned long long currentSector;
		unsigned long long fromContinuationSector;
		char name[256];								//Null terminated
		inline bool checkCrc()
		{
			unsigned int oldCrc = this->headerCRC;
			this->headerCRC = 0;
			unsigned long long crc = 0;
			unsigned int * ptr = (unsigned int *)this;
			for (unsigned int i = 0; i < 128; i++, ptr ++)
				crc += *ptr;
			this->headerCRC = oldCrc;
			if (oldCrc != crc)	return false;
			return true;
		}
		inline void calculateCrc()
		{
			this->headerCRC = 0;
			unsigned int * ptr = (unsigned int *)this;
			for (unsigned int i = 0; i < 128; i++, ptr++)
				this->headerCRC += *ptr;
		}
	};
	struct FileDescriptor : Descriptor
	{
		unsigned long long dataSize;				//Number of 4096 byte sectors
		unsigned long long sector;
		unsigned long long dataSectorsContinuation;
		unsigned int dataCRC;
		unsigned int unfilledOfLastSector;			//Number of bytes not filled in the last sector
		unsigned int reserved[14];
		inline unsigned long long getFileSize()
		{
			return dataSize * 4096 - unfilledOfLastSector;
		}
		
	};
	struct FolderDescriptor : Descriptor
	{
		unsigned int reserved[4];
		unsigned long long totalChildren : 29;
		unsigned long long childrenContinuationOffset : 3;	//512 byte offset in the sector that has the rest of the child sectors
		unsigned long long childrenSectors[22];					
		unsigned long long childrenContinuationSector;		//Sector that has the rest of the child sectors
		void initialize(char const * folderName)
		{
			memset((void*)this, 0, sizeof(Descriptor));
			this->type = (unsigned short)FileType::Folder;
			if (folderName)
			{
				this->nameSize = (unsigned short)strlen(folderName);
				if (this->nameSize)	memcpy((void*)name, (void const *)folderName, this->nameSize + 1);
			}
			time_t sysTime;
			sysTime = time(nullptr);
			tm baseTime;
			memset((void*)&baseTime, 0, sizeof(tm));
			baseTime.tm_year = 117;
			baseTime.tm_isdst = true;
			this->creationDate = (unsigned int)difftime(sysTime, mktime(&baseTime));
		}
	};
	struct ChildContinuationDescriptor
	{
		unsigned short type;
		unsigned short padding;
		unsigned int flags;
		unsigned int crc;
		unsigned int reserved;
		unsigned long long continuationFromSector;			//Points to the root sector that this is continued from
		unsigned long long nextContinuationSector;
		unsigned long long children[60];
	};

	static const int FLAGS_FREE = 0;
	static const int FLAGS_FOLDER = 1;
	static const int FLAGS_FILE = 2;
	HANDLE file;
	FileSystemHeader header;
	FolderDescriptor ** rootDescriptor;
	SectorDescriptor * sectorDescriptors;
	unsigned long long totalRootDescriptors;

	inline char const * getFileName(char const * path)
	{
		size_t len = strlen(path);
		size_t i = len - 1;
		for (;path[i] != '\\' && i > 0; i--);
		if (i == len - 1)	return nullptr;
		return (char const *)((uintptr_t)path + (len - i));
	}
	inline char const * getNextPath(char const * path, size_t & len)
	{
		for (len = 1; path[len] != 0 && path[len] != '\\'; len++);
		if (len == 1)	return nullptr;
		return path;
	}
	ChildContinuationDescriptor * loadChildDescriptor(Descriptor * parentOrContinuationSector)
	{
		// TODO: finish
		return nullptr;
	}
	Descriptor * findDescriptor(Descriptor * parent, char const * name, size_t nameLen)
	{
		//Make sure the name length is valid and not too long
		if (nameLen > 255)	throw ERROR_BAD_LENGTH;
		
		//Make sure the parent descriptor is a folder
		if ((parent->flags & FLAGS_FOLDER) != FLAGS_FOLDER)	return nullptr;
		
		//Allocate a buffer to read the child descriptors of the parent
		void * buffer = malloc(512);
		if (!buffer) throw ERROR_OUTOFMEMORY;
		FolderDescriptor * folder = (FolderDescriptor *)parent;
		LARGE_INTEGER fileOffset;
		DWORD ttl;
		Descriptor * desc = (Descriptor *)buffer;

		int totalChildrenSearched = 0;
		//Search each of the children in reverse order until all the children are searched, or the child is found
		for (int i = 21; i >= 0 && totalChildrenSearched < folder->totalChildren; i--)
		{
			if (!folder->childrenSectors[i])	continue;
			totalChildrenSearched++;
			//Navigate to the correct sector containing the child
			fileOffset.QuadPart = folder->childrenSectors[i] * 4096;
			if (SetFilePointerEx(file, fileOffset, &fileOffset, FILE_BEGIN) == FALSE)
			{
				DWORD err = GetLastError();
				free(buffer);
				throw err;
			}

			//Read the child sector into the allocated buffer
			if (ReadFile(file, buffer, 512, &ttl, nullptr) == FALSE)
			{
				DWORD err = GetLastError();
				free(buffer);
				throw err;
			}
			if (ttl != 512)
			{
				free(buffer);
				throw ERROR_BAD_LENGTH;
			}

			if (desc->nameSize != nameLen)	continue;
			if (!memcmp((void const *)desc->name, (void const *)name, nameLen))	return desc;
		}
		int totalChildren = folder->totalChildren;
		while(totalChildrenSearched < totalChildren)
		{
			ChildContinuationDescriptor * childs = loadChildDescriptor(parent);
			if (!childs)
			{
				free(buffer);
				return nullptr;
			}
			for (int i = 59; i >= 0; i--)
			{
				if (!childs->children[i])	continue;
				totalChildrenSearched++;
				fileOffset.QuadPart = childs->children[i];
				parent = (Descriptor *)childs;
				if (SetFilePointerEx(file, fileOffset, &fileOffset, FILE_BEGIN) == FALSE)
				{
					DWORD err = GetLastError();
					free(buffer);
					throw err;
				}

				//Read the child sector into the allocated buffer
				if (ReadFile(file, buffer, 512, &ttl, nullptr) == FALSE)
				{
					DWORD err = GetLastError();
					free(buffer);
					throw err;
				}
				if (ttl != 512)
				{
					free(buffer);
					throw ERROR_BAD_LENGTH;
				}
				if (desc->nameSize != nameLen)	continue;
				if (!memcmp((void const *)desc->name, (void const *)name, nameLen))	return desc;
			}
			
		}
		free(buffer);
		return nullptr;
	}
	
	void removeChildDescriptor(Descriptor * parent, Descriptor * child)
	{
		if (!parent && !child) throw ERROR_BAD_ARGUMENTS;
		if (!parent)	parent = this->rootDescriptor[0];
		//Remove from the children
		//Decrement the children count of the parent
		//Remove the sector descriptor
	}
	
	
public:
	struct FileAndDirectory
	{
		char * directory;
		char * fileName;
		FileAndDirectory()
		{
			directory = nullptr;
			fileName = nullptr;
		}
		FileAndDirectory(char * dir, char * file)
		{
			directory = dir;
			fileName = file;
		}
		void operator () (char * dir, char * file)
		{
			this->directory = dir;
			this->fileName = file;
		}
	};
	DollarFs(char const * fileName, bool createMBRifMissing = false)
	{
		rootDescriptor = nullptr;
		sectorDescriptors = nullptr;
		DWORD ttl;
		LARGE_INTEGER headerPosition;
		headerPosition.QuadPart = 512;
		file = CreateFileA(fileName, GENERIC_ALL, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (file == INVALID_HANDLE_VALUE)													throw GetLastError();
		try
		{
			if (!SetFilePointerEx(file, headerPosition, &headerPosition, FILE_BEGIN))	throw GetLastError();
			if (ReadFile(file, (void*)&header, 512, &ttl, nullptr) == FALSE)
				throw GetLastError();
			if (ttl != 512)
				throw ERROR_END_OF_MEDIA;
			if (memcmp((void const *)&header.signature, (void const *)DollarOsSig, 8))
			{
				if (!createMBRifMissing)	throw ERROR_BAD_FORMAT;
				createFileSystem();
			}
			else
			{
				loadDescriptors();
				loadRootDescriptors();
			}
		}
		catch (...)
		{
			CloseHandle(file);
			if (rootDescriptor)
			{
				for (unsigned long long i = 0; i < totalRootDescriptors; i++)
					if(rootDescriptor[i])	free((void*)rootDescriptor[i]);
				free((void*)rootDescriptor);
			}
			if (sectorDescriptors)	free((void*)sectorDescriptors);
			throw;
		}
		
	}
	virtual ~DollarFs()
	{

	}
	void createMBR(char * mbrFileName)
	{
		if (!mbrFileName)	return;
		LARGE_INTEGER seekPosition;
		seekPosition.QuadPart = 0;
		unsigned short * buffer = (unsigned short *)malloc(512);
		if (!buffer)	throw ERROR_OUTOFMEMORY;
		buffer[255] = 0xAA55;
		HANDLE mbrFile = CreateFileA(mbrFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (mbrFile == INVALID_HANDLE_VALUE)
		{
			DWORD err = GetLastError();
			free(buffer);
			throw err;
		}
		__try
		{
			LARGE_INTEGER fileSize;
			if (GetFileSizeEx(mbrFile, &fileSize) == FALSE || fileSize.QuadPart > 510)									throw ERROR_BAD_LENGTH;
			if (SetFilePointerEx(file, seekPosition, &seekPosition, FILE_BEGIN) == FALSE)								throw GetLastError();
			DWORD ttl;
			if (ReadFile(mbrFile, (void*)buffer, fileSize.LowPart, &ttl, nullptr) == FALSE || ttl != fileSize.LowPart)	throw GetLastError();
			if (WriteFile(file, (void*)buffer, 512, &ttl, nullptr) == FALSE)											throw GetLastError();
			if (ttl != 512)																								throw ERROR_BAD_LENGTH;
		}
		__finally
		{
			CloseHandle(mbrFile);
			free((void*)buffer);
		}
	}
	void writeExBoot(char * exBootFileName)
	{
		LARGE_INTEGER fileSize;
		void * buffer = nullptr;
		//Open the exBoot File
		HANDLE exBootHdl = CreateFileA(exBootFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (exBootHdl == INVALID_HANDLE_VALUE)								throw GetLastError();
		__try
		{
			
			//Make sure the exBoot File is a valid size
			if (!GetFileSizeEx(exBootHdl, &fileSize))						throw GetLastError();
			if (fileSize.QuadPart <= 0 || fileSize.QuadPart > 589824)		throw ERROR_BAD_LENGTH;
			
			//Set the exboot sector and exboot total sectors to their appropriate values
			header.exbootSector = 8;							//Starts after the first 8 512 byte sectors
			header.exbootTotalSectors = fileSize.LowPart / 512;
			if (fileSize.LowPart % 512)
				header.exbootTotalSectors++;
			LARGE_INTEGER pos;
			pos.QuadPart = header.exbootSector * 512;
			if (!SetFilePointerEx(file, pos, nullptr, FILE_BEGIN))	throw GetLastError();
			
			//Allocate a buffer for the file
			buffer = (void *)malloc(header.exbootTotalSectors * 512);
			if (!buffer)	throw ERROR_OUTOFMEMORY;
			//Read the exBoot File and write to the specified sector
			DWORD ttl;
			if (ReadFile(exBootHdl, (void*)buffer, fileSize.LowPart, &ttl, nullptr) == FALSE || ttl != fileSize.LowPart)	
				throw GetLastError();
			if (WriteFile(file, (void*)buffer,header.exbootTotalSectors * 512, &ttl, nullptr) == FALSE || ttl != header.exbootTotalSectors * 512)
				throw GetLastError();
			unsigned int numberOfInts = header.exbootTotalSectors * 512 / 4;
			unsigned int * crcPtr = (unsigned int *)buffer;
			unsigned int crc = 0;
			for (unsigned int i = 0; i < numberOfInts; i++)
				crc += crcPtr[i];
			header.exbootSectorCRC = crc;
		}
		__finally
		{
			CloseHandle(exBootHdl);
			if(buffer)		free(buffer);
		}
	}
	bool removeFile(FileAndDirectory * info)
	{
		size_t pathLen = 0;
		Descriptor * parent = nullptr;
		Descriptor * child = nullptr;
		__try
		{
			//Navigate to the parent descriptor
			for (char const * dir = info->directory; dir; dir = getNextPath((char const *)((size_t)dir + pathLen), pathLen))
			{
				Descriptor * newChild = findDescriptor(child, dir, pathLen - 1);
				if (!newChild)	return false;
				if (parent)		free((void*)parent);
				parent = child;
				child = newChild;
			}
			removeChildDescriptor(parent, child);
			return true;
		}
		__finally
		{
			if (parent)	free((void*)parent);
			if (child)	free((void*)child);
		}
	}
	

	bool addFile(FileAndDirectory * info)
	{
		return true;
	}
	void flush()
	{
		this->writeRootDescriptors();
		this->writeDescriptors();
		this->writeHeader();
	}
};

