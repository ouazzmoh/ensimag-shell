/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "readcmd.h"
#include "variante.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>


struct process_node *process_list = NULL;

struct process_node {
  pid_t pid;
  char* cmd_name;
  struct process_node * next;
};

void terminate(char *line) {
#if USE_GNU_READLINE == 1
  /* rl_clear_history() does not exist yet in centOS 6 */
  clear_history();
#endif
  if (line)
    free(line);
  printf("exit\n");
  exit(0);
}

void exec_cmd(struct cmdline *l){
    /* If input stream closed, normal termination */
  if (!l) {
    terminate(0);
  }

  if (l->err) {
    /* Syntax error, read another command */
    printf("error: %s\n", l->err);
    // continue;
  }

  if (l->in){printf("in: %s\n", l->in);}
  if (l->out){printf("out: %s\n", l->out);}
  if (l->bg){printf("background (&)\n");}


  //Number of the commands
  int num_cmd = 0;
  for (num_cmd = 0; l->seq[num_cmd]; num_cmd++){}//Calculate the lenght of commands
  //Creating pipes
  int pipe_fd[num_cmd - 1][2]; 
  if (num_cmd > 1){
    for (int k = 0; k < num_cmd-1; k++){
      if(pipe(pipe_fd[k]) < 0){
        exit(1); //Failure happened when creating the pipes
      }
    }
  }

  //Looping through the commands, forking, piping, executing
  for (int i = 0; i < num_cmd; i++) {
    char **cmd = l->seq[i];
    int pid = fork();
    if (pid == -1) {
        perror("fork() has resulted in an error\n");
    }
    //Child
    else if (pid == 0) {
      // Check for input redirection
      if (l->in) {
        int in_fd = open(l->in, O_RDONLY);
        if (in_fd == -1) {
          perror("Problem opening file descriptor\n");
          exit(1);
        }
        dup2(in_fd, 0);
        close(in_fd);
      }
      // Check for output redirection
      if (l->out) {
        int out_fd = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (out_fd == -1) {
          perror("Problem opening file descriptor\n");
          exit(1);
        }
        dup2(out_fd, 1);
        close(out_fd);
      }
      if (i > 0){
        //Use of pipes
        dup2(pipe_fd[i-1][0], 0); //associating stdin to read channel
        close(pipe_fd[i-1][1]);
        close(pipe_fd[i-1][0]);
      }
      if (i < num_cmd-1){
        dup2(pipe_fd[i][1], 1);//associating stdout to write canal
        close(pipe_fd[i][0]);
        close(pipe_fd[i][1]);
      }
      //Execute command
      execvp(cmd[0], cmd);
      exit(1);
    }
    //Parent
    else {
      if (i > 0){
        close(pipe_fd[i-1][0]);
        close(pipe_fd[i-1][1]);
      }
      // The parent process waits for the execution of the child
      if (!l->bg) {
        int status;
        waitpid(pid, &status, 0);
      }
      // The parent doesn't wait for the execution
      else {
        printf("The child is to be ran in the backgrounds\n");
          
          //Add process to background processes
          struct process_node *bg_process = (struct process_node *)malloc(sizeof( struct process_node));
          bg_process -> pid = pid;
          bg_process -> cmd_name = strdup(cmd[0]);

          if (process_list == NULL){
            process_list = bg_process;
            process_list -> next = NULL;
          }
          else{
            bg_process -> next = process_list;
            process_list  = bg_process;
          }
        }
      }
    }
}



int question6_executer(char *line) {
  /* Question 6: Insert your code to execute the command line
   * identically to the standard execution scheme:
   * parsecmd, then fork+execvp, for a single command.
   * pipe and i/o redirection are not required.
   */
  
  if (line == 0 || !strncmp(line, "exit", 4)) {
      terminate(line);
    }

    else if (!strncmp(line, "jobs", 4)) {
      struct process_node *currentNode = process_list;

      while (currentNode != NULL) {
          printf("the task's pid:%d; and name : %s\n", currentNode->pid, currentNode->cmd_name);
          currentNode = currentNode->next;
      }
    }

  struct cmdline *l = parsecmd(&line);
  exec_cmd(l);
  if (line){
    free(line);
  }
  return 0;
}



SCM executer_wrapper(SCM x) {
  return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif




void remove_child() {
    //Handler to be associated with the SIGCHLD process: removes the process from the list and prints int
    //Variante: Terminaison asynchrone
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {

      struct process_node *currentNode = process_list;
      struct process_node *prevNode = NULL;
      while (currentNode != NULL) {
          if (currentNode->pid == pid) {
              if (prevNode == NULL) {
                  process_list = currentNode -> next;
              } else {
                  prevNode->next = currentNode->next;
              }

              printf("The process with pid %d and command name %s\n has terminated\n", currentNode->pid, currentNode->cmd_name);

              free(currentNode->cmd_name);
              free(currentNode);
              break;
          }
          prevNode = currentNode;
          currentNode = currentNode->next;
      }
    }
}






int main() {
  printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);



#if USE_GUILE == 1
  scm_init_guile();
  /* register "executer" function in scheme */
  scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif



  //Associate the SIGCHLD signal with the removing function
  signal(SIGCHLD, remove_child);

  while (1) {
    struct cmdline *l;
    char *line = 0;
    char *prompt = "ensishell>";

    /* Readline use some internal memory structure that
       can not be cleaned at the end of the program. Thus
       one memory leak per command seems unavoidable yet */
    line = readline(prompt);

    //		int pid = fork();
    //		if (pid == 0){
    //			execve(line, NULL, NULL)
    //		}

    if (line == 0 || !strncmp(line, "exit", 4)) {
      terminate(line);
    }

    else if (!strncmp(line, "jobs", 4)) {
      struct process_node *currentNode = process_list;

      while (currentNode != NULL) {
          printf("the task's pid:%d; and name : %s\n", currentNode->pid, currentNode->cmd_name);
          currentNode = currentNode->next;
      }
    }

#if USE_GNU_READLINE == 1
    add_history(line);
#endif

#if USE_GUILE == 1
    /* The line is a scheme command */
    if (line[0] == '(') {
      char catchligne[strlen(line) + 256];
      sprintf(catchligne,
              "(catch #t (lambda () %s) (lambda (key . parameters) (display "
              "\"mauvaise expression/bug en scheme\n\")))",
              line);
      scm_eval_string(scm_from_locale_string(catchligne));
      free(line);
      continue;
    }
#endif

    /* parsecmd free line and set it up to 0 */
    l = parsecmd(&line);
    exec_cmd(l);
  }
}
