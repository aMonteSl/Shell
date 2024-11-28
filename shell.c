#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

enum {
    MAX_LINE = 1024,
};

// Función para iniciar la shell
void init_shell() {
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set.\n");
        exit(EXIT_FAILURE);
    }

    // Cambiar al directorio $HOME
    if (chdir(home_dir) != 0) {
        perror("cd");
        exit(EXIT_FAILURE);
    }
}

// Función que lee una línea de la entrada estándar
char *read_line() {
    char *line = malloc(MAX_LINE);
    
    do {
        printf("%s> ", getcwd(NULL, 0));
        fgets(line, MAX_LINE, stdin);
    } while (line[0] == '\n');

    return line;
}

char **tokenize(char *line) {
    char **tokens = malloc(MAX_LINE * sizeof(char *));
    char *token;
    char *saveptr;
    int i = 0;

    token = strtok_r(line, " \n", &saveptr);
    while (token != NULL) {
        tokens[i] = token;
        i++;
        token = strtok_r(NULL, " \n", &saveptr);
    }
    tokens[i] = NULL;

    return tokens;
}

// Función que comprueba si el comando es el builtin cd
int builtincd(char **tokens) {
    return strcmp(tokens[0], "cd") == 0;
}

void execute_cd(char **tokens) {
    if (tokens[1] == NULL) {
        init_shell();
    } else {
        if (chdir(tokens[1]) != 0) {
            perror("cd");
        }
    }
}

// Función que busca el comando en las rutas de PATH
char *find_command_in_path(char *command) {
    char *path = getenv("PATH");
    if (path == NULL) {
        return NULL;
    }

    char *path_copy = strdup(path);
    char *dir = strtok(path_copy, ":");
    char *full_path = malloc(MAX_LINE);

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
    return NULL;
}

void handle_redirection(char **tokens, int *input_redirect, char **input_file, int *output_redirect, char **output_file, int *background) {
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            *input_redirect = 1;
            *input_file = tokens[i + 1];
            tokens[i] = NULL; // Remove the redirection part from tokens
        } else if (strcmp(tokens[i], ">") == 0) {
            *output_redirect = 1;
            *output_file = tokens[i + 1];
            tokens[i] = NULL; // Remove the redirection part from tokens
        } else if (strcmp(tokens[i], "&") == 0) {
            *background = 1;
            tokens[i] = NULL; // Remove the background part from tokens
        }
    }
}

void execute_command(char **tokens, int input_redirect, char *input_file, int output_redirect, char *output_file, int background) {
    if (input_redirect) {
        int fd = open(input_file, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    } else if (background) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (output_redirect) {
        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (strncmp(tokens[0], "./", 2) == 0 || strncmp(tokens[0], "/", 1) == 0) {
        execv(tokens[0], tokens);
    } else {
        char *command_path = find_command_in_path(tokens[0]);
        if (command_path != NULL) {
            execv(command_path, tokens);
            free(command_path);
        } else {
            fprintf(stderr, "Command not found: %s\n", tokens[0]);
        }
    }
    perror("execv");
    exit(EXIT_FAILURE);
}

void execute(char **tokens) {
    int pid;
    int pid_wait;
    int status;
    int input_redirect = 0;
    int output_redirect = 0;
    int background = 0;
    char *input_file = NULL;
    char *output_file = NULL;

    handle_redirection(tokens, &input_redirect, &input_file, &output_redirect, &output_file, &background);

    pid = fork();
    switch (pid) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);
        break;
    case 0: 
        // Proceso hijo
        execute_command(tokens, input_redirect, input_file, output_redirect, output_file, background);
        break;
    default:
        // Proceso padre
        if (!background) {
            pid_wait = wait(&status);
            if (pid_wait == -1) {
                perror("wait");
                exit(EXIT_FAILURE);
            }
        }
        break;
    }
}

void check_background_processes() {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
        // Proceso hijo terminado
    }
}

int main(int argc, char *argv[]) {
    char *line;
    char **tokens;

    // Comando para iniciar la shell (básicamente cambiar al directorio $HOME)
    init_shell();
    printf("Shell iniciada\n");

    while (1) {
        // Verificar si hay procesos en segundo plano que hayan terminado
        check_background_processes();

        // Llamar a función que devolverá el prompt que se ha leído de la entrada estándar
        line = read_line();
        printf("line: %s", line);
        // Ahora toca tokenizar la línea leída
        tokens = tokenize(line);
        for (int i = 0; tokens[i] != NULL; i++) {
            printf("token[%d]: %s\n", i, tokens[i]);
        }
        // Ver si el comando es el builtin cd
        if (builtincd(tokens)) {
            // Llamar a la función que se encargará de cambiar el directorio de trabajo
            execute_cd(tokens);
        } else {
            // Ejecutar el comando
            execute(tokens);
        }

        free(line);
        free(tokens);
        tokens = NULL;
        line = NULL;
    }

    return 0;
}