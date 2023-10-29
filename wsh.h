#define max 100
#define EXIT 1
#define CD 2
#define FG 3
#define BG 4
#define JOBS 5
#define EXTERNAL 6

typedef struct process
{
  struct process *next;       /* next process in pipeline */
  char **argv;                /* for exec */
  int argc;                   /* count of arguments including command */
  pid_t pid;                  /* process ID */
  int completed;             /* true if process has completed */
  int stopped;               /* true if process has stopped */
  int status;                 /* reported status value */
  int is_foreground;          /*is process in foreground -- when job is created*/
  int process_type;             /*check whether process is inbuilt or external i.e, use execvp*/
}process;


/* A job is a pipeline of processes.  */
typedef struct job
{
  struct job *next;           /* next active job */
  process *first_process;     /* list of processes in this job */
  pid_t pgid;                 /* process group ID */
  int job_id;                 /* store job id       */
  int foreground;             /* represents foreground process for signal handelling*/
  int stdin, stdout, stderr;  /* standard i/o channels */
}job;
