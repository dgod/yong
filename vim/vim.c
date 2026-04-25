#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif
int y_run_tool(int t,int p,bool wait);
int main(int arc,char *arg[])
{
	int t=1,p=0,res;
	bool wait;
	
	for(int i=1;i<arc;i++)
	{
		if(!strcmp(arg[i],"--reload-all"))
		{
			wait=true;
			t=7;
		}
		else if(!strcmp(arg[i],"-w"))
		{
			wait=true;
		}
		else if(!strcmp(arg[i],"-t") && i<arc-1)
		{
			t=atoi(arg[++i]);
		}
		else
		{
			p=atoi(arg[i]);
		}
	}
	res=y_run_tool(t,p,wait);
	printf("%d\n",res);
#ifdef _WIN32
	if(AttachConsole((DWORD)-1))
		_cprintf("%d\n",res);
#endif
	return 0;
}
