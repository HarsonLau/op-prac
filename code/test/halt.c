/* halt.c
 *	Simple program to test whether running a user program works.
 *	
 *	Just do a "syscall" that shuts down the OS.
 *
 * 	NOTE: for some reason, user programs with global data structures 
 *	sometimes haven't worked in the Nachos environment.  So be careful
 *	out there!  One option is to allocate data structures as 
 * 	automatics within a procedure, but if you do this, you have to
 *	be careful to allocate a big enough stack to hold the automatics!
 */

#include "syscall.h"
void TestFork(){
	 int res=1;
	int i=0;
	for( i=0;i<1000;i++){
		res =res*2;
		if(res>i){
			Yield();
		}
	}
}
void func(){
	int tid1=Exec("sort");
	int tid2=Fork(TestFork);
	Join(tid2);
	Exit(0);
}
int
main()
{
/* not reached */
/*
	int fd1,fd2;
	int result;
	char buffer[20];
	Create("write.txt");
	fd1= Open("read.txt");
	fd2 = Open("write.txt");
	result = Read(buffer,20,fd1);
	Write(buffer,result,fd2);
	Close(fd1);
	Close(fd2);
*/
//Fork(TestFork);
//int tid1=Exec("sort");
//Join(tid1);
int i=Fork(func);
Exit(0);
//Exit(0);
//int tid=Exec("sort");
return 0;

}
