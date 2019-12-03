// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"
#include "time.h"
//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------
bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    DEBUG('f',"file size :%d , need %d sectors\n",fileSize,numSectors);
    if (freeMap->NumClear() < numSectors){
	    DEBUG('f',"Disk space not enough\n");
	return FALSE;		// not enough space
    }

	if(fileSize > MaxFileSize){
		DEBUG('f',"file size > MaxFileSize\n");
		return false;
	}

	// if we do not need to use a second index
	if(numSectors<=NumDirect){
		DEBUG('f',"do not need second index\n");
		for (int i = 0; i < numSectors; i++){
			dataSectors[i] = freeMap->Find();
			DEBUG('f',"dataSector[%d]=%d\n",i,dataSectors[i]);
		}
	}
	else{
		// first use direct index
		for (int i = 0; i < NumDirect; i++){
			dataSectors[i] = freeMap->Find();
			DEBUG('f',"dataSector[%d]=%d\n",i,dataSectors[i]);

		}
		// calculate the sectors left
		int sectorsLeft=numSectors-NumDirect;
		DEBUG('f',"need to put %d sectors in second index \n",sectorsLeft);
		for(int i=0;sectorsLeft>0;i++){
			// find a sector to accommodate the sector numbers
			dataSectors[NumDirect+i]=freeMap->Find();
			int num1=0;
			if(sectorsLeft<SecondDirect)
				num1=sectorsLeft;
			else 
				num1=SecondDirect;
			int *sectors=new int[num1];
			for(int j=0;j<num1;j++){
				sectors[j]=freeMap->Find();
				DEBUG('f',"second index %d ,%d th secotr ,location %d\n",i,j,sectors[j]);
			}
			synchDisk->WriteSector(dataSectors[NumDirect+i],(char*)sectors);
			delete sectors;
			sectorsLeft-=num1;
		}
	}
	return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------
void 
FileHeader::Deallocate(BitMap *freeMap)
{
	if(numSectors<=NumDirect){
		DEBUG('f',"Do not need to deallocate second indexes\n");
		for (int i = 0; i < numSectors; i++) {
			ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
			freeMap->Clear((int) dataSectors[i]);
		}
	}
	else{
		//deallocate direct 
		for (int i=0;i<NumDirect;i++){
			ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
			freeMap->Clear((int) dataSectors[i]);
		}
		DEBUG('f',"Deallocating Second level indexes\n");
		//deallocate indirect 
		int sectorsLeft=numSectors-NumDirect;
		for(int i=0;sectorsLeft>0;i++){
			int num1=0;
			if(sectorsLeft<SecondDirect)
				num1=sectorsLeft;
			else 
				num1=SecondDirect;
			DEBUG('f',"need to deallocate %d sectors\n",num1);
			int * sectors=new int[SecondDirect];
			DEBUG('f',"array sectors created with length %d\n",num1);
			synchDisk->ReadSector(dataSectors[NumDirect+i],(char *)sectors);
			for (int j=0;j<num1;j++){
				ASSERT(freeMap->Test((int )sectors[j]));
				freeMap->Clear((int)sectors[j]);
			}
			DEBUG('f',"deallocated %d sectors\n",num1);
			delete sectors;
			sectorsLeft-=num1;
			ASSERT(freeMap->Test(dataSectors[NumDirect+i]));
			freeMap->Clear(dataSectors[NumDirect+i]);	
		}
	}
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------
void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------
int
FileHeader::ByteToSector(int offset)
{
	int sectorOffset = offset / SectorSize;
	if(sectorOffset < NumDirect)
	    return(dataSectors[offset / SectorSize]);
	else
	{
		int sectorsLeft=sectorOffset-NumDirect;
		int secondIndex=sectorsLeft/SecondDirect;
		int *sectors=new int[SecondDirect];
		synchDisk->ReadSector(dataSectors[NumDirect+secondIndex],(char*)sectors);
		int indexOffset=sectorOffset-NumDirect-secondIndex*SecondDirect;
		int res=sectors[indexOffset];
		delete sectors;
		return res;
	}
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++)
	printf("%d ", dataSectors[i]);
	printf("\n");
/*
    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
  */
    printf("create time:%s\n",create_time);
    printf("last visit time :%s\n",visit_time);
    printf("last modify time :%s\n",modify_time);
    delete [] data;
}

void FileHeader::set_create_time(){
	time_t timep;
	time(&timep);
	strncpy(create_time,asctime(gmtime(&timep)),25);
	create_time[24]='\0';
}
void FileHeader::set_visit_time(){
	time_t timep;
	time(&timep);
	strncpy(visit_time,asctime(gmtime(&timep)),25);
	visit_time[24]='\0';
}
void FileHeader::set_modify_time(){
	time_t timep;
	time(&timep);
	strncpy(modify_time,asctime(gmtime(&timep)),25);
	modify_time[24]='\0';
}
//----------------------------------------------------------------------
// FileHeader::extendLength
//	Extend the file length by sectorNum sectors
//	This function will change the fileheader itself and the freemap
//	It's the caller's duty to write the changes back to the disk
//----------------------------------------------------------------------
bool FileHeader::extendLength(int newNumBytes,int sectorNum,BitMap* freeMap){
	/*如果一级索引还有剩余的话，一定要利用起来*/
	if(numSectors<NumDirect&&sectorNum>0){
		for(int i=numSectors;i<NumDirect&&sectorNum>0;i++){
			dataSectors[i]=freeMap->Find();
			numSectors++;
			sectorNum--;
			DEBUG('f',"Using %dth direct index->%2d\n",i,dataSectors[i]);
		}
	}
	for(int i=1;i<=NumSecondIndex;i++){
		if(sectorNum<=0)
			break;
		/*如果需要新开一个二级索引的话，就新开一个吧*/
		if(numSectors==(NumDirect+(i-1)*SecondDirect)){
			dataSectors[NumDirect+i-1]=freeMap->Find();
			DEBUG('f',"Using %dth second index->%2d\n",i-1,dataSectors[NumDirect+i-1]);		
			int * sectors=new int[SecondDirect];
			for(int j=0;j<SecondDirect&&sectorNum>0;j++){
				sectors[j]=freeMap->Find();
				numSectors++;
				sectorNum--;
				DEBUG('f',"sectors[%2d]=%2d\n",j,sectors[j]);
			}
			synchDisk->WriteSector(dataSectors[NumDirect+i-1],(char*)sectors);
			delete sectors;
		}

		if(sectorNum<=0)
			break;
		/*逻辑比较复杂，我英文差，说不清楚^~^
		* 如果当前文件控制块中的信息显示已分配给本文件的扇区数的确使用了二级索引，
		* 但是二级索引那块还有剩余空间
		* 这一块用了多少个条目呢，除去直接索引的那部分，模上SecondDirect即32
		* 然后就从这个地方开始，分配给
		*/
		if(numSectors>(NumDirect+(i-1)*SecondDirect)&&numSectors<(NumDirect+i*SecondDirect)&&sectorNum>0){
			int * sectors=new int[SecondDirect];
			synchDisk->ReadSector(dataSectors[NumDirect+i-1],(char*)sectors);
			int index=(numSectors-NumDirect)%SecondDirect;
			DEBUG('f',"%dth second index ->%2d is not full\n",i-1,dataSectors[i-1]);
			for(int j=index;j<SecondDirect&&sectorNum>0;j++){
				sectors[j]=freeMap->Find();
				numSectors++;
				sectorNum--;
				DEBUG('f',"sectors[%2d]=%2d\n",j,sectors[j]);
			}
			synchDisk->WriteSector(dataSectors[NumDirect+i-1],(char*)sectors);
			delete sectors;
		}

		if(sectorNum<=0)
			break;
	}
	if(sectorNum<=0){
		numBytes=newNumBytes;
		return true;
	}
	else 
		return false;
}