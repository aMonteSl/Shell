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

// Signal handler for SIGINT
void 
sigint_handler(int sig) {
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

// Expand globbing in the tokens
void expandglobbing(char **tokens) {
    tokens = tokens;
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
            setenv("result", result_str, 1); // Actualizar la variable de entorno result
            if (exit_status != 0) {
                printf("El proceso hijo terminó con un código de salida distinto de 0: %d\n", exit_status);
            }
        } else if (WIFSIGNALED(status)) {
            printf("El proceso hijo fue terminado por una señal: %d\n", WTERMSIG(status));
        }
    } while (pidwait != pid);
}

void handle_redirection(char **tokens, int *input_redirect, char **input_file, int *output_redirect, char **output_file) {
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            *input_redirect = 1;
            *input_file = tokens[i + 1];
            for (int j = i; tokens[j] != NULL; j++) {
                tokens[j] = tokens[j + 2];
            }
            i--;
        } else if (strcmp(tokens[i], ">") == 0) {
            *output_redirect = 1;
            *output_file = tokens[i + 1];
            for (int j = i; tokens[j] != NULL; j++) {
                tokens[j] = tokens[j + 2];
            }
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

char *find_command_in_path(char *command) {
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
executecommand(LineToken *lt, int background) {
    int input_redirect = 0;
    int output_redirect = 0;
    char *input_file = NULL;
    char *output_file = NULL;

    handle_redirection(lt->tokens, &input_redirect, &input_file, &output_redirect, &output_file);

    if (input_redirect) {
        redirectinput(input_file);
    } else if (background) {
        redirectinput("/dev/null");
    }

    if (output_redirect) {
        redirectoutput(output_file);
    }

    if (islocalcommand(lt->tokens[0])) {
        execv(lt->tokens[0], lt->tokens);
    } else {
        char *command_path = find_command_in_path(lt->tokens[0]);
        if (command_path != NULL) {
            execv(command_path, lt->tokens);
            free(command_path);

        } else {
            fprintf(stderr, "Command not found: %s\n", lt->tokens[0]);
            freelinetoken(lt);
            free(lt);
            exit(EXIT_FAILURE);
        }
    }
    perror("execv");



    freelinetoken(lt);
    free(lt); // Liberar la memoria asignada para lt
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

// Main function
int main(int argc, char* argv[]){
    LineToken *lt = malloc(sizeof(LineToken));

    int isterminal = idstdinterminal();

    initshell();
    signal(SIGINT, sigint_handler);

    if (lt == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    lt->line = NULL;
    lt->tokens = NULL;

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
        expandglobbing(lt->tokens);

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