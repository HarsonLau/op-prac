// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"
#include "synch.h"

// testnum is set in main.cc
int testnum = 1;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
        int num;
        for (num = 0; num < 5; num++) {
                currentThread->Print(false);
                printf("looped %d times;\n",num+1);
                currentThread->Yield();
        }
}

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

        Thread *t = new Thread("forked thread");

        t->Fork(SimpleThread, (void*)1);
        SimpleThread(0);
}


//----------------------------------------------------------------------
// ThreadTest2
// 	create ThreadNumLimit +1 process
//----------------------------------------------------------------------
void ThreadTest2(){
    DEBUG('t', "Entering ThreadTest2");
    for(int i=0;i<=ThreadsNumLimit;i++){
            Thread *t=new Thread("forked thread in test 2");
            t->Print();
    }
}


//----------------------------------------------------------------------
// ThreadTest3
// 	print out all of the threads
//----------------------------------------------------------------------
void ThreadTest3(){
    DEBUG('t', "Entering ThreadTest3");
    for (int i=0;i<ThreadsNumLimit/2;i++){
            Thread *t =new Thread("thread");
    }
    ThreadShow();
}

//----------------------------------------------------------------------
// ThreadTest4
//      Test for preemptive scheduling based on priority
//      
//----------------------------------------------------------------------
void ThreadTest4(){

        DEBUG('t', "Entering ThreadTest4\n");
        Thread* t1=new Thread("thread1",157);
        t1->Print();
        t1->Fork(SimpleThread,(void*)1);

        scheduler->Print();

        Thread* t2=new Thread("thread2",126);
        t2->Print();
        t2->Fork(SimpleThread,(void*)2);

        scheduler->Print();

        Thread* t3=new Thread("thread3",148);
        t3->Print();
        t3->Fork(SimpleThread,(void*)3);
        scheduler->Print();

        Thread* t4=new Thread("thread4",138);
        t4->Print();
        t4->Fork(SimpleThread,(void*)4);
        scheduler->Print();

        currentThread->Yield();
        scheduler->Print();
}

//----------------------------------------------------------------------
// Reader writer problem using Semaphore
//----------------------------------------------------------------------
Semaphore mutex=Semaphore("mutex for reader counter",1);
Semaphore db=Semaphore("mutex for DataBase",1);
int ReaderCnt=0;
void reader(int a){
        while(1){
                mutex.P();
                ReaderCnt++;
                if(ReaderCnt==1)
                        db.P();
                mutex.V();
                printf("Reader %d is reading\n",currentThread->getTid());

                mutex.P();
                ReaderCnt--;
                if(ReaderCnt==0)
                        db.V();
                mutex.V();
        }
}
void writer(int a){
        while(1){
                db.P();
                printf("writer %d is writing\n",currentThread->getTid());
                db.V();
        }
}
void ThreadTest5(){
        DEBUG('t', "Entering ThreadTest4\n");
        Thread* r1=new Thread("Reader 1");
        Thread* r2=new Thread("Reader 2");
        Thread* r3=new Thread("Reader 3");
        Thread* r4=new Thread("Reader 4");
        Thread* r5=new Thread("Reader 5");
        Thread* w1=new Thread("writer 1");
        Thread* w2=new Thread("writer 2");
        r1->Fork(reader,(void*)1);
        r2->Fork(reader,(void*)1);
        r3->Fork(reader,(void*)1);
        w1->Fork(writer,(void*)2);
        w2->Fork(writer,(void*)2);
        r4->Fork(reader,(void*)1);
        r5->Fork(reader,(void*)1);
}
//----------------------------------------------------------------------
// Reader writer problem using Condition
//----------------------------------------------------------------------

int ActiveWriter=0;
int ActiveReader=0;
int WaitingWriter=0;
int WaitingReader=0;
Lock CntLock=Lock("Lock for reader-writer problem");
Condition Read=Condition("Read");
Condition Write=Condition("Write");
void CWriter(int a){
        CntLock.Acquire();
        //Whenever there is a Reader reading or waiting to read
        // the writer shall wait
        while(ActiveReader>0||WaitingReader>0){
                WaitingWriter++;
                Write.Wait(&CntLock);
                WaitingWriter--;
        }
        ActiveWriter++;
        CntLock.Release();
        printf("Writer %d is Writing\n!");
        CntLock.Acquire();
        //if there is Reader waiting,wake up them all
        if(WaitingReader>0)
                Read.Broadcast(&CntLock);
        //if no Reader is reading or waiting to read 
        // Let a writer to write
        else if (ActiveReader==0&&WaitingWriter>0){
                Write.Signal(&CntLock);
        }
        CntLock.Release();
}
void CReader(int a){
        CntLock.Acquire();//获取锁
        //When there is a writer writing ,wait
        if(ActiveWriter>0){
                WaitingReader++;//等待计数器自增
                Read.Wait(&CntLock);//先放弃锁，然后睡眠，然后被唤醒，获取锁
                WaitingReader--;//等待计数器自减
        }
        ActiveReader++;
        CntLock.Release();
        printf("Reader %d is Reading\n");
        CntLock.Acquire();
        ActiveReader--;
        //if there is any Reader waiting ,wake up them all
        if(WaitingReader>0)
                Read.Broadcast(&CntLock);
        else if (ActiveReader==0&&WaitingWriter>0)
                Write.Signal(&CntLock);
        CntLock.Release();
}
void ThreadTest6(){
        DEBUG('t', "Entering ThreadTest4\n");
        Thread* r1=new Thread("Reader 1");
        Thread* r2=new Thread("Reader 2");
        Thread* r3=new Thread("Reader 3");
        Thread* r4=new Thread("Reader 4");
        Thread* r5=new Thread("Reader 5");
        Thread* w1=new Thread("writer 1");
        Thread* w2=new Thread("writer 2");
        r1->Fork(CReader,(void*)1);
        r2->Fork(CReader,(void*)1);
        w1->Fork(CWriter,(void*)1);
        w2->Fork(CWriter,(void*)1);
        r3->Fork(CReader,(void*)1);
        r4->Fork(CReader,(void*)1);
        r5->Fork(CReader,(void*)1);
}


//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
        case 1:
                ThreadTest1();
                break;
        case 2:
                ThreadTest2();          //try to create ThreadNumLimit+1 threads
                break;
        case 3:
                ThreadTest3();          //crate some threads then call ThreadShow
                break;
        case 4:
                ThreadTest4();
                break;
        case 5:
                ThreadTest5();
        case 6:
                ThreadTest6();
        default:
                printf("No test specified.\n");
                break;
    }
}

