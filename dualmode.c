// dualmode.cpp : Defines the entry point for the console application.
//

#define UNICODE
#include <windows.h>
#include <process.h> /*_beginthread, _endthread*/
#include <stdio.h>
#include <string.h>
// define bool, for C < C99
typedef enum { false, true } bool;


HANDLE cout_pipe, cin_pipe, cerr_pipe;
#define PIPES_TIMEOUT 1000
#define PIPES_BUFLEN 1024
#define ARG_BUFLEN 2048

// Create named pipes for stdin, stdout and stderr
// Parameter: process id
bool pipes_create(long pid) 
{
	wchar_t name[256];
	swprintf(name, L"\\\\.\\pipe\\%dcout", pid);
	if (INVALID_HANDLE_VALUE == (cout_pipe = CreateNamedPipe(
	            name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
		        1, PIPES_BUFLEN, PIPES_BUFLEN, PIPES_TIMEOUT, NULL)))
		return 0;
	swprintf(name, L"\\\\.\\pipe\\%dcin", pid);
	if (INVALID_HANDLE_VALUE == (cin_pipe = CreateNamedPipe(
	            name, PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
				1, PIPES_BUFLEN, PIPES_BUFLEN, PIPES_TIMEOUT, NULL)))
		return 0;
	swprintf(name, L"\\\\.\\pipe\\%dcerr", pid);
	if (INVALID_HANDLE_VALUE == (cerr_pipe = CreateNamedPipe(
	            name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
				1, PIPES_BUFLEN, PIPES_BUFLEN, PIPES_TIMEOUT, NULL)))
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
	TCHAR buffer[PIPES_BUFLEN];
	DWORD count = 0;
	ConnectNamedPipe(cout_pipe, NULL);
	while (ReadFile(cout_pipe, buffer, PIPES_BUFLEN, &count, NULL)) 	{
		buffer[count] = 0;
        fprintf(stdout, "%s", buffer);
        fflush(stdout);
	}
}

// Thread function that handles incoming bytestreams to be outputed on stderr
void pipes_cerr_thread(void* dummy) 
{
	TCHAR buffer[PIPES_BUFLEN];
	DWORD count = 0;
	ConnectNamedPipe(cerr_pipe, NULL);
	while (ReadFile(cerr_pipe, buffer, PIPES_BUFLEN, &count, NULL))	{
		buffer[count] = 0;
		fprintf(stderr, "%s", buffer);
		fflush(stderr);
	}
}

// Thread function that handles incoming bytestreams from stdin
void pipes_cin_thread(void* dummy)
{
    TCHAR buffer[PIPES_BUFLEN];
	DWORD countr = 0;
	DWORD countw = 0;
	ConnectNamedPipe( cin_pipe, NULL);
	for(;;) {
		if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, PIPES_BUFLEN, &countr, NULL))
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
    // get full program name, exclude extension
    wchar_t module_name[MAX_PATH+1];
    GetModuleFileName(0, module_name, MAX_PATH);
    module_name[MAX_PATH] = 0;
    module_name[wcslen(module_name)-4] = 0;

    // create command line for child process
    wchar_t cmd_line[ARG_BUFLEN];
    int count = wcslen(module_name) + 4;
    if (count > ARG_BUFLEN) {
        fprintf(stderr, "ERROR: Application name too long.\n");
        return 1;
    }
    wcscat(cmd_line, module_name);
    wcscat(cmd_line, L".exe");
 
    // write all arguments to child command line
    int i = 0;
    for (i = 1; i < argc; ++i) {
        int length = MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, NULL, 0);
        wchar_t wide_buffer[length];
        // convert UTF-8 -> UTF-16
        MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, wide_buffer, length);
        count += length + 1;
        if (count >= ARG_BUFLEN) break;
        wcscat(cmd_line, L" ");
        wcscat(cmd_line, wide_buffer);
    }

	// spawn child process in suspended mode and create pipes
	PROCESS_INFORMATION pinfo;
	STARTUPINFO sinfo;
	memset(&sinfo, 0, sizeof(STARTUPINFO));
	sinfo.cb = sizeof(STARTUPINFO);
	if (!CreateProcess(NULL, cmd_line, NULL, NULL, FALSE, 
	                   CREATE_SUSPENDED, NULL, NULL, &sinfo, &pinfo)) {
        fprintf(stderr, "ERROR: Could not create process.\n");
		return 1;
	}
	if (!pipes_create(pinfo.dwProcessId)) {
        fprintf(stderr, "ERROR: Could not create named pipes.\n");

		return 1;
	}

	// start the pipe threads and resume child process
    pipes_start_threads();
	ResumeThread(pinfo.hThread);
	
	// wait for child process to exit and close pipes
	WaitForSingleObject(pinfo.hProcess, INFINITE);
	pipes_close();
	ULONG exit_code;
	GetExitCodeProcess(pinfo.hProcess, (ULONG*) &exit_code);
	return exit_code;
}
