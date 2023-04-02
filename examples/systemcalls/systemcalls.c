#include "systemcalls.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>

#define ELOG(format, ...)                                       \
    fprintf(                                                    \
        stderr,                                                 \
        "ERROR (file=%s, line=%d, function=%s): " format "\n",  \
        __FILE__,                                               \
        __LINE__,                                               \
        __func__,                                               \
        __VA_ARGS__)

#ifdef DEBUG
#define DLOG(format, ...)                                           \
    printf(                                                         \
        "DEBUG(filename=%s, line=%d, function=%s): " format "\n",   \
        basename(__FILE__),                                         \
        __LINE__,                                                   \
        __func__,                                                   \
        __VA_ARGS__)
#else
#define DLOG(format, ...)
#endif

#define TRYC(expr, msg)                                         \
    if ((expr) == -1) {                                         \
        fprintf(                                                \
            stderr,                                             \
            "EXCEPTION (file=%s, line=%d, function=%s): %s\n",  \
            __FILE__,                                           \
            __LINE__,                                           \
            __func__,                                           \
            #expr);                                             \
        perror("ERROR (" msg ")");                              \
        goto done;                                              \
    } NULL

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    bool ok = false;
    int status;

    TRYC(status = system(cmd), "system");

    if (cmd == NULL && status == 0) {
        ELOG("%s","Shell couldn't be created");
    } else if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == 127) {
            ELOG("Command \"%s\" couldn't be executed", cmd);
        } else {
            ok = true;
        }
    }

  done:
    return ok;
}

void do_exec_child(char * const command[])
{
    TRYC(execv(command[0], command), "execv");

  done:
    exit(EXIT_FAILURE);
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/
bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    DLOG("count = %d", count);
    for (i=0; i<count; i++) {
        DLOG("command[%d] = %s", i, command[i]);
    }

    int status;
    pid_t pid;
    bool ok = false;

    TRYC(pid = fork(), "fork");
    if (pid == 0)
        do_exec_child(command);

    TRYC(waitpid(pid, &status, 0), "waitpid");
    DLOG("Command exited, status = %d", status);

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0) {
            ELOG("Command failed with status %d", WEXITSTATUS(status));
        } else {
            ok = true;
        }
    } else if (WIFSIGNALED(status)) {
        ELOG("Command received signal %d", WTERMSIG(status));
        if (WCOREDUMP(status)) {
            ELOG("%s", "Command produced a core dump");
        }
    } else {
        ELOG("Unknown status %d", status);
    }

    va_end(args);

  done:
    DLOG("do_exec done, ok = %d", ok);
    return ok;
}

void do_exec_redirect_child(const char *outputfile, char * const command[])
{
    int out_fd = -2;
    TRYC(
        out_fd = creat(outputfile, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
        "creat");
    TRYC(close(STDOUT_FILENO), "close");
    TRYC(dup2(out_fd, STDOUT_FILENO), "dup2");
    TRYC(close(out_fd), "close");
    
    TRYC(execv(command[0], command), "execv");

  done:
    if (out_fd >= 0)
        close(out_fd);

    exit(EXIT_FAILURE);
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    DLOG("count = %d", count);
    for (i=0; i<count; i++) {
        DLOG("command[%d] = %s", i, command[i]);
    }

    int status;
    pid_t pid;
    bool ok = false;

    TRYC(pid = fork(), "fork");
    if (pid == 0)
        do_exec_redirect_child(outputfile, command);

    TRYC(waitpid(pid, &status, 0), "waitpid");
    DLOG("Command exited, status = %d", status);

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0) {
            ELOG("Command exited with status %d", WEXITSTATUS(status));
        } else {
            ok = true;
        }
    } else if (WIFSIGNALED(status)) {
        ELOG("Command received signal %d", WTERMSIG(status));
        if (WCOREDUMP(status)) {
            ELOG("%s","Command produced a core dump");
        }
    } else {
        ELOG("Unknown status %d", status);
    }

    va_end(args);

  done:
    DLOG("do_exec_redirect done, ok = %d", ok);
    return ok;
}

