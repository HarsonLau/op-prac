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
        default:
                printf("No test specified.\n");
                break;
    }
}

