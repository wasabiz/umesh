#include <string.h>
#include <stdio.h>

#include <signal.h>
#include <sys/wait.h>

#include "parse.h"
#include "xvect.h"

void print_job_list(job *);
void exec_job_list(job *);

xvect cz_jobs;                 /* list of pgids of suspended jobs */

void
do_sigchld(int sig)
{
  pid_t pid, pgid;
  int status;
  size_t i;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (WIFSTOPPED(status)) {
      pgid = getpgid(pid);
      for (i = 0; i < xv_size(&cz_jobs);) {
        if (*(pid_t *)xv_get(&cz_jobs, i) == pgid) {
          xv_splice(&cz_jobs, i, 1);
        } else {
          ++i;
        }
      }
      xv_push(&cz_jobs, &pgid);
    }
  }
}

void
do_sigtstp(int sig)
{
  /* pass */
}

void
init(void)
{
  struct sigaction ign, chld, stp;

  ign.sa_handler = SIG_IGN;
  ign.sa_flags = 0;
  sigemptyset(&ign.sa_mask);
  sigaction(SIGTTOU, &ign, NULL);

  /**
   * To avoid SIG_IGN being inherited to child processes,
   * registering a newly defined empty handler to SIGTSTP and SIGINT
   */
  stp.sa_handler = do_sigtstp;
  stp.sa_flags = 0;
  sigemptyset(&stp.sa_mask);
  sigaction(SIGTSTP, &stp, NULL);
  sigaction(SIGINT, &stp, NULL);

  chld.sa_handler = do_sigchld;
  chld.sa_flags = 0;
  sigemptyset(&chld.sa_mask);
  sigaction(SIGCHLD, &chld, NULL);
}

void
exec_bg(void)
{
  sigset_t sigset;
  pid_t pgid;

  sigemptyset(&sigset);
  sigaddset(&sigset, SIGCHLD);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  if (xv_size(&cz_jobs) > 0) {
    pgid = *(pid_t *)xv_pop(&cz_jobs);
    kill(-pgid, SIGCONT);
  } else {
    fprintf(stderr, "no suspended process\n");
  }

  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}

int
main(int argc, char *argv[]) {
  char s[LINELEN];
  job *curr_job;

  xv_init(&cz_jobs, sizeof(pid_t));

  init();

  while (get_line(s, LINELEN)) {
    if (! strcmp(s, "exit\n"))
      break;

    if (! strcmp(s, "bg\n")) {
      exec_bg();
    }
    else {
      curr_job = parse_line(s);

#if 0
      print_job_list(curr_job);
#endif

      exec_job_list(curr_job);
    }

    free_job(curr_job);
  }

  xv_destroy(&cz_jobs);

  return 0;
}
