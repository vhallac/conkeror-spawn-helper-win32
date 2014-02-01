/**
 * (C) Copyright 2008 Jeremy Maitin-Shepard
 *
 * Use, modification, and distribution are subject to the terms specified in the
 * COPYING file.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <share.h>
#include <errno.h>

#include <fcntl.h>
#include <io.h>
#include <winsock2.h>

void msg(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
}

void fail(const char *msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

void failerr(char *msg)
{
    perror(msg);
    exit(1);
}

// Bug alert. This could lock up. while loop should contain "var = foo"
#define TRY(var, foo) var = foo; while (var == -1) { if(errno != EINTR) failerr(#foo); }

void *Malloc(size_t count)
{
    void *r = malloc(count);
    if (!r) fail("malloc");
    return r;
}

void *Realloc(void *buffer, size_t count)
{
    void *r = realloc(buffer, count);
    if (!r) fail("realloc");
    return r;
}

HANDLE fd_to_handle(int fd)
{
    return (void *)_get_osfhandle(fd);
}

/**
 * read_all: read from the specified file descriptor, returning a
 * malloc-allocated buffer containing the data that was read; the
 * number of bytes read is stored in *bytes_read.  If max_bytes is
 * non-negative, it specifies the maximum number of bytes to read.
 * Otherwise, read_all reads from the file descriptor until the end of
 * file is reached.
 */
char *read_all(int fd, int max_bytes, int *bytes_read)
{
    BOOL ok;
    int capacity = 256;
    char *buffer = Malloc(capacity);
    int count = 0;
    if (max_bytes > 0)
        capacity = max_bytes;
    if (max_bytes < 0 || max_bytes > 0) {
        while (1) {
            int remain;
            if (count == capacity) {
                capacity *= 2;
                buffer = Realloc(buffer, capacity);
            }
            remain = capacity - count;
            if (max_bytes > 0 && remain > max_bytes)
                remain = max_bytes;
            ok = ReadFile(fd_to_handle(fd), buffer+count, remain, &remain, NULL);
            if (!ok) break;
            count += remain;
            if (remain == 0 || count == max_bytes)
                break;
        }
    }
    *bytes_read = count;
    return buffer;
}

/**
 * read_all: read from the specified file descriptor, returning a
 * malloc-allocated buffer containing the data that was read; the
 * number of bytes read is stored in *bytes_read.  If max_bytes is
 * non-negative, it specifies the maximum number of bytes to read.
 * Otherwise, read_all reads from the file descriptor until the end of
 * file is reached.
 */
char *recv_all(int fd, int max_bytes, int *bytes_read)
{
    int capacity = 256;
    char *buffer = Malloc(capacity);
    int count = 0;
    if (max_bytes > 0)
        capacity = max_bytes;
    if (max_bytes < 0 || max_bytes > 0) {
        while (1) {
            int remain;
            if (count == capacity) {
                capacity *= 2;
                buffer = Realloc(buffer, capacity);
            }
            remain = capacity - count;
            if (max_bytes > 0 && remain > max_bytes)
                remain = max_bytes;
            TRY(remain, recv(fd, buffer + count, remain, 0));
            count += remain;
            if (remain == 0 || count == max_bytes)
                break;
        }
    }
    *bytes_read = count;
    return buffer;
}

/**
 * next_term: return the next NUL terminated string from buffer, and
 * adjust buffer and len accordingly.
 */
char *next_term(char **buffer, int *len)
{
    char *p = *buffer;
    int x = 0;
    int max_len = 1024;
    while (x < max_len && p[x])
        ++x;
    if (x == max_len)
        fail("error parsing");
    *buffer += x + 1;
    *len -= (x + 1);
    return p;
}

char *next_term_copy(char **buffer, int *len)
{
    char *tmp = next_term(buffer, len);
    char *rv = Malloc(strlen(tmp) + 1);
    strcpy(rv, tmp);
    return rv;
}

struct fd_info
{
    int desired_fd;
    int orig_fd;
    char *path;
    int open_mode;
    int perms;
};

void write_all(int fd, const char *buf, int len) {
  int result;
  do {
    TRY(result, _write(fd, buf, len));

    buf += result;
    len -= result;
  } while (len > 0);
}

void send_all(int fd, const char *buf, int len) {
  int result;
  do {
      TRY(result, send(fd, buf, len, 0));

    buf += result;
    len -= result;
  } while (len > 0);
}

/**
 * my_connect: Create a connection to the local Conkeror process on
 * the specified TCP port.  After connecting, the properly formatted
 * header specifying the client_key and the "role" (file descriptor or
 * -1 to indicate the control socket) are sent as well.  The file
 * descriptor for the socket is returned.
 */
int my_connect(int port, char *client_key, int role)
{
    int sockfd;
    int result;
    struct sockaddr_in sa;

    TRY(sockfd, socket(PF_INET, SOCK_STREAM, 0));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(sa.sin_zero, 0, sizeof(sa.sin_zero));

    TRY(result, connect(sockfd, (struct sockaddr *)&sa, sizeof(sa)));

    /* Send the client key */
    send_all(sockfd, client_key, strlen(client_key));

    /* Send the role */
    if (role < 0) {
        send_all(sockfd, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 15);
    }
    else {
        char buf[16];
        // snprintf for windows?
        sprintf(buf, "%15d", role);
        send_all(sockfd, buf, 15);
    }

    return sockfd;
}

int control_fd;

void check_duplicate_fds(struct fd_info *fds, int fd_count)
{
    int i, j;
    for (i = 0; i < fd_count; ++i) {
        for (j = i + 1; j < fd_count; ++j) {
            if (fds[i].desired_fd == fds[j].desired_fd)
                fail("duplicate redirection requested");
        }
    }
}

char *read_file(char *file_name, int *len_addr)
{
    char *buf;
    int file;

    file = _sopen(file_name, O_RDONLY, _SH_DENYNO, 0);
    if (file < 0) failerr("open");

    buf = read_all(file, -1, len_addr);
    close(file)
;
    return buf;
}

struct executable_info {
    char *client_key;
    char *server_key;
    char *workdir;
    char *executable;
    char *argstr;
    struct fd_info *fds;
    int fd_count;
};

void parse_key_file(char *file_name, struct executable_info *info_addr)
{
    char *buf;
    char *ptr;
    int len;
    int argc;
    int arglen = 1;
    int i;

    /* Read the entire file into buf. */
    buf = read_file(file_name, &len);
    remove(file_name);

    ptr = buf;
    info_addr->client_key = next_term_copy(&ptr, &len);

    info_addr->server_key = next_term_copy(&ptr, &len);
    info_addr->executable = next_term_copy(&ptr, &len);
    info_addr->workdir = next_term_copy(&ptr, &len);

    info_addr->argstr = Malloc(arglen);
    info_addr->argstr[0] = '\0';

    argc = atoi(next_term(&ptr, &len));
    for (i = 0; i < argc; ++i)
    {
        char *argv = next_term(&ptr, &len);
        arglen += strlen(argv) + 1;
        info_addr->argstr = Realloc(info_addr->argstr, arglen);
        if (i) strcat(info_addr->argstr, " ");
        strcat(info_addr->argstr, argv);
    }

    info_addr->fd_count = atoi(next_term(&ptr, &len));

    if (info_addr->fd_count < 0) fail("invalid fd count");
    info_addr->fds = Malloc(sizeof(struct fd_info) * info_addr->fd_count);
    for (i = 0; i < info_addr->fd_count; ++i) {
        info_addr->fds[i].desired_fd = atoi(next_term(&ptr, &len));
        info_addr->fds[i].path = next_term_copy(&ptr, &len);
        if (info_addr->fds[i].path[0]) {
            info_addr->fds[i].open_mode = atoi(next_term(&ptr, &len));
            info_addr->fds[i].perms = atoi(next_term(&ptr, &len));
        }
    }

    free(buf);

    if (len != 0) fail("invalid input file");
}

void free_resources(struct executable_info *exec_info)
{
    int i;

    free(exec_info->client_key);
    free(exec_info->server_key);
    free(exec_info->workdir);
    free(exec_info->executable);
    free(exec_info->argstr);
    for (i = 0; i != exec_info->fd_count; ++i)
    {
        free(exec_info->fds[i].path);
    }
    free(exec_info->fds);
}

void open_fds(struct fd_info *fds, int fd_count, int port, char *client_key)
{
    int i;

    /* Validate the file descriptor redirection request. */
    check_duplicate_fds(fds, fd_count);

    /* Create a socket connection or open a local file for each
       requested file descriptor redirection. */
    for (i = 0; i < fd_count; ++i) {
        if (fds[i].path[0]) {
            fds[i].orig_fd = _sopen(fds[i].path, fds[i].open_mode, _SH_DENYNO, fds[i].perms);
            if (fds[i].orig_fd < 0) failerr("open");
        } else {
            fds[i].orig_fd = my_connect(port, client_key, fds[i].desired_fd);
        }
    }
}

void check_server_key(int control_fd, char *server_key)
{
    int len = strlen(server_key);
    int read_len;
    char *buf = recv_all(control_fd, len, &read_len);
    if (len != read_len || memcmp(buf, server_key, len) != 0)
    {
        fail("server key mismatch");
    }
    free(buf);
}

HANDLE create_child_process(struct executable_info *exec_info)
{
    STARTUPINFO startup_info;
    PROCESS_INFORMATION process_info;
    BOOL ok;
    HANDLE *handle_loc[] = {
        &startup_info.hStdInput,
        &startup_info.hStdOutput,
        &startup_info.hStdError
    };
    int i;
    char *workdir;

    memset(&process_info, 0, sizeof(process_info));
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput  = INVALID_HANDLE_VALUE;
    startup_info.hStdOutput = INVALID_HANDLE_VALUE;
    startup_info.hStdError  = INVALID_HANDLE_VALUE;


    for (i=0; i < exec_info->fd_count; ++i) {
        if (exec_info->fds[i].desired_fd <= 2)
        {
            HANDLE fd_handle = (void *)_get_osfhandle(exec_info->fds[i].orig_fd);
            *(handle_loc[i]) = fd_handle;
        }
        else
        {
            // Silently ignore the problem, and hope that nobody will attempt unixy
            // things in Windows...
        }
    }

    workdir = exec_info->workdir;
    if (workdir && strlen(workdir) == 0) workdir = NULL;

    ok = CreateProcess(exec_info->executable,
	                   exec_info->argstr,
                       NULL /* lpProcessAttributes */,
                       NULL /* lpThreadAttributes */,
                       TRUE /* bInheritHandles */,
                       CREATE_NO_WINDOW /* dwCreationFlags. */,
                       NULL /* lpEnvironment */,
	                   workdir /* Current directory */,
                       &startup_info,
	                   &process_info);

    if (!ok) failerr("fork");

    return process_info.hProcess;
}

void wait_exit(HANDLE hControl, HANDLE hProcess)
{

    DWORD result;
    HANDLE objects[] = {
        hControl,
        hProcess
    };
    result = WaitForMultipleObjects(2,
	                                objects,
	                                FALSE /* bWaitAll */,
	                                INFINITE /* dwMilliSeconds */);
    switch (result)
    {
    case WAIT_OBJECT_0:
        /* Server asking us to kill child.
           TODO: Need to read data and make sure we are really being asked? */
        TerminateProcess(hProcess, 0 /* ExitCode */);
        break;
    case WAIT_OBJECT_0 + 1: {
        // TODO: plug in the application status here.
        char buffer[]="0";
        send_all(control_fd, buffer, strlen(buffer) + 1);
        Sleep(1000);
        break;
    }
    default:
        // TODO: Terminate it?
        break;
    }
}

void winsock_cleanup(void)
{
    WSACleanup();
}

int main(int argc, char **argv)
{
    int port;
    struct executable_info execinfo;
    HANDLE child_handle;

    WSADATA wsaData;

    atexit(winsock_cleanup);

    if (argc != 3 || (port = atoi(argv[2])) == 0)
        fail("Invalid arguments");

    if(WSAStartup(0x0002,&wsaData))
        fail("Cannot initialize winsock");

    parse_key_file(argv[1], &execinfo);

    control_fd = my_connect(port, execinfo.client_key, -1);

    open_fds(execinfo.fds, execinfo.fd_count, port, execinfo.client_key);

    check_server_key(control_fd, execinfo.server_key);

    child_handle = create_child_process(&execinfo);

    wait_exit((HANDLE)control_fd, child_handle);

    free_resources(&execinfo);

    return 0;
}
