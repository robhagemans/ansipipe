// dualmode.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <process.h> /*_beginthread, _endthread*/
#include <stdio.h>
#include <string.h>
// define bool, for C < C99
typedef enum { false, true } bool;


HANDLE cout_pipe, cin_pipe, cerr_pipe;
#define CONNECTIMEOUT 1000


// Create named pipes for stdin, stdout and stderr
// Parameter: process id
BOOL pipes_create(DWORD pid) 
{
	TCHAR name[256];
	sprintf(name, "\\\\.\\pipe\\%dcout", pid);
	if (INVALID_HANDLE_VALUE == (cout_pipe = CreateNamedPipe(
	            name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
		        1, 1024, 1024, CONNECTIMEOUT, NULL)))
		return 0;
	sprintf(name, "\\\\.\\pipe\\%dcin", pid);
	if (INVALID_HANDLE_VALUE == (cin_pipe = CreateNamedPipe(
	            name, PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
				1, 1024, 1024, CONNECTIMEOUT, NULL)))
		return 0;
	sprintf(name, "\\\\.\\pipe\\%dcerr", pid);
	if (INVALID_HANDLE_VALUE == (cerr_pipe = CreateNamedPipe(
	            name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
				1, 1024, 1024, CONNECTIMEOUT, NULL)))
		return 0;
	return 1;
}

// Close all named pipes
void pipes_close() 
{
    // Give pipes a chance to flush
    Sleep(100);
	CloseHandle(cout_pipe);
	CloseHandle(cerr_pipe);
	CloseHandle(cin_pipe);
}

// Thread function that handles incoming bytestreams to be output on stdout
void pipes_cout_thread(void* dummy) 
{
	TCHAR buffer[1024];
	DWORD count = 0;
	ConnectNamedPipe(cout_pipe, NULL);
	while (ReadFile(cout_pipe, buffer, 1024, &count, NULL)) 	{
		buffer[count] = 0;
        fprintf(stdout, "%s", buffer);
        fflush(stdout);
	}
}

// Thread function that handles incoming bytestreams to be outputed on stderr
void pipes_cerr_thread(void* dummy) 
{
	TCHAR buffer[1024];
	DWORD count = 0;
	ConnectNamedPipe(cerr_pipe, NULL);
	while (ReadFile(cerr_pipe, buffer, 1024, &count, NULL))	{
		buffer[count] = 0;
		fprintf(stderr, "%s", buffer);
		fflush(stderr);
	}
}

// Thread function that handles incoming bytestreams from stdin
void pipes_cin_thread(void* dummy)
{
    TCHAR buffer[1024];
	DWORD countr = 0;
	DWORD countw = 0;
	ConnectNamedPipe( cin_pipe, NULL);
	for(;;) {
		if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, 1024, &countr, NULL))
			break;
		if (!WriteFile(cin_pipe, buffer, countr, &countw, NULL))
			break;
	}
}

// Start handler pipe handler threads
void pipes_start_threads()
{
	_beginthread(pipes_cin_thread, 0, NULL);
	_beginthread(pipes_cout_thread, 0, NULL);
	_beginthread(pipes_cerr_thread, 0, NULL);
}


int main(int argc, char* argv[])
{
	// create command line string based on program name
    TCHAR module_name[MAX_PATH+1];
    GetModuleFileName(0, module_name, MAX_PATH);
    module_name[MAX_PATH] = 0;
    module_name[strlen(module_name)-4] = 0;
    TCHAR args[2048];
    TCHAR args_clean[2048];
    strncpy(args, GetCommandLine(), 2047);
    args[2047] = 0;

    // Find where to cut out the program name (which may be quoted)
    int offset = 0;
    TCHAR* ptr = args;
    bool quoted = false;
    if(args[0] == '\"') {
        quoted = true;
        offset++;
        ptr++;
    }
    while(*ptr) {
        offset++;
        if (quoted && *ptr == '\"') {
            offset++;
            break;
        }
        if (!quoted && *ptr == ' ')
            break;
        ptr++;
    }
    strncpy(args_clean, args + offset, 2047);
    args_clean[2047] = 0;  

    TCHAR cmdline[2048];
    // Build the create-process call
	sprintf(cmdline, "%s.exe %s", module_name, args_clean);
  
	// create process in suspended mode
	PROCESS_INFORMATION pinfo;
	STARTUPINFO sinfo;
	memset(&sinfo, 0, sizeof(STARTUPINFO));
	sinfo.cb = sizeof(STARTUPINFO);

	if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 
	                   CREATE_SUSPENDED, NULL, NULL, &sinfo, &pinfo)) {
        fprintf(stderr, "ERROR: Could not create process.\n");
		return 1;
	}
	if (!pipes_create(pinfo.dwProcessId)) {
        fprintf(stderr, "ERROR: Could not create named pipes.\n");
		return 1;
	}

	pipes_start_threads();

	// resume process
	ResumeThread(pinfo.hThread);
	WaitForSingleObject(pinfo.hProcess, INFINITE);
	pipes_close();

	ULONG exit_code;
	GetExitCodeProcess(pinfo.hProcess, (ULONG*) &exit_code);
	return exit_code;
}
