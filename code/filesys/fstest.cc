// fstest.cc 
//	Simple test routines for the file system.  
//
//	We implement:
//	   Copy -- copy a file from UNIX to Nachos
//	   Print -- cat the contents of a Nachos file 
//	   Perftest -- a stress test for the Nachos file system
//		read and write a really large file in tiny chunks
//		(won't work on baseline system!)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include "filesys.h"
#include "filehdr.h"
#include "system.h"
#include "thread.h"
#include "disk.h"
#include "stats.h"

#define TransferSize 	10 	// make it small, just to be difficult

//----------------------------------------------------------------------
// Copy
// 	Copy the contents of the UNIX file "from" to the Nachos file "to"
//----------------------------------------------------------------------

void
Copy(char *from, char *to)
{
    FILE *fp;
    OpenFile* openFile;
    int amountRead, fileLength;
    char *buffer;

// Open UNIX file
    if ((fp = fopen(from, "r")) == NULL) {	 
	printf("Copy: couldn't open input file %s\n", from);
	return;
    }

// Figure out length of UNIX file
    fseek(fp, 0, 2);		
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

// Create a Nachos file of the same length
    DEBUG('f', "Copying file %s, size %d, to file %s\n", from, fileLength, to);
    if (!fileSystem->Create(to, fileLength)) {	 // Create Nachos file
	printf("Copy: couldn't create output file %s\n", to);
	fclose(fp);
	return;
    }
    
    openFile = fileSystem->Open(to);
    ASSERT(openFile != NULL);
    
// Copy the data in TransferSize chunks
    buffer = new char[TransferSize];
    while ((amountRead = fread(buffer, sizeof(char), TransferSize, fp)) > 0)
	openFile->Write(buffer, amountRead);	
    delete [] buffer;

// Close the UNIX and the Nachos files
    delete openFile;
    fclose(fp);
}

//----------------------------------------------------------------------
// Print
// 	Print the contents of the Nachos file "name".
//----------------------------------------------------------------------

void
Print(char *name)
{
    OpenFile *openFile;    
    int i, amountRead;
    char *buffer;

    if ((openFile = fileSystem->Open(name)) == NULL) {
	printf("Print: unable to open file %s\n", name);
	return;
    }
    
    buffer = new char[TransferSize];
    while ((amountRead = openFile->Read(buffer, TransferSize)) > 0)
	for (i = 0; i < amountRead; i++)
	    printf("%c", buffer[i]);
    delete [] buffer;

    delete openFile;		// close the Nachos file
    return;
}

//----------------------------------------------------------------------
// PerformanceTest
// 	Stress the Nachos file system by creating a large file, writing
//	it out a bit at a time, reading it back a bit at a time, and then
//	deleting the file.
//
//	Implemented as three separate routines:
//	  FileWrite -- write the file
//	  FileRead -- read the file
//	  PerformanceTest -- overall control, and print out performance #'s
//----------------------------------------------------------------------

#define FileName 	"TestFile"
#define Contents 	"1234567890"
#define ContentSize 	strlen(Contents)
#define FileSize 	((int)(ContentSize * 5000))

static void 
FileWrite()
{
    OpenFile *openFile;    
    int i, numBytes;

    printf("Sequential write of %d byte file, in %d byte chunks\n", 
	FileSize, ContentSize);
    if (!fileSystem->Create(FileName, 0)) {
      printf("Perf test: can't create %s\n", FileName);
      return;
    }
    openFile = fileSystem->Open(FileName);
    if (openFile == NULL) {
	printf("Perf test: unable to open %s\n", FileName);
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Write(Contents, ContentSize);
	if (numBytes < 10) {
	    printf("Perf test: unable to write %s\n", FileName);
	    delete openFile;
	    return;
	}
    }
    delete openFile;	// close file
}

static void 
FileRead()
{
    OpenFile *openFile;    
    char *buffer = new char[ContentSize];
    int i, numBytes;

    printf("Sequential read of %d byte file, in %d byte chunks\n", 
	FileSize, ContentSize);

    if ((openFile = fileSystem->Open(FileName)) == NULL) {
	printf("Perf test: unable to open file %s\n", FileName);
	delete [] buffer;
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Read(buffer, ContentSize);
	if ((numBytes < 10) || strncmp(buffer, Contents, ContentSize)) {
	    printf("Perf test: unable to read %s\n", FileName);
	    delete openFile;
	    delete [] buffer;
	    return;
	}
    }
    delete [] buffer;
    delete openFile;	// close file
}

void
PerformanceTest()
{
    printf("Starting file system performance test:\n");
    stats->Print();
    FileWrite();
    FileRead();
    if (!fileSystem->Remove(FileName)) {
      printf("Perf test: unable to remove %s\n", FileName);
      return;
    }
    stats->Print();
}
void ex4_test(){
    if (!fileSystem->Create("/testdir", -1)) {
	    DEBUG('f',"can't create directory\n");
    }
    if (!fileSystem->Create("/testdir/test.txt", 6000)) {
	    DEBUG('f',"can't create test.txt in directory testdir \n");
    }
    if (!fileSystem->Remove("/testdir/test.txt")) {
	    DEBUG('f',"can't remove test.txt in directory testdir \n");
    }
    if (!fileSystem->Create("/testdir/test1.txt", 100)) {
	    DEBUG('f',"can't create test1.txt in directory testdir \n");
    }
    if (!fileSystem->Create("/testdir/test2.txt", 100)) {
	    DEBUG('f',"can't create test2.txt in directory testdir \n");
    }
    if(!fileSystem->Remove("/testdir")){
	    DEBUG('f',"can't remove /testdir  which is not empty\n");
    }

}
void ex5_test(){
    if (!fileSystem->Create("/test.txt", 600)) {
	    DEBUG('f',"can't create test.txt in directory testdir \n");
    }
    int sec=fileSystem->GetHeaderSector("/test.txt");
    OpenFile* opf=new OpenFile(sec);
    char * tmp=new char[6000];
    for(int i=0;i<6000;i++){
	    tmp[i]=(char)('0'+i%10);
    }
    tmp[5999]='\0';
    opf->WriteAt(tmp,6000,300);
    delete opf;
    delete tmp;
}
void ReaderFunc(int x){
	int sec=fileSystem->GetHeaderSector("/test.txt");
	OpenFile* opf=new OpenFile(sec);
	char * tmp=new char[600];
	for(int i=0;i<600;i++){
		tmp[i]=(char)('0'+i%10);
	}
	tmp[599]='\0';
	for(int cnt=0;cnt<10;cnt++){
		opf->ReadAt(tmp,200,200);
	}
	delete opf;
	delete tmp;
}
void WriterFunc(int x){
	int sec=fileSystem->GetHeaderSector("/test.txt");
	OpenFile* opf=new OpenFile(sec);
	char * tmp=new char[600];
	for(int i=0;i<600;i++){
		tmp[i]=(char)('0'+i%10);
	}
	tmp[599]='\0';
	for(int cnt=0;cnt<10;cnt++){
		opf->WriteAt(tmp,200,200);
	}
	delete opf;
	delete tmp;
}
void CleannerFunc(int x){
     while(!fileSystem->Remove("/test.txt")) {
	    DEBUG('f',"can't remove test.txt in directory testdir \n");
    }
}
void ex7_test(){
    if (!fileSystem->Create("/test.txt", 600)) {
	    DEBUG('f',"can't create test.txt in directory testdir \n");
    }
    Thread *R1=new Thread("Reader 1");
    Thread *R2=new Thread("Reader 2");
    Thread *W1=new Thread("Writer 1");
    Thread *W2=new Thread("Writer 2");
    Thread *C1=new Thread("Cleanner 1");
    R1->Fork(ReaderFunc,(void *)1);
    W1->Fork(WriterFunc,(void *)1);
    R2->Fork(ReaderFunc,(void *)1);
    W2->Fork(WriterFunc,(void *)1);
    C1->Fork(CleannerFunc,(void *)1);
}

void ex2_test(){
	FileHeader* fhdr=new FileHeader();
	fhdr->FetchFrom(0);
	fhdr->Print();
	delete fhdr;
}
void ex3_test(){
    if (!fileSystem->Create("/test.txt", 6000)) {
	    DEBUG('f',"can't create test.txt");
    }
    if (!fileSystem->Remove("/test.txt")) {
	    DEBUG('f',"can't remove test.txt");
    }
}
void MyTest(){
	//ex2_test();
//	ex3_test();
//	ex4_test();

//	ex5_test();

	ex7_test();

}
