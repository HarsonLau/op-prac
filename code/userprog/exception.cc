// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "machine.h"
#include "filesys.h"
#include "addrspace.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
struct ForkInfo{
	AddrSpace* caller;
	int pc;	
};
void ForkWrapper(int x){;
	ForkInfo* info=(ForkInfo*)x;
	AddrSpace* addrspace=new AddrSpace();
	addrspace->CopyFrom(info->caller);
	currentThread->space=addrspace;
	addrspace->RestoreState();
	machine->WriteRegister(PCReg,info->pc);
	machine->WriteRegister(NextPCReg,(info->pc)+4);
	currentThread->SaveUserState();
	machine->Run();
}
void ExecWrapper(int x){
	ForkInfo* info=(ForkInfo*)x;
	currentThread->space=info->caller;
	int address=info->pc;
	int value;
	int count = 0;
	do{
		machine->ReadMem(address++,1,&value);
		count++;
	}while(value != 0);
	DEBUG('A',"filename length is %d.\n",count);
	address = address - count;
	char* fileName =new char[count];
	for(int i = 0; i < count; i++){
		machine->ReadMem(address+i,1,&value);
		fileName[i] = (char)value;
	}
	DEBUG('A',"filename  is %s.\n",fileName);
	OpenFile* executable=fileSystem->Open(fileName);
	if(!executable){
		DEBUG('a',"cannot find executable file %s\n",fileName);
		return;
	}
	AddrSpace* space=new AddrSpace(executable);
	currentThread->space=space;
	space->InitRegisters();
	space->RestoreState();
	delete fileName;
	delete executable;
	delete info;
	machine->Run();
}
void
ExceptionHandler(ExceptionType which)
{
	int type = machine->ReadRegister(2);
	if(which==SyscallException){
		switch (type){
		case SC_Halt:{
			DEBUG('A',"Halt ,initiated by user program tid =%d,name %s.\n",currentThread->getTid(),currentThread->getName());
			interrupt->Halt();
			break;
		}
		case SC_Exit:{
			if(1){
				for(int i=0;i<machine->pageTableSize;i++){
					if(machine->pageTable[i].valid){
						machine->pageTable[i].valid=false;
						if(PhysicalPageTable[machine->pageTable[i].physicalPage].valid){
							PhysicalPageTable[machine->pageTable[i].physicalPage].valid=false;
						}
					}
				}
				DEBUG('A',"thread %d %s finished with code %d\n",currentThread->getTid(),currentThread->getName(),machine->ReadRegister(4));
				machine->IncrementPC();
				currentThread->Finish();
			}
			else{
				machine->IncrementPC();
			}
			break;
		}
		case SC_Create:
		{
			DEBUG('A',"Create ,initiated by user program tid =%d.\n",currentThread->getTid());
			int address = machine->ReadRegister(4);
			int value;
			int count = 0;
			do{
				machine->ReadMem(address++,1,&value);
				count++;
			}while(value != 0);
			DEBUG('A',"filename length is %d.\n",count);
			address = address - count;
			char* fileName =new char[count];
			for(int i = 0; i < count; i++){
				machine->ReadMem(address+i,1,&value);
				fileName[i] = (char)value;
			}
			DEBUG('A',"fileName: %s.\n",fileName);
			fileSystem->Create(fileName,128);
			machine->IncrementPC();
			break;
		}
		case SC_Open:
		{
			DEBUG('A',"Open ,initiated by user program.\n");
			int address = machine->ReadRegister(4);
			int value;
			int count = 0;
			do{
				machine->ReadMem(address++,1,&value);
				count++;
			}while(value != 0);
			DEBUG('A',"filename length is %d.\n",count);
			address = address - count;
			char* fileName =new char[count];
			for(int i = 0; i < count; i++){
				machine->ReadMem(address+i,1,&value);
				fileName[i] = (char)value;
			}
			DEBUG('A',"fileName: %s.\n",fileName);
			OpenFile * openfile=fileSystem->Open(fileName);
			machine->WriteRegister(2,int(openfile));
			machine->IncrementPC();
			delete fileName;
			break;
		}
		case SC_Close:
		{
			DEBUG('A',"Close ,initiated by user program.\n");
			OpenFile* openfile=(OpenFile *)machine->ReadRegister(4);
			delete openfile;
			machine->IncrementPC();
			break;
		}
		case SC_Write:
		{
			DEBUG('A',"Write ,initiated by user program.\n");
			int addr =machine->ReadRegister(4);
			int size =machine ->ReadRegister(5);
			OpenFile * openfile =(OpenFile *)machine -> ReadRegister(6);
			char *data = new char [size];
			int tmp=0;
			for (int i=0;i<size;i++){
				machine->ReadMem(addr+i,1,&tmp);
				data[i]=(char )tmp;
			}
			if((int) openfile !=1){
				openfile->Write(data,size);
			}
			else{
				DEBUG('a',"Write to stdout\n");
				for(int i=0;i<size;i++){
					putchar(data[i]);
				}
			}
			machine->IncrementPC();
			delete data;
			break;
		}
		case SC_Read:
		{
			DEBUG('A',"Read ,initiated by user program.\n");
			int addr =machine->ReadRegister(4);
			int size =machine ->ReadRegister(5);
			OpenFile * openfile =(OpenFile *)machine -> ReadRegister(6);
			char *data = new char [size];
			int tmp=0;
			int res=0;
			if((int )(openfile)!=0){
				res=openfile->Read(data,size);
			}
			else{
				for(int i=0;i<size;i++)
					data[i]=getchar();
				res=size;
			}
			for(int i=0;i<size;i++){
				machine->WriteMem(addr+i,1,int(data[i]));
			}
			machine->WriteRegister(2,res);
			machine->IncrementPC();
			delete data;
			break;
		}
		case SC_Exec:
		{
			DEBUG('A',"Exec ,initiated by user program.\n");
			int address=machine->ReadRegister(4);
			Thread * t=new Thread("Exec thread",currentThread->getPriority()-1);
			ForkInfo * info=new ForkInfo;
			info->caller=currentThread->space;
			info->pc=address;
			t->Fork(ExecWrapper,info);
			machine->WriteRegister(2,t->getTid());
			machine->IncrementPC();
			break;
		}
		case SC_Fork:{
			DEBUG('A',"Fork ,initiated by user program.\n");
			DEBUG('A',"Tid %2d \n",currentThread->getTid());
			int funcPc=machine->ReadRegister(4);
			ForkInfo *info=new ForkInfo;
			info->caller=currentThread->space;
			info->pc=funcPc;
			Thread * t1=new Thread("Forked by system call");
			t1->Fork(ForkWrapper,info);
			machine->WriteRegister(2,t1->getTid());
			machine->IncrementPC();
			break;
		}
		case SC_Yield:{
			DEBUG('A',"Yield ,initiated by user program.\n");
			machine->IncrementPC();
			currentThread->Yield();
			break;
		}
		case SC_Join:{
			DEBUG('A',"Join ,initiated by user program.\n");
			int tid=machine->ReadRegister(4);
			DEBUG('A',"Join ,caller id = %d ,waiting id =%d",currentThread->getTid(),tid);
			while (TidMap[tid]&&tid!=currentThread->getTid()){
				DEBUG('A',"Yield caused by join\n");
				currentThread->Yield();
			}
			machine->IncrementPC();
			break;
		}
		case SC_RDir:{
			int address = machine->ReadRegister(4);
			int value;
			int count = 0;
			do{
				machine->ReadMem(address++,1,&value);
				count++;
			}while(value != 0);
			address = address - count;
			char fileName[count];
			for(int i = 0; i < count; i++){
				machine->ReadMem(address+i,1,&value);
				fileName[i] = (char)value;
			}
			rmdir(fileName);
			machine->IncrementPC();
			break;
		}
		case SC_CDir:{
			int address = machine->ReadRegister(4);
			int value;
			int count = 0;
			do{
				machine->ReadMem(address++,1,&value);
				count++;
			}while(value != 0);
			address = address - count;
			char fileName[count];
			for(int i = 0; i < count; i++){
				machine->ReadMem(address+i,1,&value);
				fileName[i] = (char)value;
			}
			mkdir(fileName,00777);
			machine->IncrementPC();
			break;
		}
		case SC_Remove:{
			int address = machine->ReadRegister(4);
			int value;
			int count = 0;
			do{
				machine->ReadMem(address++,1,&value);
				count++;
			}while(value != 0);
			address = address - count;
			char fileName[count];
			for(int i = 0; i < count; i++){
				machine->ReadMem(address+i,1,&value);
				fileName[i] = (char)value;
			}
			fileSystem->Remove(fileName);
			machine->IncrementPC();
			break;
		}
		case SC_Ls:{
			system("ls");
			machine->IncrementPC();
			break;
		}
		case SC_Pwd:{
			system("pwd");
			machine->IncrementPC();
			break;
		}
		case SC_Cd:{
			int address = machine->ReadRegister(4);
			int value;
			int count = 0;
			do{
				machine->ReadMem(address++,1,&value);
				count++;
			}while(value != 0);
			address = address - count;
			char fileName[count];
			for(int i = 0; i < count; i++){
				machine->ReadMem(address+i,1,&value);
				fileName[i] = (char)value;
			}
			chdir(fileName);
			machine->IncrementPC();
			break;
		}
		case SC_Help:{
			printf(" x     [path] execute the file specified\n");
			printf(" rmdir [path] remove the dir specified by path\n ");
			printf(" mkdir [path] create the dir specified by path\n ");
			printf(" rm    [path] remove the file specified by path\n ");
			printf(" ls    list all the file in the current dir\n");
			printf(" pwd   present working directory\n");
			printf(" help\n");
			machine->IncrementPC();
			break;
		}
		
		default:
			break;
		}
	}
	else if(which==PageFaultException){
		int virtAddr=machine->ReadRegister(BadVAddrReg);
		machine->LRU_TLB(virtAddr);
//		machine->FIFO_TLB(virtAddr);
	}
	else if(which==IllegalInstrException){
	int virtAddr=machine->registers[BadVAddrReg];
			int vpn = (unsigned) virtAddr / PageSize;
			printf("Ilegal Instruction exception,vpn=%d\n",vpn);
			ASSERT(FALSE);
	}
	else {
		DEBUG('a',"Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
    }
}
