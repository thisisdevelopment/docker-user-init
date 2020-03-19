/* set user and group id and exec */

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *argv0;

static void usage(int exitcode)
{
    printf("Usage: %s command [args]\n", argv0);
    exit(exitcode);
}

void exec_cmd(char *args[], int ignore_exit_status)
{
    pid_t child, pid;
    int status;
    int link[2];

    if (pipe(link)==-1)
        err(1, "pipe");

    child = fork();
    if (child == 0) {
        dup2(link[1], STDOUT_FILENO);
        dup2(STDOUT_FILENO, STDERR_FILENO);
        close(link[0]);
        close(link[1]);
        execv(args[0], args);
        err(1, "Failed executing: %s", args[0]);
    }
    else if (child > 0) {
       int nbytes = 0;
       char output[4096 + 1];

       close(link[1]);
       memset(output, 0, 4096);
       while(0 != (nbytes = read(link[0], output, sizeof(output)))) {
           printf("%.*s\n", nbytes, output);
           memset(output, 0, 4096);
       }

       pid = wait(&status);
       if (pid < 0)
           err(1, "Failed waiting: %s", args[0]);

       if (!WIFEXITED(status))
           errx(1, "Something died: %s", args[0]);

       if (WEXITSTATUS(status) != 0 && !ignore_exit_status)
           errx(1, "Failed executing: %s, exit status: %d", args[0], WEXITSTATUS(status));
    }
    else {
        err(1, "Fork failed: %s", args[0]);
    }
}

int main(int argc, char *argv[])
{
    char *user, *group, **cmdargv;
    char *end;

    pid_t pid = getpid();
    uid_t uid = getuid();
    uid_t euid = geteuid();
    gid_t gid = getgid();

    user = getenv("DOCKER_USER");
    if (user == NULL)
        errx(1, "DOCKER_USER not set");

    group = strchr(user, ':');
    if (group == NULL)
       errx(1, "DOCKER_USER is missing a group");

    *group++ = '\0';

    if (euid != 0)
        errx(1, "This should run setuid/setgid");

    if (pid != 1)
        errx(1, "Should only be used with pid=1");

    struct passwd *orig_pw = getpwnam(user);
    if (orig_pw == NULL)
        errx(1, "No such user: %s", user);

    argv0 = argv[0];
    if (argc < 2)
        usage(0);

    char s_uid[32];
    sprintf(s_uid, "%d", uid);
    char s_gid[32];
    sprintf(s_gid, "%d", gid);

    if (uid != 0) {
        setuid(0);

        char *cmd_usermod[]={"/usr/sbin/usermod", "-u", s_uid, user, NULL};
        exec_cmd(cmd_usermod, 0);

        char *cmd_groupmod[]={"/usr/sbin/groupmod", "-g", s_gid, group, NULL};
        exec_cmd(cmd_groupmod, 0);

        char s_from_group[33];
        char s_to_group[33];
        sprintf(s_from_group, "%d", orig_pw->pw_gid);
        sprintf(s_to_group, ":%d", gid);

        char *cmd_chown_group[]={"/usr/bin/find", "/", "-xdev", "-group", s_from_group, "-exec", "/bin/chown", s_to_group, "{}", "+", NULL};
        exec_cmd(cmd_chown_group, 0);

        char s_from_user[33];
        sprintf(s_from_user, "%d", orig_pw->pw_uid);

        char *cmd_chown_user[]={"/usr/bin/find", "/", "-xdev", "-user", s_from_user, "-exec", "/bin/chown", s_uid, "{}", "+", NULL};
        exec_cmd(cmd_chown_user, 0);
    }

    struct passwd *pw = getpwnam(user);
    setenv("HOME", pw->pw_dir, 1);

    int ngroups = 0;
    gid_t *glist = NULL;

    while (1) {
        int r = getgrouplist(pw->pw_name, gid, glist, &ngroups);

        if (r >= 0) {
            if (setgroups(ngroups, glist) < 0)
                err(1, "setgroups");
            break;
        }

        glist = realloc(glist, ngroups * sizeof(gid_t));
        if (glist == NULL)
            err(1, "malloc");
    }

    if (setgid(gid) < 0)
        err(1, "setgid(%i)", gid);

    if (setuid(uid) < 0)
        err(1, "setuid(%i)", uid);

    cmdargv = &argv[0];
    cmdargv[0] = "/usr/bin/dumb-init";
    execvp(cmdargv[0], cmdargv);
    err(1, "%s", cmdargv[0]);

    return 1;
}