// dualmode.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <iostream>
#include <process.h> /*_beginthread, _endthread*/
#include <stdio.h>

HANDLE coutPipe, cinPipe, cerrPipe;
#define CONNECTIMEOUT 1000


// Create named pipes for stdin, stdout and stderr
// Parameter: process id
BOOL 
CreateNamedPipes(DWORD pid)
{
	TCHAR name[256];

	sprintf(name, "\\\\.\\pipe\\%dcout", pid);
	if (INVALID_HANDLE_VALUE == (coutPipe = CreateNamedPipe(name, 
															PIPE_ACCESS_INBOUND, 
															PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
															1,
															1024,
															1024,
															CONNECTIMEOUT,
															NULL)))
		return 0;
	sprintf(name, "\\\\.\\pipe\\%dcin", pid);
	if (INVALID_HANDLE_VALUE == (cinPipe = CreateNamedPipe(name, 
															PIPE_ACCESS_OUTBOUND, 
															PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
															1,
															1024,
															1024,
															CONNECTIMEOUT,
															NULL)))
		return 0;
	sprintf(name, "\\\\.\\pipe\\%dcerr", pid);
	if (INVALID_HANDLE_VALUE == (cerrPipe = CreateNamedPipe(name, 
															PIPE_ACCESS_INBOUND, 
															PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
															1,
															1024,
															1024,
															CONNECTIMEOUT,
															NULL)))
		return 0;

	return 1;
}

// Close all named pipes
void
CloseNamedPipes()
{
  Sleep(100); //Give pipes a chance to flush
	CloseHandle(coutPipe);
	CloseHandle(cerrPipe);
	CloseHandle(cinPipe);
}


// Thread function that handles incoming bytesreams to be outputed
// on stdout
void 
OutPipeTh(void*)
{
	TCHAR buffer[1024];
	DWORD count = 0;

	ConnectNamedPipe(coutPipe, NULL);

	while(ReadFile(coutPipe, buffer, 1024, &count, NULL))
	{
		buffer[count] = 0;
    std::cout << buffer << std::flush;
	}
}

// Thread function that handles incoming bytesreams to be outputed
// on stderr
void 
ErrPipeTh(void*)
{
	TCHAR buffer[1024];
	DWORD count = 0;

	ConnectNamedPipe(cerrPipe, NULL);

	while(ReadFile(cerrPipe, buffer, 1024, &count, NULL))
	{
		buffer[count] = 0;
    std::cerr << buffer << std::flush;
	}
}

// Thread function that handles incoming bytesreams from stdin
void 
InPipeTh(void*)
{
	TCHAR buffer[1024];
	DWORD countr = 0;
	DWORD countw = 0;

	ConnectNamedPipe( cinPipe, NULL);

	for(;;)
	{
		if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE),
					  buffer,
					  1024,
					  &countr,
					  NULL))
				break;


		if (!WriteFile(cinPipe, 
					   buffer, 
					   countr, 
					   &countw, 
					   NULL))
			break;
	}
}

// Start handler pipe handler threads
void RunPipeThreads()
{
	_beginthread(InPipeTh, 0, NULL);
	_beginthread(OutPipeTh, 0, NULL);
	_beginthread(ErrPipeTh, 0, NULL);
}


int 
main(int , char* argv[])
{
	// create command line string based on program name
  TCHAR cLine[2048];
  TCHAR module_name[MAX_PATH+1];
  GetModuleFileName(0, module_name, MAX_PATH);
  module_name[MAX_PATH] = 0;
  module_name[strlen(module_name)-4] = 0;
  
  TCHAR cArgs[2048];
  TCHAR cArgsClean[2048];
  strncpy_s(cArgs, GetCommandLine(), 2047);
  cArgs[2048] = 0;

  //Find where to cut out the program name (which may be quoted)
  int offset=0;
  TCHAR* ptr = cArgs;
  bool inQuote = false;
  if(cArgs[0] == '\"')
  {
    inQuote = true;
    offset++;
    ptr++;
  }
  while(*ptr)
  {
    offset++;
    if(inQuote && *ptr == '\"')
    {
      offset++;
      break;
    }
    if(!inQuote && *ptr == ' ')
      break;

    ptr++;
  }
  strncpy_s(cArgsClean, cArgs + offset, 2047);
  cArgsClean[2048] = 0;  

  //Build the create process call
	sprintf(cLine, "%s.exe %s", module_name, cArgsClean);
  
	// create process in suspended mode
	PROCESS_INFORMATION pInfo;
	STARTUPINFO sInfo;
	memset(&sInfo, 0, sizeof(STARTUPINFO));
	sInfo.cb = sizeof(STARTUPINFO);
  //std::cout << "Calling `" << cLine << "`" << std::endl;
	if (!CreateProcess(NULL,
				  cLine, 
				  NULL,
				  NULL,
				  FALSE,
				  CREATE_SUSPENDED,
				  NULL,
				  NULL,
				  &sInfo,
				  &pInfo))
	{
    std::cerr << "ERROR: Could not create process." << std::endl;
		return 1;
	}

	if (!CreateNamedPipes(pInfo.dwProcessId))
	{
    std::cerr << "ERROR: Could not create named pipes." << std::endl;
		return 1;
	}

	RunPipeThreads();

	// resume process
	ResumeThread(pInfo.hThread);

	WaitForSingleObject(pInfo.hProcess, INFINITE);

	CloseNamedPipes();

	ULONG exitCode;

	GetExitCodeProcess(pInfo.hProcess, (ULONG*)&exitCode);

	return exitCode;
}
