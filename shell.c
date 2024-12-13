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
};

// Define struct for the LineToken
typedef struct LineToken {
    char *line;
    char **tokens;
} LineToken;

// Define struct for the Redirection
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


// Verify if the standard input is a terminal
int
idstdinterminal(void){
    struct stat statbuf;
    fstat(STDIN_FILENO, &statbuf);
    return S_ISCHR(statbuf.st_mode);

}

// Initialize the shell
void
initshell(void){
    printf("Shell 1.0\n");

    // Change to the HOME directory
    char *homedir = getenv("HOME");
    if (homedir == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set. Using current directory.\n");
        return;
    }
    if (chdir(homedir) != 0) {
        fprintf(stderr, "Error: Failed to change directory to HOME (%s): %s\n", homedir, strerror(errno));
        fprintf(stderr, "Using current directory as fallback.\n");
    }

    // Initialize the RESULT environment variable
     if (setenv("result", "0", 1) != 0) {
        perror("setenv");
     } 
}

// Init LineToken
void
initlinetoken(LineToken *lt) {
    lt->line = NULL;
    lt->tokens = NULL;
}

// Signal handler for SIGINT
void 
siginthandler(int sig) {
    printf("\nCaught signal %d (SIGINT). Type 'exit' to quit the shell.\n", sig);
}

// Print the prompt
void
printpromt(void){
    char *user = getenv("USER");
    char *current_dir = getcwd(NULL, 0);

    if (user == NULL) {
        fprintf(stderr, "Error: USER environment variable not set.\n");
        exit(EXIT_FAILURE);
    }

    if (current_dir == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    printf("%s@:%s$ ", user, current_dir);
    free(current_dir);
}

// Check for background child processes
void
checkbackgroundchilds() {
    int status;
    int pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[Proceso en segundo plano con PID %d terminado]\n", pid);
    }
}


// Read a line from the standard input
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

// Tokenize a line
char **
tokenize(char *line) {
    char **tokens = malloc(MAX_LINE * sizeof(char *));
    char *token;
    char *saveptr;
    int i = 0;

    if (tokens == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if (line == NULL) {
        free(tokens);
        return NULL;
    }

    token = strtok_r(line, " \t\n", &saveptr);
    while (token != NULL) {
        tokens[i] = token;
        i++;
        token = strtok_r(NULL, " \t\n", &saveptr);
    }
    tokens[i] = NULL;

    return tokens;
}

// Free memory allocated for LineToken
void
freelinetoken(LineToken *lt) {
    if (lt->line != NULL) {
        free(lt->line);
        lt->line = NULL;
    }
    if (lt->tokens != NULL) {
        free(lt->tokens);
        lt->tokens = NULL;
    }
}

// Remove double quotes from the tokens
void
removequotes(char **tokens) {
    int i = 0;
    while (tokens[i] != NULL) {
        int len = strlen(tokens[i]);
        if (tokens[i][0] == '"') {
            // Move the string one position to the left
            for (int j = 0; j < len; j++) {
                tokens[i][j] = tokens[i][j + 1];
            }
            len--; // Reduce the length of the string
        }
        if (tokens[i][len - 1] == '"') {
            tokens[i][len - 1] = '\0'; // Remove the last character
        }
        i++;
    }
}

// Replace environment variables in the tokens
int
replaceenvvars(char **tokens) {
    for (int i = 0; tokens[i] != NULL; i++) {
        if (tokens[i][0] == '$') {
            char *env_value = getenv(tokens[i] + 1);
            if (env_value != NULL) {
                tokens[i] = env_value;
            } else {
                // Print error message and return failure
                fprintf(stderr, "error: var %s does not exist\n", tokens[i] + 1);
                return 0;
            }
        }
    }
    return 1;
}

// Expand globbing
void globbing(char ***tokens) {
    glob_t globbuf;
    int flags = 0;
    char **new_tokens = NULL;
    size_t new_tokens_count = 0;

    for (int i = 0; (*tokens)[i] != NULL; i++) {
        if (i == 0) {
            flags = GLOB_NOCHECK;
        } else {
            flags |= GLOB_APPEND;
        }
        glob((*tokens)[i], flags, NULL, &globbuf);
    }

    for (int i = 0; i < globbuf.gl_pathc; i++) {
        new_tokens_count++;
    }

    new_tokens = malloc((new_tokens_count + 1) * sizeof(char *));
    if (new_tokens == NULL) {
        perror("malloc");
        globfree(&globbuf);
        return;
    }

    for (int i = 0; i < globbuf.gl_pathc; i++) {
        new_tokens[i] = strdup(globbuf.gl_pathv[i]);
        if (new_tokens[i] == NULL) {
            perror("strdup");
            for (int j = 0; j < i; j++) {
                free(new_tokens[j]);
            }
            free(new_tokens);
            globfree(&globbuf);
            return;
        }
    }

    new_tokens[new_tokens_count] = NULL;

    globfree(&globbuf);

    // Free the old tokens
    for (int i = 0; (*tokens)[i] != NULL; i++) {
        free((*tokens)[i]);
    }
    free(*tokens);

    // Replace the old tokens with the new tokens
    *tokens = new_tokens;
}

// Exit command
int
exitcommand(char **tokens) {
    if (strcmp(tokens[0], "exit") == 0) {
        return 1;
    }
    return 0;
}

// If the command is "cd"
int
builtincd(char **tokens) {
    return strcmp(tokens[0], "cd") == 0;
}

// Execute the "cd" command
void 
changecwd(char **tokens) {
    if (tokens[1] == NULL) {
        // If no arguments move to the HOME directory
        char *homedir = getenv("HOME");
        if (homedir == NULL) {
            fprintf(stderr, "Error: HOME environment variable not set.\n");
        } else {
            if (chdir(homedir) != 0) {
                perror("cd");
            }
        }
    } else {
        if (chdir(tokens[1]) != 0) {
            perror("cd");
        }
    }
}

// If is an environment variable assignment
int
isenvassignment(char **tokens) {
    return strchr(tokens[0], '=') != NULL;
}

// Handle environment variable assignment
void
handleenvassignment(char **tokens) {
    char *env_assignment = strdup(tokens[0]); // Duplicate the string to not modify the original
    char *key = strtok(env_assignment, "=");
    char *value = strtok(NULL, "=");

    if (key && value) {
        setenv(key, value, 1); // 1 to overwrite if it already exists
    } else {
        fprintf(stderr, "Invalid environment variable assignment: %s\n", tokens[0]);
    }
    free(env_assignment); // Free the duplicated memory
}

// Check if its a process in the background
int 
procbackground(char **tokens) {
    // Encuentra el último token.
    char *last_token = tokens[0];
    int i;
    for (i = 1; tokens[i] != NULL; i++) {
        last_token = tokens[i];
    }

    // Si el último token es "&", se ejecutará en segundo plano.
    if (strcmp(last_token, "&") == 0) {
        tokens[i - 1] = NULL; // Eliminar el "&" de los tokens.
        return 1; // Indica que es un proceso en segundo plano.
    }
    return 0; // Indica que no es un proceso en segundo plano.
}

// Wait for a child process
void
waitchild(int pid) {
    int pidwait;
    int status;

    do {
        pidwait = waitpid(pid, &status, 0);
        if (pidwait == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            char result_str[10];
            snprintf(result_str, sizeof(result_str), "%d", exit_status);
            setenv("result", result_str, 1);
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
    *file = token;
}

// Remove redirection tokens from the tokens array
void removeredirectiontokens(char **tokens, int i) {
    for (int j = i; tokens[j] != NULL; j++) {
        tokens[j] = tokens[j + 2];
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
    int fd = open(input_file, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
}

void
redirectoutput(char *output_file) {
    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

char*
findcommandinpath(char *command) {
    char *path = getenv("PATH");
    if (path == NULL) {
        return NULL;
    }

    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        perror("strdup");
        return NULL;
    }

    char *full_path = malloc(MAX_LINE);
    if (full_path == NULL) {
        perror("malloc");
        free(path_copy);
        return NULL;
    }

    char *dir = strtok(path_copy, ":");
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

void
initredirect(Redirection *redir) {
    redir->isinputredirect = 0;
    redir->isoutputredirect = 0;
    redir->inputfile = NULL;
    redir->outputfile = NULL;
}

// Remove "./" from the command of the first token
void
removedotslash(char *token) {
    int len = strlen(token);
    for (int i = 0; i < len - 1; i++) {
        token[i] = token[i + 2];
    }
    token[len - 2] = '\0';
}

void
freeall(LineToken *lt, Redirection *redir, HereDoc *heredoc) {
    freelinetoken(lt);
    free(lt);
    free(redir);
    free(heredoc);
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
    heredoc->lines = NULL;
    heredoc->size = 0;

    char heredoc_line[MAX_LINE];
    size_t len;

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
    // Is not neccesary to create a new child process
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

void erasetoken(char **tokens, char* token) {
    int i = 0;
    while (tokens[i] != NULL) {
        if (strcmp(tokens[i], token) == 0) {
            // Move tokens to the left
            for (int j = i; tokens[j] != NULL; j++) {
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
        redirectinput(redir->inputfile);
    } else if (background) {
        // printf("Redirigiendo la entrada a /dev/null\n");
        redirectinput("/dev/null");
    
    }

    if (redir->isoutputredirect) {
        redirectoutput(redir->outputfile);
    }

    if (ishere(lt->tokens) && noredirects(redir)) {
        // printf("SERA HERE\n");
        // Primero leer las líneas de HERE hasta que encontremos un }
        readheredoc(heredoc);
        // Create pipe to redirect the heredoc to the command
        redirectheredoc(heredoc);
        // Remove the HERE command from the tokens
        erasetoken(lt->tokens, "HERE{");
        
    }

    if (islocalcommand(lt->tokens[0])) {
        // printf("Is a local command\n");
        commandpath = strdup(lt->tokens[0]);
        if (commandpath == NULL) {
            perror("strdup");
            freeall(lt, redir, heredoc);
            exit(EXIT_FAILURE);
        }
        removedotslash(lt->tokens[0]);
    } else {
        commandpath = findcommandinpath(lt->tokens[0]);
        // printf("Command path: %s\n", commandpath);
        if (commandpath == NULL) {
            fprintf(stderr, "Command not found: %s\n", lt->tokens[0]);
            freeall(lt, redir, heredoc);
            exit(EXIT_FAILURE);
        }
    }

    execv(commandpath, lt->tokens);

    perror("execv");

    free(commandpath);
    freeall(lt, redir, heredoc);
    exit(EXIT_FAILURE);
}

// Start a process
void
startprocess(LineToken *lt){
    int pidchild;
    int background = procbackground(lt->tokens);

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
        printf("[Proceso en segundo plano iniciado con PID %d]\n", pidchild);
        // Agregar más detalles si es necesario
    } else {
        waitchild(pidchild);
    }
}

int
isifok(char **tokens) {
    return strcmp(tokens[0], "ifok") == 0;
}

int
exitwitherror(char **tokens) {
    int correct = atoi(getenv("result"));
    if (correct != 0) {
        return 1;
    }
    return 0;
}

int
isifnot(char **tokens) {
    return strcmp(tokens[0], "ifnot") == 0;
}

int
exitwithsuccess(char **tokens) {
    int correct = atoi(getenv("result"));
    if (correct == 0) {
        return 1;
    }
    return 0;
}



// Main function
int
main(int argc, char* argv[]){
    LineToken *lt = malloc(sizeof(LineToken));

    int isterminal = idstdinterminal();

    initshell();
    signal(SIGINT, siginthandler);

    if (lt == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    initlinetoken(lt);

    do {

        checkbackgroundchilds();

        readline(&lt->line, isterminal);
        // printf("Linea leida: %s", lt->line);

        if (lt->line == NULL) {
            break;
        } else if (lt->line[0] == '\n') {
            freelinetoken(lt);
            continue;
        }

        lt->tokens = tokenize(lt->line);
        if (lt->tokens == NULL) {
            freelinetoken(lt);
            exit(EXIT_FAILURE);
        }

        // Remove double quotes from the tokens
        removequotes(lt->tokens);

        // Replace environment variables and check for errors
        if (!replaceenvvars(lt->tokens)) {
            freelinetoken(lt);
            continue; // Skip execution if there is an error
        }

        if (lt->tokens[0] == NULL || lt->tokens == NULL) {
            freelinetoken(lt);
            continue;
        }

        // Expand globbing HAY QUE HACER ESTO AL FINAL
        globbing(&lt->tokens);

        // Print tokens
        // for (int i = 0; lt->tokens[i] != NULL; i++) {
        //     printf("Token %d: %s\n", i, lt->tokens[i]);
        // }

        // If the command is "exit", exit the shell
        if (exitcommand(lt->tokens)) {
            break;
        }

        // Print memory address of tokens and line
        // printf("Memory address of tokens: %p\n", lt->tokens);
        // printf("Memory address of line: %p\n", lt->line);

        // Place for the execution of the command

        
        if (isifok(lt->tokens)) {
            // if the exit has been with failure skip the next command
            if (exitwitherror(&lt->tokens[1])) {
                printf("Skipping next command\n");
                freelinetoken(lt);
                continue;
            } else {
                erasetoken(lt->tokens, "ifok");
            }
        } else if (isifnot(lt->tokens)) {
            // if the exit has been with success skip the next command
            if (exitwithsuccess(&lt->tokens[1])) {
                printf("Skipping next command\n");
                freelinetoken(lt);
                continue;
            } else {
                erasetoken(lt->tokens, "ifnot");
            }
        }

        for (int i = 0; lt->tokens[i] != NULL; i++) {
            printf("Token %d: %s\n", i, lt->tokens[i]);
        }
        
        
        // First if its cd make builtincd
        if (builtincd(lt->tokens)){
            changecwd(lt->tokens);
        } else if (isenvassignment(lt->tokens)) {
            handleenvassignment(lt->tokens);
        } else {
            startprocess(lt);
        }

        freelinetoken(lt);

    } while (1);

    freelinetoken(lt);
    free(lt); // Libera la memoria asignada para lt

    exit(EXIT_SUCCESS);
}