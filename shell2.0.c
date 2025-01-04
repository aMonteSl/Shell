#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <glob.h>
#include <sys/wait.h>
#include <fcntl.h>

enum {
    MAX_LINE = 1024,
    PATH_MAX = 4096
};

typedef struct LineToken {
    char *line;
    char **tokens;
} LineToken;

typedef struct Redirection
{
    int isinputredirect;
    int isoutputredirect;
    char *inputfile;
    char *outputfile;
} Redirection;

typedef struct HereDoc {
    char *lines;
    int size;
} HereDoc;

void 
siginthandler(int sig) {
    printf("\nCaught signal %d (SIGINT). Type 'exit' to quit the shell.\n", sig);
}


int
itisterminal(void){
    struct stat statbuf;
    fstat(STDIN_FILENO, &statbuf);
    return S_ISCHR(statbuf.st_mode);

}

void
initshell(void){
    // char *homedir;
    // homedir = getenv("HOME");

    // if (homedir == NULL) {
    //     fprintf(stderr, "Error: HOME environment variable not set. Using current directory.\n");
    //     return;
    // }
    // if (chdir(homedir) != 0) {
    //     fprintf(stderr, "Error: Failed to change directory to HOME (%s): %s\n", homedir, strerror(errno));
    //     fprintf(stderr, "Using current directory as fallback.\n");
    // }

     if (setenv("result", "0", 1) != 0) {
        perror("setenv");
     } 
}


void
initlinetoken(LineToken *lt) {
    lt->line = NULL;
    lt->tokens = NULL;
}


void
checkbackgroundchilds() {
    int status;
    int pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[%d]+ Done\n", pid);
    }
}

char*
getenvvar(char *var) {
    char *envvar;
    envvar = getenv(var);
    if (envvar == NULL) {
        fprintf(stderr, "Error: %s environment variable not set.\n", var);
    }
    return envvar;
}


void
printpromt(void){
    char *user;
    char *current_dir;

    user = getenvvar("USER");

    current_dir = getcwd(NULL, 0);

    if (current_dir == NULL) {
        perror("getcwd");
        current_dir = getenvvar("HOME");
    }

    printf("%s@:%s$ ", user, current_dir);
    free(current_dir);
}



void
readline(char** pline, int isterminal){
    char line[MAX_LINE] = "";

    if (isterminal){
        printpromt();
    }

    if (fgets(line, MAX_LINE, stdin) != NULL) {
        *pline = strdup(line);
    } else {
        *pline = NULL;
    }
    

}

void freeline(char **line) {
    if (*line != NULL) {
        free(*line);
        *line = NULL;
    }
}

void freetokens(char ***tokens) {
    if (*tokens != NULL) {
        for (int i = 0; (*tokens)[i] != NULL; i++) {
            free((*tokens)[i]);
        }
        free(*tokens);
        *tokens = NULL;
    }
}

void
freelinetoken(LineToken *lt) {

    if (lt == NULL) {
        return;
    }

    freeline(&lt->line);
    freetokens(&lt->tokens);
}

char**
inittokens() {
    char **tokens = malloc(MAX_LINE * sizeof(char *));
    if (tokens == NULL) {
        perror("malloc");
    }
    return tokens;
}

int
addtoken(char **tokens, int index, char *token) {
    tokens[index] = strdup(token);
    if (tokens[index] == NULL) {
        perror("strdup");
        return -1;
    }
    return 0;
}

char** tokenize(char *line) {
    char **tokens;
    char *saveptr;
    char *token;
    int i = 0;

    if (line == NULL) {
        return NULL;
    }

    tokens = inittokens();
    if (tokens == NULL) {
        return NULL;
    }

    token = strtok_r(line, " \t\n", &saveptr);

    while (token != NULL) {
        if (addtoken(tokens, i, token) == -1) {
            freetokens(&tokens);
            return NULL;
        }
        i++;
        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    tokens[i] = NULL;
    return tokens;
}


// char** 
// tokenize(char *line) {
//     char **tokens;
//     char *saveptr;
//     char *token;
//     int i = 0;

//     if (line == NULL){
//         return NULL;
//     }

//     tokens = malloc(MAX_LINE * sizeof(char *));
//     if (tokens == NULL) {
//         perror("malloc");
//         return NULL;
//     }

//     token = strtok_r(line, " \t\n", &saveptr);

//     while (token != NULL) {
//         tokens[i] = strdup(token);
//         if (tokens[i] == NULL) {
//             perror("strdup");
//             for (int j = 0; j < i; j++) {
//                 free(tokens[j]);
//             }
//             free(tokens);
//             return NULL;
//         }
//         i++;
//         token = strtok_r(NULL, " \t\n", &saveptr);
//     }

//     tokens[i] = NULL;
//     return tokens;
// }

void
removequote(char *token) {
    int len = strlen(token);
    if (len > 0 && token[0] == '"') {
        for (int j = 0; j < len; j++) {
            token[j] = token[j + 1];
        }
        len--;
    }
    if (len > 0 && token[len - 1] == '"') {
        token[len - 1] = '\0';
    }
}

void
removequotes(char **tokens) {
    int i = 0;

    while (tokens[i] != NULL) {
        removequote(tokens[i]);
        i++;
    }
}


// void
// removequotes(char **tokens) {
//     int len;
//     int i = 0;
//     int j;

//     while (tokens[i] != NULL) {
//         len = strlen(tokens[i]);
//         if (len > 0 && tokens[i][0] == '"') {

//             for (j = 0; j < len; j++) {
//                 tokens[i][j] = tokens[i][j + 1];
//             }
//             len--;
//         }
//         if (len > 0 && tokens[i][len - 1] == '"') {
//             tokens[i][len - 1] = '\0';
//         }
//         i++;
//     }
// }

int
replacetoken(char **token, char *newvalue) {
    char *newtoken = strdup(newvalue);
    if (newtoken == NULL) {
        perror("strdup");
        return 1;
    }
    free(*token);
    *token = newtoken;
    return 0;
}

int replaceenvvars(char **tokens) {
    char *envvar;
    int i = 0;

    while (tokens[i] != NULL) {
        if (tokens[i][0] == '$') {
            envvar = getenvvar(tokens[i] + 1);
            if (envvar == NULL) {
                return 1;
            }
            if (replacetoken(&tokens[i], envvar)) {
                return 1;
            }
        }
        i++;
    }
    return 0;
}


// int
// replaceenvvars(char **tokens) {
//     char *envvar;
//     char *new_token;
//     int i = 0;

//     while (tokens[i] != NULL) {
//         if (tokens[i][0] == '$') {
//             envvar = getenv(tokens[i] + 1);
//             if (envvar == NULL) {
//                 fprintf(stderr, "error: var %s does not exist\n", tokens[i] + 1);
//                 return 0;
//             }
//             new_token = strdup(envvar);
//             if (new_token == NULL) {
//                 perror("strdup");
//                 return 0;
//             }
//             free(tokens[i]);
//             tokens[i] = new_token;
//         }
//         i++;
//     }
//     return 1;
// }

int
nolinetoken(LineToken *lt) {
    return lt->line == NULL || lt->tokens == NULL || lt->tokens[0] == NULL;
}


void
globbing(char ***tokens) {
    int flags;
    int i;
    int j;
    char **new_tokens;
    size_t count;
    glob_t globbuf;
    memset(&globbuf, 0, sizeof(globbuf));

    flags = GLOB_NOCHECK;
    for (i = 0; (*tokens)[i] != NULL; i++) {
        if (i > 0) flags |= GLOB_APPEND;
        if (glob((*tokens)[i], flags, NULL, &globbuf) != 0) {
            perror("glob");
        }
    }

    freetokens(tokens);

    count = globbuf.gl_pathc;
    new_tokens = malloc((count + 1) * sizeof(char *));
    if (new_tokens == NULL) {
        perror("malloc");
        globfree(&globbuf);
        return;
    }


    for (i = 0; i < count; i++) {
        new_tokens[i] = strdup(globbuf.gl_pathv[i]);
        if (new_tokens[i] == NULL) {
            perror("strdup");
            for (j = 0; j < i; j++) {
                free(new_tokens[j]);
            }
            free(new_tokens);
            globfree(&globbuf);
            return;
        }
    }
    new_tokens[count] = NULL;


    globfree(&globbuf);


    *tokens = new_tokens;
}

int
isifok(char **tokens) {
    return strcmp(tokens[0], "ifok") == 0;
}

int
isifnot(char **tokens) {
    return strcmp(tokens[0], "ifnot") == 0;
}

int
exitwitherror(void) {
    int correct;
    correct = atoi(getenv("result"));
    if (correct != 0) {
        return 1;
    }
    return 0;
}

int
exitwithsuccess(void) {
    int correct;
    correct = atoi(getenv("result"));
    if (correct == 0) {
        return 1;
    }
    return 0;
}


int
exitcommand(char **tokens) {
    if (strcmp(tokens[0], "exit") == 0) {
        return 1;
    }
    return 0;
}

int
isenvassignment(char **tokens) {
    return strchr(tokens[0], '=') != NULL;
}


void
handleenvassignment(char **tokens) {
    char *env_assignment = strdup(tokens[0]);
    char *key = strtok(env_assignment, "=");
    char *value = strtok(NULL, "=");

    if (key && value) {
        setenv(key, value, 1);
    } else {
        fprintf(stderr, "Invalid environment variable assignment: %s\n", tokens[0]);
    }
    free(env_assignment);
}
int
builtincd(char **tokens) {
    return strcmp(tokens[0], "cd") == 0;
}

void
changeresult(int result) {
    char result_str[10];
    snprintf(result_str, sizeof(result_str), "%d", result);
    setenv("result", result_str, 1);
}

int changedir(char *dir) {
    if (chdir(dir) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

void changecwd(char **tokens) {
    char *homedir;
    int result = 0;

    if (tokens[1] == NULL) {
        homedir = getenvvar("HOME");
        if (homedir == NULL) {
            result = 1;
        } else {
            result = changedir(homedir);
        }
    } else {
        result = changedir(tokens[1]);
    }

    changeresult(result);
}



// Wait for a child process
void
waitchild(int pid) {
    int pidwait;
    int status;
    int exit_status;

    do {
        pidwait = waitpid(pid, &status, 0);
        if (pidwait == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
            exit_status = WEXITSTATUS(status);
            changeresult(exit_status);
            // int exit_status = WEXITSTATUS(status);
            // char result_str[10];
            // snprintf(result_str, sizeof(result_str), "%d", exit_status);
            // setenv("result", result_str, 1);
            if (exit_status != 0) {
                printf("El proceso hijo terminó con un código de salida distinto de 0: %d\n", exit_status);
            }
        } else if (WIFSIGNALED(status)) {
            printf("El proceso hijo fue terminado por una señal: %d\n", WTERMSIG(status));
        }
    } while (pidwait != pid);
}

// Check if there is any input redirection
int
isinputredirect(char *token) {
    return strcmp(token, "<") == 0;
}

// Check if there is any output redirection
int
isoutputredirect(char *token) {
    return strcmp(token, ">") == 0;
}

// Set the redirection values
void
setredirection(int *redirectflag, char **file, char *token) {
    *redirectflag = 1;
    *file = strdup(token);
    if (*file == NULL) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
}

// Remove redirection tokens from the tokens array
void removeredirectiontokens(char **tokens, int i) {
    int j;
    for (j = 0; j < 2; j++) {
        erasetoken(tokens, tokens[i]);
    }
}
void 
manageredirections(char **tokens, Redirection *redir) {
    int i;

    for (i = 0; tokens[i] != NULL; i++) {
        if (isinputredirect(tokens[i])) {
            // *input_redirect = 1;
            // *input_file = tokens[i + 1];
            setredirection(&redir->isinputredirect, &redir->inputfile, tokens[i + 1]);
            removeredirectiontokens(tokens, i);
            i--;
        } else if (isoutputredirect(tokens[i])) {
            // *output_redirect = 1;
            // *output_file = tokens[i + 1];
            // for (int j = i; tokens[j] != NULL; j++) {
            //     tokens[j] = tokens[j + 2];
            // }
            setredirection(&redir->isoutputredirect, &redir->outputfile, tokens[i + 1]);
            removeredirectiontokens(tokens, i);
            i--;
        }
    }
}

void
redirectinput(char *input_file) {
    int fd;
    fd = open(input_file, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
}

void
redirectoutput(char *output_file) {
    int fd;
    fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
}

int
islocalcommand(char *command) {
    // Check if the command is a local command (starts with "./")
    return strncmp(command, "./", 2) == 0;
}

// Remove "./" from the command of the first token
void
removedotslash(char *token) {
    int i;
    int len;

    len = strlen(token);
    for (i = 0; i < len - 1; i++) {
        token[i] = token[i + 2];
    }
    token[len - 2] = '\0';
}

char* buildlocalpath(char *command){
    char cwd[PATH_MAX];
    char *commandpath;

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        commandpath = malloc(strlen(cwd) + strlen(command) + 2);
        if (commandpath == NULL) {
            perror("malloc");
            return NULL;
        }
        sprintf(commandpath, "%s/%s", cwd, command);
        return commandpath;
    } else {
        perror("getcwd");
        return NULL;
    }
}

char*
findcommandinpath(char *command) {
    char *path;
    char *path_copy;
    char *full_path;
    char *dir;

    path = getenv("PATH");
    if (path == NULL) {
        return NULL;
    }

    path_copy = strdup(path);
    if (path_copy == NULL) {
        perror("strdup");
        return NULL;
    }

    full_path = malloc(MAX_LINE);
    if (full_path == NULL) {
        perror("malloc");
        free(path_copy);
        return NULL;
    }

    dir = strtok(path_copy, ":");
    while (dir != NULL) {
        snprintf(full_path, MAX_LINE, "%s/%s", dir, command);
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return full_path;
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    free(full_path);
    fprintf(stderr, "Command not found in PATH: %s\n", command);
    return NULL;
}

char*
buildpath(char *command){
    if (islocalcommand(command)) {
        removedotslash(command);
        return buildlocalpath(command);
    } else {
        return findcommandinpath(command);
    }
}

void
initredirect(Redirection *redir) {
    redir->isinputredirect = 0;
    redir->isoutputredirect = 0;
    redir->inputfile = NULL;
    redir->outputfile = NULL;
}

void freeredirections(Redirection *redir) {
    if (redir->inputfile != NULL) {
        free(redir->inputfile);
        redir->inputfile = NULL;
    }
    if (redir->outputfile != NULL) {
        free(redir->outputfile);
        redir->outputfile = NULL;
    }
}

void
freeall(LineToken *lt, Redirection *redir, HereDoc *heredoc) {
    freelinetoken(lt);
    free(lt);
    freeredirections(redir);
    free(redir);
    free(heredoc);
    lt = NULL;
    redir = NULL;
    heredoc = NULL;
}

int 
ishere(char **tokens) {
    int i;
    for (i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "HERE{") == 0) {
            return 1;
        }
    }
    return 0;
}

int
noredirects(Redirection *redir) {
    return !redir->isinputredirect && !redir->isoutputredirect;
}

// Read the lines of the HERE command until we find a }
void
readheredoc(HereDoc *heredoc){
    char heredoc_line[MAX_LINE];
    size_t len;

    heredoc->lines = NULL;
    heredoc->size = 0;

    printf("HEREDOC> ");
    while (fgets(heredoc_line, sizeof(heredoc_line), stdin) != NULL) {
        if (strcmp(heredoc_line, "}\n") == 0) {
            break;
        }
        len = strlen(heredoc_line);
        heredoc->size += len;
        heredoc->lines = realloc(heredoc->lines, heredoc->size + 1);
        if (heredoc->lines == NULL) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        if (heredoc->size == len) {
            heredoc->lines[0] = '\0'; // Inicializar la primera vez
        }
        strcat(heredoc->lines, heredoc_line);
        printf("HEREDOC> ");
    }
    
}

// Create pipes to redirect de heredoc to the command
void
redirectheredoc(HereDoc *heredoc) {
    int pipefd[2];
    
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    if (write(pipefd[1], heredoc->lines, heredoc->size) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    close(pipefd[1]);
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);

    free(heredoc->lines);

}

int
icanacces(char *file) {
    return access(file, R_OK) != 0;
}



int
procbackground(char **tokens) {
    int i;

    for (i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "&") == 0) {
            return 1;
        }
    }

    return 0;
}

void erasetoken(char **tokens, char* token) {
    int i;
    int j;

    i = 0;
    while (tokens[i] != NULL) {
        if (strcmp(tokens[i], token) == 0) {
            free(tokens[i]);
            for (j = i; tokens[j] != NULL; j++) {
                tokens[j] = tokens[j + 1];
            }
            break;
        }
        i++;
    }
}

void
executecommand(LineToken *lt, int background) {
    Redirection *redir = malloc(sizeof(Redirection));
    HereDoc *heredoc = malloc(sizeof(HereDoc));
    char *commandpath;

    if (redir == NULL) {
        perror("malloc");
        freelinetoken(lt);
        free(lt);
        exit(EXIT_FAILURE);
    }

    initredirect(redir);

    manageredirections(lt->tokens, redir);

    if (redir->isinputredirect) {
        if (icanacces(redir->inputfile)) {
            perror("access");
            freeall(lt, redir, heredoc);
            exit(EXIT_FAILURE);
        }
        redirectinput(redir->inputfile);
    } else if (background) {
        redirectinput("/dev/null");
    
    }

    if (redir->isoutputredirect) {
        redirectoutput(redir->outputfile);
    }

    if (ishere(lt->tokens) && noredirects(redir)) {
        readheredoc(heredoc);
        redirectheredoc(heredoc);
        erasetoken(lt->tokens, "HERE{");
        
    }

    commandpath = buildpath(lt->tokens[0]);

    if (commandpath == NULL) {
        fprintf(stderr, "Command not found: %s\n", lt->tokens[0]);
        freeall(lt, redir, heredoc);
        exit(EXIT_FAILURE);
    }

    execv(commandpath, lt->tokens);

    perror("execv");

    free(commandpath);
    freeall(lt, redir, heredoc);
    exit(EXIT_FAILURE);
}


void
startprocess(LineToken *lt){
    int pidchild;
    int background;

    background = procbackground(lt->tokens);

    if (background) {
        erasetoken(lt->tokens, "&");
    }
    
    switch (pidchild = fork()) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);
        break;
    case 0:
        executecommand(lt, background);
        freelinetoken(lt);
        free(lt);
        exit(EXIT_FAILURE);
        break;
    }

    if (background) {
        printf("[%d]+ Start\n", pidchild);

    } else {
        waitchild(pidchild);
    }
}





// Main function
int
main(int argc, char* argv[]){
    LineToken *lt = malloc(sizeof(LineToken));
    int isterminal;
    int fail = 0;

    signal(SIGINT, siginthandler);

    if (lt == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    isterminal = itisterminal();

    initshell();
    
    initlinetoken(lt);

    do {

        checkbackgroundchilds();

        readline(&lt->line, isterminal);

        if (lt->line == NULL) {
            break;
        }

        if (lt->line[0] == '\n') {
            freelinetoken(lt);
            continue;
        }

        
        lt->tokens = tokenize(lt->line);
        if (lt->tokens == NULL) {
            freelinetoken(lt);
            exit(EXIT_FAILURE);
        }

        removequotes(lt->tokens);

        fail = replaceenvvars(lt->tokens);
        
        if (fail) {
            freelinetoken(lt);
            continue;
        }

        if (nolinetoken(lt)) {
            freelinetoken(lt);
            continue;
        }


        globbing(&lt->tokens);
        

        if (isifok(lt->tokens)) {
            // if the exit has been with failure skip the next command
            if (exitwitherror()) {
                printf("Skipping next command\n");
                freelinetoken(lt);
                continue;
            } else {
                erasetoken(lt->tokens, "ifok");
            }
        } else if (isifnot(lt->tokens)) {
            // if the exit has been with success skip the next command
            if (exitwithsuccess()) {
                printf("Skipping next command\n");
                freelinetoken(lt);
                continue;
            } else {
                erasetoken(lt->tokens, "ifnot");
            }
        }

        if (nolinetoken(lt)) {
            freelinetoken(lt);
            continue;
        }

        if (exitcommand(lt->tokens)) {
            printf("Ending shell\n");
            break;
        }

        if (isenvassignment(lt->tokens)) {
            handleenvassignment(lt->tokens);
            freelinetoken(lt);
            continue;
        }

        if (builtincd(lt->tokens)){
            changecwd(lt->tokens);
        } else {
            startprocess(lt);
        }
        
        /*
        // First if its cd make builtincd
        if (builtincd(lt->tokens)){
            changecwd(lt->tokens);
        } else if (isenvassignment(lt->tokens)) {
            handleenvassignment(lt->tokens);
        } else {
            startprocess(lt);
        }
        */

        freelinetoken(lt);
        

    } while (1);

    freelinetoken(lt);
    free(lt);

    exit(EXIT_SUCCESS);
}