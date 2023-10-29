#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "wsh.h"

int tokenize(char *line, char *delimiter, char **argv);
void exit_command(int argc, char **argv);
void cd(int argc, char **argv);
void fg(int argc, char **argv);
void bg(int argc, char **argv);
void jobs_command(int argc, char **argv);
int job_is_stopped(struct job *j);
int job_is_completed(struct job *j);
job * find_job_by_id(int id);
void build_and_launch_job(char *line);
void launch_job(struct job *j, int is_foreground);
void launch_process(job *j, process *p, pid_t pgid, int infile, int outfile, int errfile, int foreground);
void init_shell();
int mark_process_status(process *p, int status);
void put_job_in_foreground(job *j);
void wait_for_process(process *p);
void wait_for_job(job *j);
int get_process_type(char *command);
void execute_buitin_command(process *p);
void remove_job(job *j);
job * find_job_by_id(int id);
job * get_last_job();
void add_to_joblist(job *j);
void Ctrl_signal_handler(int sig);
job * get_job_by_pid(pid_t pid);
void sigchild_handler();

job *jobs[256];
int shell_is_interactive = 1;
int shell_terminal;
int job_count = 0;
pid_t shell_pgid;
pid_t current_pid = -1;

int main(int argc, char *argv[])
{
  init_shell();
  if(argc == 1) {
    while (1)
  {
    signal(SIGINT, Ctrl_signal_handler);
    signal(SIGTSTP, Ctrl_signal_handler);
    signal(SIGCHLD, sigchild_handler);
    printf("wsh> ");
    char *line;
    size_t linecap = 0;
    if (getline(&line, &linecap, stdin) < 0)
    {
      exit(0);
    }
    // if no input provided continue
    build_and_launch_job(line);
  }
  } else {
    FILE *f = fopen(argv[1], "r");
    if(f != NULL){
      size_t size = 0;
      char *buf;
      while(getline(&buf, &size, f) > 0){
        build_and_launch_job(buf);
      }
    } else {
      return -1;
    }
  }

  return 0;
}

void Ctrl_signal_handler(int sig)
{
  if(current_pid > 0) {
    kill(current_pid, sig);
    current_pid = -1;
  }
}
void sigchild_handler() {
  int status;
  pid_t pid = waitpid(-1, &status, WNOHANG);
  job *j = get_job_by_pid(pid);
  if(j != NULL) {
    j = NULL;
    job_count--;
  }
}

job * get_job_by_pid(pid_t pid) {

  if(job_count <= 0 || pid < 0) {
    return NULL;
  }
  int i=0, j=0;
  while(i<job_count) {
    if(!jobs[j]) {
      j++;
      continue;
    }
    
    for(process *p = jobs[j]->first_process;p;p= p->next) {
      if(p->pid == pid) return jobs[j];
    }
    i++;
    j++;
  }
  return NULL;
}

void init_shell()
{

  /* See if we are running interactively.  */
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = 1; //isatty(shell_terminal)

  if (shell_is_interactive)
  {
    /* Loop until we are in the foreground.  */
    // while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
    //   kill(-shell_pgid, SIGTTIN);

    /* Ignore interactive and job-control signals.  */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    /* Put ourselves in our own process group.  */
    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(shell_terminal, shell_pgid);
  }
}

void build_and_launch_job(char *line)
{
  char **processes_strings = (char **)malloc(256 * sizeof(char *));
  int total_processes = tokenize(line, "|", processes_strings);
  if(total_processes == 0) return;

  int is_foreground = 1;
  job *j = (job *)malloc(sizeof(job));
  j->next = NULL;

  if (strcmp( line+(strlen(line) -1), "&") == 0) {
    is_foreground = 0;
    *(line+(strlen(line) -1)) = '\0';
  }
  struct process *first_process;
  struct process *prev_process;
  for (int i = 0; i < total_processes; i++)
  {
    char **argv = (char **)malloc(256 * sizeof(char *));
    int argc = tokenize(*(processes_strings+i), " ", argv);
    process *p = (process *)malloc(sizeof(process));
    if (i == 0)
      first_process = p;
    else
      prev_process->next = p;
    p->argc = argc;
    p->argv = argv;
    p->process_type = get_process_type(argv[0]);
    p->is_foreground = is_foreground;
    prev_process = p;
  }
  prev_process->next = NULL;
  j->first_process = first_process;
  j->stdin = STDIN_FILENO;
  j->stdout = STDOUT_FILENO;
  j->stderr = STDERR_FILENO; 
  j->foreground = is_foreground;
  if(!is_foreground) add_to_joblist(j);
  launch_job(j, is_foreground);
  if(is_foreground && j->first_process->process_type == EXTERNAL) { 
    remove_job(j);
  }
}

void add_to_joblist(job *j)
{
  if (j && j->first_process->process_type == EXTERNAL)
  {
    int i = 0;
    while (jobs[i] != NULL)
      i++;
    jobs[i] = j;
    j->job_id = i+1;
    job_count++;
  }
}
void remove_job(job *j) {
  if(j->job_id > 256 || j->job_id == 0) return;

  jobs[j->job_id - 1] = NULL;
  job_count--;
}

int get_process_type(char *command)
{
  if (command == NULL)
    return 0;

  else if (strcmp(command, "exit") == 0)
  {
    return EXIT;
  }

  else if (strcmp(command, "cd") == 0)
  {
    return CD;
  }
  else if (strcmp(command, "fg") == 0)
  {
    return FG;
  }
  else if (strcmp(command, "bg") == 0)
  {
    return BG;
  }
  else if (strcmp(command, "jobs") == 0)
  {
    return JOBS;
  }
  else
  {
    return EXTERNAL;
  }
}

int tokenize(char *line, char *delimiter, char **argv)
{
  int i = 0;
  char *token;
  while ((token = strsep(&line, delimiter)) != NULL)
  {
    if (strcmp(token, "") == 0 || strcmp(token, "\n") == 0)
      continue;
    argv[i++] = token;
    // ignore empty tokens and new line tokens if any
  }

  if (i == 0)
    return 0;
  // remove the new line char for the last argument
  if (argv[i - 1][strlen(argv[i - 1]) - 1] == '\n')
  {
    argv[i - 1][strlen(argv[i - 1]) - 1] = '\0';
  }
  argv[i] = NULL;
  return i;
}

//builtin commands start

void exit_command(int argc, char **argv)
{
  if (argc > 1)
  {
    exit(1);
  }
  else
  {
    exit(0);
  }
}

void cd(int argc, char **argv)
{
  if (argc == 1 || argc > 2)
  {
    printf("cd command accepts only 1 argument\n");
  }
  else if (chdir(argv[1]) == -1)
  {
    perror("chdir");
  }
}

void fg(int argc, char **argv)
{
  job *j;
  if (argc > 1)
  {
    int jobid = atoi(*(argv + 1));
    j = find_job_by_id(jobid);
  }
  else
  {
    j = get_last_job();
  }
  remove_job(j);
  if (!j)
    return;
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  tcsetpgrp(shell_terminal, j->pgid);
  if (kill(j->pgid, SIGCONT) < 0)
  {
    printf("job not found");
  }
  wait_for_job(j);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  tcsetpgrp(shell_terminal, shell_pgid);
}

void bg(int argc, char **argv) {
  job *j;
  if(argc > 1) {
    int jobid = atoi(*(argv+1));
    j = find_job_by_id(jobid);
  } else {
    j = get_last_job();
  }
  if(!j) return;
  j->foreground = 0;
  if (kill(j->pgid, SIGCONT) < 0)
  {
    printf("job not found");
  }
  tcsetpgrp(shell_terminal, shell_pgid);

}

void jobs_command(int argc, char **argv) {
  if(job_count <= 0) return;
  int i=0, j=0;
  while(i<job_count) {
    if(!jobs[j]) {
      j++;
      continue;
    }
    printf("%d:", jobs[j]->job_id);
     process *p;
  for (p = jobs[j]->first_process; p; p = p->next)
  {
    for(int k=0;k<p->argc;k++){
      printf(" %s", *((p->argv)+k));
    } 
    if(p->next) {
      printf(" |");
    }
  }
  if(jobs[j]->first_process->is_foreground == 0) {
    printf(" &");
  }
  printf("\n");
  i++;
  j++;
}

}
//builtin commands end

int job_is_completed(struct job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
  {
    if (!p->completed)
      return 0;
  }

  return 1;
}

int job_is_stopped(struct job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
  {
    if (!p->stopped || !p->completed)
      return 0;
  }

  return 1;
}

void launch_job(struct job *j, int is_foreground)
{
  int mypipe[2], infile, outfile;
  infile = j->stdin;

  for (process *p = j->first_process; p; p = p->next)
  {
    if (p->next)
    {
      if (pipe(mypipe) < 0)
      {
        perror("pipe");
        exit(1);
      }
      outfile = mypipe[1];
    }
    else
      outfile = j->stdout;

    launch_process(j,p, j->pgid, infile,
                   outfile, j->stderr, is_foreground);
  if (infile != j->stdin) close(infile);
  if (outfile != j->stdout) close(outfile);
  infile = mypipe[0];
  }
}

void wait_for_process(process *p)
{
  int status;
  waitpid(p->pid, &status, WUNTRACED);
  mark_process_status(p, status);
}

void wait_for_job(job *j) {
  process *curr_proc = j->first_process;
  while(curr_proc) {
    if(curr_proc->completed != 1) {
      wait_for_process(curr_proc);
    }
    curr_proc = curr_proc->next;
  }
}

int mark_process_status(process *p, int status)
{
  if (p->pid > 0)
  {
    p->status = status;
    if (WIFSTOPPED(status)) {
      p->stopped = 1;
      job *new_job = (job *)malloc(sizeof(job));
      new_job->first_process = p;
      new_job->pgid = p->pid;
      current_pid = p->pid;
      add_to_joblist(new_job);
    }
    else
    {
      p->completed = 1;
    }
    return 0;
  }
  return -1;
}

void launch_process(job *j, process *p, pid_t pgid,
                    int infile, int outfile, int errfile,
                    int foreground)
{

  if (p->process_type != EXTERNAL && p->process_type != 0)
  {
    execute_buitin_command(p);
    return;
  }

 
  // TODO//
  /* Exec the new process.  Make sure we exit.  */
  pid_t pid = fork();

  if (pid < 0)
  {
    return;
  }
  else if (pid == 0)
  {
     /* Set the handling for job control signals back to the default.  */
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTSTP, SIG_DFL);
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);

  /* Set the standard input/output channels of the new process.  */
  if (infile != STDIN_FILENO)
  {
    dup2(infile, STDIN_FILENO);
    close(infile);
  }
  if (outfile != STDOUT_FILENO)
  {
    dup2(outfile, STDOUT_FILENO);
    close(outfile);
  }
  if (errfile != STDERR_FILENO)
  {
    dup2(errfile, STDERR_FILENO);
    close(errfile);
  }
    p->pid = getpid();
    if (j->pgid > 0)
    {
      setpgid(0, j->pgid);
    }
    else
    {
      j->pgid = p->pid;
      setpgid(0, p->pid);
    }
    execvp(p->argv[0], p->argv);
    perror("execvp");
    exit(0);
  }
  else
  {
    p->pid = pid;
    if (j->pgid <= 0)
    {
      j->pgid = p->pid;
    }
    setpgid(pid, j->pgid);
    if (foreground)
    {
      tcsetpgrp(0, j->pgid);
      wait_for_process(p);
      signal(SIGTTOU, SIG_IGN);
      tcsetpgrp(0, shell_pgid);
      signal(SIGTTOU, SIG_DFL);
    }
  }

}

void execute_buitin_command(process *p)
{
  switch (p->process_type)
  {
  case EXIT:
    exit_command(p->argc, p->argv);
    break;
  case CD:
    cd(p->argc, p->argv);
    break;
  case JOBS:
    jobs_command(p->argc, p->argv);
    break;
  case FG:
    fg(p->argc, p->argv);
    break;
  case BG:
    bg(p->argc, p->argv);
    break;
  default:
    break;
  }
}

job * find_job_by_id(int id) {
  int i=0,j=0;
  while(i<job_count) {
    if(!jobs[j]){
      j++;
      continue;
    }
    else if(jobs[j] && jobs[j] ->job_id == id) {
      return jobs[j];
    } else{
      i++;
      j++;
    }
  }
  return NULL;
}

job * get_last_job() {
  int i=1,j=0;
  while(i<=job_count) {
    if(!jobs[j]) {
      j++;
      continue;
    }
    if(jobs[j] && i == job_count) {
      return jobs[j];
    }
    i++;
    j++;
  }
  return NULL;
}
