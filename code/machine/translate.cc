// translate.cc 
//	Routines to translate virtual addresses to physical addresses.
//	Software sets up a table of legal translations.  We look up
//	in the table on every memory reference to find the true physical
//	memory location.
//
// Two types of translation are supported here.
//
//	Linear page table -- the virtual page # is used as an index
//	into the table, to find the physical page #.
//
//	Translation lookaside buffer -- associative lookup in the table
//	to find an entry with the same virtual page #.  If found,
//	this entry is used for the translation.
//	If not, it traps to software with an exception. 
//
//	In practice, the TLB is much smaller than the amount of physical
//	memory (16 entries is common on a machine that has 1000's of
//	pages).  Thus, there must also be a backup translation scheme
//	(such as page tables), but the hardware doesn't need to know
//	anything at all about that.
//
//	Note that the contents of the TLB are specific to an address space.
//	If the address space changes, so does the contents of the TLB!
//
// DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "machine.h"
#include "addrspace.h"
#include "system.h"
#include <string.h>

// Routines for converting Words and Short Words to and from the
// simulated machine's format of little endian.  These end up
// being NOPs when the host machine is also little endian (DEC and Intel).

unsigned int
WordToHost(unsigned int word) {
#ifdef HOST_IS_BIG_ENDIAN
	 register unsigned long result;
	 result = (word >> 24) & 0x000000ff;
	 result |= (word >> 8) & 0x0000ff00;
	 result |= (word << 8) & 0x00ff0000;
	 result |= (word << 24) & 0xff000000;
	 return result;
#else 
	 return word;
#endif /* HOST_IS_BIG_ENDIAN */
}

unsigned short
ShortToHost(unsigned short shortword) {
#ifdef HOST_IS_BIG_ENDIAN
	 register unsigned short result;
	 result = (shortword << 8) & 0xff00;
	 result |= (shortword >> 8) & 0x00ff;
	 return result;
#else 
	 return shortword;
#endif /* HOST_IS_BIG_ENDIAN */
}

unsigned int
WordToMachine(unsigned int word) { return WordToHost(word); }

unsigned short
ShortToMachine(unsigned short shortword) { return ShortToHost(shortword); }


//----------------------------------------------------------------------
// Machine::ReadMem
//      Read "size" (1, 2, or 4) bytes of virtual memory at "addr" into 
//	the location pointed to by "value".
//
//   	Returns FALSE if the translation step from virtual to physical memory
//   	failed.
//
//	"addr" -- the virtual address to read from
//	"size" -- the number of bytes to read (1, 2, or 4)
//	"value" -- the place to write the result
//----------------------------------------------------------------------

bool
Machine::ReadMem(int addr, int size, int *value)
{
    int data;
    ExceptionType exception;
    int physicalAddress;
    
    DEBUG('a', "Reading VA 0x%x, size %d\n", addr, size);
    
    exception = Translate(addr, &physicalAddress, size, FALSE);
    if (exception != NoException) {
	machine->RaiseException(exception, addr);
        if(exception!=PageFaultException)
                return false;
        exception = Translate(addr, &physicalAddress, size, FALSE);
        if (exception != NoException) {
                machine->RaiseException(exception, addr);
        }
        exception = Translate(addr, &physicalAddress, size, FALSE);
        if (exception != NoException) {
                machine->RaiseException(exception, addr);
				return false;
        }
    }
    switch (size) {
      case 1:
	data = machine->mainMemory[physicalAddress];
	*value = data;
	break;
	
      case 2:
	data = *(unsigned short *) &machine->mainMemory[physicalAddress];
	*value = ShortToHost(data);
	break;
	
      case 4:
	data = *(unsigned int *) &machine->mainMemory[physicalAddress];
	*value = WordToHost(data);
	break;

      default: ASSERT(FALSE);
    }
    
    DEBUG('a', "\tvalue read = %8.8x\n", *value);
    return (TRUE);
}

//----------------------------------------------------------------------
// Machine::WriteMem
//      Write "size" (1, 2, or 4) bytes of the contents of "value" into
//	virtual memory at location "addr".
//
//   	Returns FALSE if the translation step from virtual to physical memory
//   	failed.
//
//	"addr" -- the virtual address to write to
//	"size" -- the number of bytes to be written (1, 2, or 4)
//	"value" -- the data to be written
//----------------------------------------------------------------------

bool
Machine::WriteMem(int addr, int size, int value)
{
    ExceptionType exception;
    int physicalAddress;
     
    DEBUG('a', "Writing VA 0x%x, size %d, value 0x%x\n", addr, size, value);

    exception = Translate(addr, &physicalAddress, size, true);
    if (exception != NoException) {
	machine->RaiseException(exception, addr);
        if(exception!=PageFaultException)
                return false;
        exception = Translate(addr, &physicalAddress, size, true);
        if (exception != NoException) {
                machine->RaiseException(exception, addr);
        }
        exception = Translate(addr, &physicalAddress, size, true);
        if (exception != NoException) {
                machine->RaiseException(exception, addr);
				return false;
        }
    }
    switch (size) {
      case 1:
	machine->mainMemory[physicalAddress] = (unsigned char) (value & 0xff);
	break;

      case 2:
	*(unsigned short *) &machine->mainMemory[physicalAddress]
		= ShortToMachine((unsigned short) (value & 0xffff));
	break;
      
      case 4:
	*(unsigned int *) &machine->mainMemory[physicalAddress]
		= WordToMachine((unsigned int) value);
	break;
	
      default: ASSERT(FALSE);
    }
    
    return TRUE;
}

//----------------------------------------------------------------------
// Machine::Translate
// 	Translate a virtual address into a physical address, using 
//	either a page table or a TLB.  Check for alignment and all sorts 
//	of other errors, and if everything is ok, set the use/dirty bits in 
//	the translation table entry, and store the translated physical 
//	address in "physAddr".  If there was an error, returns the type
//	of the exception.
//
//	"virtAddr" -- the virtual address to translate
//	"physAddr" -- the place to store the physical address
//	"size" -- the amount of memory being read or written
// 	"writing" -- if TRUE, check the "read-only" bit in the TLB
//----------------------------------------------------------------------
ExceptionType
Machine::Translate(int virtAddr, int* physAddr, int size, bool writing)
{
    int i;
    unsigned int vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;

    DEBUG('a', "\tTranslate 0x%x, %s: ", virtAddr, writing ? "write" : "read");

// check for alignment errors
    if (((size == 4) && (virtAddr & 0x3)) || ((size == 2) && (virtAddr & 0x1))){
	DEBUG('a', "alignment problem at %d, size %d!\n", virtAddr, size);
	return AddressErrorException;
    }
    
// calculate the virtual page number, and offset within the page,
// from the virtual address
    vpn = (unsigned) virtAddr / PageSize;
    offset = (unsigned) virtAddr % PageSize;
    

  
    for (entry = NULL, i = 0; i < TLBSize; i++)
    	if (tlb[i].valid && (tlb[i].virtualPage == vpn)) {
			entry = &tlb[i];			// FOUND!
			TLBHit++;
			break;
	    }
	if (entry == NULL) {				// not found
    	    DEBUG('a', "*** no valid TLB entry found for this virtual page!\n");
			TLBMiss++;
    	    return PageFaultException;		// really, this is a TLB fault,
						// the page may be in memory,
						// but not in the TLB
	}
    

    if (entry->readOnly && writing) {	// trying to write to a read-only page
	DEBUG('a', "%d mapped read-only at %d in TLB!\n", virtAddr, i);
	return ReadOnlyException;
    }
    pageFrame = entry->physicalPage;

    // if the pageFrame is too big, there is something really wrong! 
    // An invalid translation was loaded into the page table or TLB. 
    if (pageFrame >= NumPhysPages) { 
	DEBUG('a', "*** frame %d > %d!\n", pageFrame, NumPhysPages);
	return BusErrorException;
    }
    entry->use = TRUE;		// set the use, dirty bits
    if (writing){
        entry->dirty = true;
        pageTable[vpn].dirty=true;
		PhysicalPageTable[pageFrame].dirty=true;
    }
	entry->LastHitTime=stats->totalTicks;
	PhysicalPageTable[pageFrame].LastHitTime=stats->totalTicks;
    *physAddr = pageFrame * PageSize + offset;
    ASSERT((*physAddr >= 0) && ((*physAddr + size) <= MemorySize));
    DEBUG('a', "phys addr = 0x%x\n", *physAddr);
    return NoException;
}

int Machine::FIFO_TLB(int virtAddr){
    int vpn = (unsigned) virtAddr / PageSize;
	TranslationEntry *entry=&tlb[0];
	for(int i=0;i<TLBSize;i++){
		if (!tlb[i].valid){
			entry=&tlb[i];
			break;
		}
		else if(tlb[i].InTime<entry->InTime)
			entry=&tlb[i];
	}
	if(entry->valid)
		pageTable[entry->virtualPage]=*entry;
	if(pageTable[vpn].valid)
		*entry=pageTable[vpn];
	else{
		/*need to allocate a physical page*/
		AllocatePhysicalPage(vpn);
		*entry=pageTable[vpn];
	}
		entry->InTime=stats->totalTicks;
	return 0;
}

int Machine::LRU_TLB(int virtAddr){
        //printf("LRU TLB callled for vpn %d\n",virtAddr);
        unsigned int vpn = (unsigned) virtAddr / PageSize;
	TranslationEntry *entry=&tlb[0];
	for(int i=0;i<TLBSize;i++){
		if (!tlb[i].valid){
			entry=&tlb[i];
			break;
		}
		else if(tlb[i].LastHitTime<entry->LastHitTime)
			entry=&tlb[i];
	}
	if(entry->valid)
		pageTable[entry->virtualPage]=*entry;
	if(pageTable[vpn].valid)
		*entry=pageTable[vpn];
	else{
		/*need to allocate a physical page*/
		AllocatePhysicalPage(vpn);
		*entry=pageTable[vpn];
	}
		entry->InTime=stats->totalTicks;
	return 0;
}
int Machine::Invert_LRU_TLB(int virtAddr){
    unsigned int vpn = (unsigned) virtAddr / PageSize;
	TranslationEntry *entry=&tlb[0];
	for(int i=0;i<TLBSize;i++){
		if (!tlb[i].valid){
			entry=&tlb[i];
			break;
		}
		else if(tlb[i].LastHitTime<entry->LastHitTime)
			entry=&tlb[i];
	}
	if(entry->valid){
		pageTable[entry->physicalPage]=*entry;
	}
	for(int i=0;i<pageTableSize;i++){
		if(pageTable[i].valid&&pageTable[i].virtualPage==vpn){
			*entry=pageTable[i];
			entry->InTime=stats->totalTicks;
			return 0;
		}
	}
	InvertedAllocatePage(vpn);
	for(int i=0;i<pageTableSize;i++){
		if(pageTable[i].valid&&pageTable[i].virtualPage==vpn){
			*entry=pageTable[i];
			 entry->InTime=stats->totalTicks;
			return 0;
		}
	}
	return 0;
}
int Machine::InvertedAllocatePage(int vpn){
	//choose a physical page 
	int ppn=0;
	for(int i=0;i<NumPhysPages;i++){
		if(!PhysicalPageTable[i].valid){
			ppn=i;
			break;
		}
		if(PhysicalPageTable[i].LastHitTime<PhysicalPageTable[ppn].LastHitTime)
			ppn=i;
	}
	int OldVpn=PhysicalPageTable[ppn].VirtualPageNumber;
	if(PhysicalPageTable[ppn].valid){
		Thread *T=PhysicalPageTable[ppn].OwnerThread;
		/* write back */
		if(T&&PhysicalPageTable[ppn].dirty /* &&pageTable[PhysicalPageTable[ppn].VirtualPageNumber].dirty*/){
			//printf("   Swap out \n");
			#ifdef DiskImage
			T->space->DiskAddrSpace->WriteAt(
				&(machine->mainMemory[ppn*PageSize]),
				PageSize,
				PhysicalPageTable[ppn].VirtualPageNumber*PageSize
			);
			#else
			memcpy(&(T->space->vSpace[PhysicalPageTable[ppn].VirtualPageNumber*PageSize]),
				&(machine->mainMemory[ppn*PageSize]),
				PageSize);
			#endif
		}

		/* update tlb*/
		for(int i=0;i<TLBSize;i++){
			if(tlb[i].valid&&tlb[i].physicalPage==ppn){
				tlb[i].valid=false;
			}
		}

		/* update pagetable (hardware and pcb) */
		for(int i=0;i<pageTableSize;i++){
			if(pageTable[i].physicalPage==ppn&& pageTable[i].valid){
				pageTable[i].valid=false;
				if(T){
					T->space->pageTable[i].valid=false;
				}
			}
		}
	}

	#ifdef DiskImage
	currentThread->space->DiskAddrSpace->ReadAt(
		&(machine->mainMemory[ppn*PageSize]),
		PageSize,
		vpn*PageSize
	);
	#else
	memcpy(&(machine->mainMemory[ppn*PageSize]),
			&(currentThread->space->vSpace[vpn*PageSize]),
			PageSize
			);
	#endif
	/*update the global physical page table*/
	PhysicalPageTable[ppn].LastHitTime		=stats->totalTicks;
	PhysicalPageTable[ppn].valid			=true;
	PhysicalPageTable[ppn].dirty			=false;
	PhysicalPageTable[ppn].OwnerThread		=currentThread;
	PhysicalPageTable[ppn].VirtualPageNumber        =vpn;
	pageTable[ppn].valid		=true;
	pageTable[ppn].dirty		=false;
	pageTable[ppn].use			=false;
	pageTable[ppn].readOnly		=false;
	pageTable[ppn].physicalPage	=ppn;
	pageTable[ppn].virtualPage	=vpn;
	pageTable[ppn].InTime		=stats->totalTicks;
	pageTable[ppn].LastHitTime	=stats->totalTicks;
	return ppn;
}
int Machine::AllocatePhysicalPage(int vpn){
	//TLB_PageTable_check();

	/*choose a physical page*/
	int ppn=0;
	for(int i=0;i<NumPhysPages;i++){
		if(!PhysicalPageTable[i].valid){
			ppn=i;
			break;
		}
		if(PhysicalPageTable[i].LastHitTime<PhysicalPageTable[ppn].LastHitTime)
			ppn=i;
	}

	int OldVpn=PhysicalPageTable[ppn].VirtualPageNumber;

	/* 
	printf("ppn is %2d,OldVPn is %2d,vpn is %2d,",ppn,OldVpn,vpn);
	if(pageTable[OldVpn].valid&&pageTable[OldVpn].dirty){
		printf("do need to swap out \n");
	}
	else
	{
		printf("no need to swap out\n");
	}
	*/

	/* if the physical page has been occupied ,
	*	first , if it is dirty,write back
	*	second ,its tlb , page table need to be updated
	 */
	if(PhysicalPageTable[ppn].valid){
		Thread *T=PhysicalPageTable[ppn].OwnerThread;
		if(T){
			T->space->pageTable[OldVpn].valid=false;
		}
		/* write back */
		if(T /* &&PhysicalPageTable[ppn].dirty /* &&pageTable[PhysicalPageTable[ppn].VirtualPageNumber].dirty*/){
			//printf("   Swap out \n");
			#ifdef DiskImage
			T->space->DiskAddrSpace->WriteAt(
				&(machine->mainMemory[ppn*PageSize]),
				PageSize,
				PhysicalPageTable[ppn].VirtualPageNumber*PageSize
			);
			#else
			memcpy(&(T->space->vSpace[PhysicalPageTable[ppn].VirtualPageNumber*PageSize]),
				&(machine->mainMemory[ppn*PageSize]),
				PageSize);
			#endif
			pageTable[OldVpn].dirty=false;
			T->space->pageTable[OldVpn].dirty=false;
			PhysicalPageTable[ppn].dirty=false;
		}
		pageTable[OldVpn].valid=false;

		/* update tlb*/
		for(int i=0;i<TLBSize;i++){
			if(tlb[i].valid&&tlb[i].physicalPage==ppn&&tlb[i].virtualPage==OldVpn){
				tlb[i].valid=false;
			}
		}

	}

	#ifdef DiskImage
	currentThread->space->DiskAddrSpace->ReadAt(
		&(machine->mainMemory[ppn*PageSize]),
		PageSize,
		vpn*PageSize
	);
	#else
	memcpy(&(machine->mainMemory[ppn*PageSize]),
			&(currentThread->space->vSpace[vpn*PageSize]),
			PageSize
			);
	#endif
	
	/*update the global physical page table*/
	PhysicalPageTable[ppn].LastHitTime		=stats->totalTicks;
	PhysicalPageTable[ppn].valid			=true;
	//PhysicalPageTable[ppn].dirty			=false;
	PhysicalPageTable[ppn].OwnerThread		=currentThread;
	PhysicalPageTable[ppn].VirtualPageNumber        =vpn;

	/*modify the page table in the hardware*/
	pageTable[vpn].valid		=true;
	//pageTable[vpn].dirty		=false;
	pageTable[vpn].use		=false;
	pageTable[vpn].readOnly		=false;
	pageTable[vpn].physicalPage	=ppn;
	pageTable[vpn].virtualPage	=vpn;
	pageTable[vpn].InTime		=stats->totalTicks;
	pageTable[vpn].LastHitTime	=stats->totalTicks;

	//TLB_PageTable_check();
	return ppn;
}
void Machine::TLB_PageTable_check(){
	for(int i=0;i<TLBSize;i++){
		if(tlb[i].valid){
			ASSERT(tlb[i].physicalPage==pageTable[tlb[i].virtualPage].physicalPage);
			ASSERT(pageTable[tlb[i].virtualPage].valid);
			ASSERT(PhysicalPageTable[tlb[i].physicalPage].valid);
		}
	}

	for(int i=0;i<pageTableSize;i++){
		if(pageTable[i].valid){
			ASSERT(PhysicalPageTable[pageTable[i].physicalPage].valid);
			ASSERT(PhysicalPageTable[pageTable[i].physicalPage].VirtualPageNumber==i);
		}
	}
/* 
	for(int i=0;i<NumPhysPages;i++){
		if(PhysicalPageTable[i].valid){
			ASSERT(pageTable[PhysicalPageTable[i].VirtualPageNumber].valid);
			ASSERT(pageTable[PhysicalPageTable[i].VirtualPageNumber].physicalPage==i);
			ASSERT(pageTable[PhysicalPageTable[i].VirtualPageNumber].dirty==PhysicalPageTable[i].dirty);
		}
	}
*/
}