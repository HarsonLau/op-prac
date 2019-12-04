// synchdisk.cc 
//	Routines to synchronously access the disk.  The physical disk 
//	is an asynchronous device (disk requests return immediately, and
//	an interrupt happens later on).  This is a layer on top of
//	the disk providing a synchronous interface (requests wait until
//	the request completes).
//
//	Use a semaphore to synchronize the interrupt handlers with the
//	pending requests.  And, because the physical disk can only
//	handle one operation at a time, use a lock to enforce mutual
//	exclusion.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchdisk.h"
#include "system.h"
#define CacheEnabled
//----------------------------------------------------------------------
// DiskRequestDone
// 	Disk interrupt handler.  Need this to be a C routine, because 
//	C++ can't handle pointers to member functions.
//----------------------------------------------------------------------
static void
DiskRequestDone (int arg)
{
    SynchDisk* disk = (SynchDisk *)arg;

    disk->RequestDone();
}

//----------------------------------------------------------------------
// SynchDisk::SynchDisk
// 	Initialize the synchronous interface to the physical disk, in turn
//	initializing the physical disk.
//
//	"name" -- UNIX file name to be used as storage for the disk data
//	   (usually, "DISK")
//----------------------------------------------------------------------
SynchDisk::SynchDisk(char* name)
{
    semaphore = new Semaphore("synch disk", 0);
    lock = new Lock("synch disk lock");
    disk = new Disk(name, DiskRequestDone, (int) this);
    readerCnt=new int[NumSectors];
    openerCnt=new int[NumSectors];
    memset(readerCnt,0,sizeof(int)*NumSectors);
    memset(openerCnt,0,sizeof(int)*NumSectors);
   for(int i=0;i<NumSectors;i++){
	rCntMutex[i]=new Semaphore("reader cnt mutex",1);
	oCntMutex[i]=new Semaphore("opener cnt mutex",1);
	RW[i]=new Semaphore("reader writer mutex",1);
   } 
   for(int i=0;i<CacheSize;i++){
	   CacheTable[i]=new CacheEntry();
	   CacheTable[i]->dirty=CacheTable[i]->valid=false;
	   CacheTable[i]->lru=stats->totalTicks;
   }
}

//----------------------------------------------------------------------
// SynchDisk::~SynchDisk
// 	De-allocate data structures needed for the synchronous disk
//	abstraction.
//----------------------------------------------------------------------
SynchDisk::~SynchDisk()
{
    delete disk;
    delete lock;
    delete semaphore;
    delete readerCnt;
    delete openerCnt;
    for(int i=0;i<NumSectors;i++){
	    delete rCntMutex[i];
	    delete oCntMutex[i];
	    delete RW[i];
    }
    for(int i=0;i<CacheSize;i++){
	    delete CacheTable[i];
    }
}

//----------------------------------------------------------------------
// SynchDisk::ReadSector
// 	Read the contents of a disk sector into a buffer.  Return only
//	after the data has been read.
//
//	"sectorNumber" -- the disk sector to read
//	"data" -- the buffer to hold the contents of the disk sector
//----------------------------------------------------------------------
void
SynchDisk::ReadSector(int sectorNumber, char* data)
{
#ifdef CacheEnabled
	bool hit=false;
	for(int i=0;i<CacheSize;i++){
		lock->Acquire();			// only one disk I/O at a time
		if(CacheTable[i]->valid&&CacheTable[i]->sector==sectorNumber){
			DEBUG('f',"Cache hit for sector %2d\n",sectorNumber);
			memcpy(data,&(CacheTable[i]->data[0]),SectorSize);
			hit=true;
			CacheTable[i]->lru=stats->totalTicks;
		}
		lock->Release();
	}
	if(!hit){
		CacheMiss(sectorNumber);
		for(int i=0;i<CacheSize;i++){
			lock->Acquire();			// only one disk I/O at a time
			if(CacheTable[i]->valid&&CacheTable[i]->sector==sectorNumber){
				DEBUG('f',"Cache hit for sector %2d\n",sectorNumber);
				memcpy(data,&(CacheTable[i]->data[0]),SectorSize);
				hit=true;
				CacheTable[i]->lru=stats->totalTicks;
			}
			lock->Release();
		}
	}
#else
    lock->Acquire();			// only one disk I/O at a time
    disk->ReadRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    lock->Release();
#endif
}

//----------------------------------------------------------------------
// SynchDisk::WriteSector
// 	Write the contents of a buffer into a disk sector.  Return only
//	after the data has been written.
//
//	"sectorNumber" -- the disk sector to be written
//	"data" -- the new contents of the disk sector
//----------------------------------------------------------------------
void
SynchDisk::WriteSector(int sectorNumber, char* data)
{
#ifdef CacheEnabled
	bool hit=false;
	for(int i=0;i<CacheSize;i++){
		lock->Acquire();			// only one disk I/O at a time
		if(CacheTable[i]->valid&&CacheTable[i]->sector==sectorNumber){
			DEBUG('f',"Cache hit for sector %2d\n",sectorNumber);
			memcpy(&(CacheTable[i]->data[0]),data,SectorSize);
			hit=true;
			CacheTable[i]->lru=stats->totalTicks;
			CacheTable[i]->dirty=true;
		}
		lock->Release();
	}
	if(!hit){
		CacheMiss(sectorNumber);
		for(int i=0;i<CacheSize;i++){
			lock->Acquire();			// only one disk I/O at a time
			if(CacheTable[i]->valid&&CacheTable[i]->sector==sectorNumber){
				DEBUG('f',"Cache hit for sector %2d\n",sectorNumber);
				memcpy(&(CacheTable[i]->data[0]),data,SectorSize);
				hit=true;
				CacheTable[i]->lru=stats->totalTicks;
				CacheTable[i]->dirty=true;
			}
			lock->Release();
		}
	}

#else
    lock->Acquire();			// only one disk I/O at a time
    disk->WriteRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    lock->Release();
#endif
}

//----------------------------------------------------------------------
// SynchDisk::RequestDone
// 	Disk interrupt handler.  Wake up any thread waiting for the disk
//	request to finish.
//----------------------------------------------------------------------
void
SynchDisk::RequestDone()
{ 
    semaphore->V();
}

void SynchDisk::StartRead(int hdrSector){
	DEBUG('F',"waiting to read hdrsector=%2d\n",hdrSector);
	rCntMutex[hdrSector]->P();
	if(readerCnt[hdrSector]==0)
		RW[hdrSector]->P();
	readerCnt[hdrSector]++;
	rCntMutex[hdrSector]->V();
	DEBUG('F',"permitted to read hdrsector=%2d\n",hdrSector);
}
void SynchDisk::EndRead(int hdrSector){
	rCntMutex[hdrSector]->P();
	readerCnt[hdrSector]--;
	if(readerCnt[hdrSector]==0)
		RW[hdrSector]->V();
	rCntMutex[hdrSector]->V();
	DEBUG('F'," read hdrsector=%2d finished\n",hdrSector);

}
void SynchDisk::StartWrite(int hdrSector){
	DEBUG('F',"waiting to write hdrsector=%2d\n",hdrSector);
	RW[hdrSector]->P();
	DEBUG('F',"premited to write hdrsector=%2d\n",hdrSector);
}
void SynchDisk::EndWrite(int hdrSector){
	RW[hdrSector]->V();
	DEBUG('F'," write hdrsector=%2d finished\n",hdrSector);
}
void SynchDisk::Open(int hdrSector){
	oCntMutex[hdrSector]->P();
	openerCnt[hdrSector]++;
	oCntMutex[hdrSector]->V();
}
void SynchDisk::Close(int hdrSector){
	oCntMutex[hdrSector]->P();
	openerCnt[hdrSector]--;
	oCntMutex[hdrSector]->V();
}
int SynchDisk::GetOpenStart(int hdrSector){
	DEBUG('F',"accessing the openercnt hdrsector:%2d \n",hdrSector);
	//oCntMutex[hdrSector]->P();
	DEBUG('f',"Open cnt for %2d is %2d\n",hdrSector,openerCnt[hdrSector]);
	 return openerCnt[hdrSector];
}
int SynchDisk::GetOpenDone(int hdrSector){
	DEBUG('f',"in get open done\n");
	//oCntMutex[hdrSector]->V();
	DEBUG('F',"finished accessing the openercnt hdrsector:%2d \n",hdrSector);
}
void SynchDisk::CacheMiss(int sector){
	int evict=0;
	for(int i=0;i<CacheSize;i++){
		if(!CacheTable[i]->valid){
			evict=i;
			break;
		}
		if(CacheTable[i]->lru<CacheTable[evict]->lru)
			evict=i;
	}
	if(CacheTable[evict]->valid&&CacheTable[evict]->dirty){
		lock->Acquire();			// only one disk I/O at a time
		disk->WriteRequest(CacheTable[evict]->sector, &(CacheTable[evict]->data[0]));
		semaphore->P();			// wait for interrupt
		CacheTable[evict]->dirty=false;
		CacheTable[evict]->valid=false;
		lock->Release();
	}
	//Swap in
	lock->Acquire();			// only one disk I/O at a time
	disk->ReadRequest(sector, &(CacheTable[evict]->data[0]));
	semaphore->P();			// wait for interrupt
	CacheTable[evict]->dirty=false;
	CacheTable[evict]->valid=true;
	CacheTable[evict]->lru=stats->totalTicks;
	CacheTable[evict]->sector=sector;
	lock->Release();
}