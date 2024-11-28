#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

enum {
    MAX_LINE = 1024,
};

void init_shell() {
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set.\n");
        exit(EXIT_FAILURE);
    }
    if (chdir(home_dir) != 0) {
        perror("cd");
        exit(EXIT_FAILURE);
    }
}

char *read_line() {
    char *line = malloc(MAX_LINE);
    do {
        printf("%s, %i> ", getcwd(NULL, 0), getpid());
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
            // Eliminar los tokens de redirección
            int j = i;
            while (tokens[j + 2] != NULL) {
                tokens[j] = tokens[j + 2];
                j++;
            }
            tokens[j] = NULL;
            i--;
        } else if (strcmp(tokens[i], ">") == 0) {
            *output_redirect = 1;
            *output_file = tokens[i + 1];
            // Eliminar los tokens de redirección
            int j = i;
            while (tokens[j + 2] != NULL) {
                tokens[j] = tokens[j + 2];
                j++;
            }
            tokens[j] = NULL;
            i--;
        } else if (strcmp(tokens[i], "&") == 0) {
            *background = 1;
            // Eliminar el token de ejecución en segundo plano
            int j = i;
            while (tokens[j] != NULL) {
                tokens[j] = tokens[j + 1];
                j++;
            }
            i--;
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
    int status;
    int input_redirect = 0;
    int output_redirect = 0;
    int background = 0;
    char *input_file = NULL;
    char *output_file = NULL;

    handle_redirection(tokens, &input_redirect, &input_file, &output_redirect, &output_file, &background);

    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        execute_command(tokens, input_redirect, input_file, output_redirect, output_file, background);
    } else {
        if (!background) {
            if (waitpid(pid, &status, 0) == -1) {
                perror("wait");
                exit(EXIT_FAILURE);
            }
        } else {
            printf("[Proceso en segundo plano iniciado con PID %d]\n", pid);
        }
    }
}

void check_background_processes() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[Proceso en segundo plano con PID %d terminado]\n", pid);
    }
}

int main(int argc, char *argv[]) {
    char *line;
    char **tokens;

    init_shell();
    printf("Shell iniciada\n");

    while (1) {
        check_background_processes();

        line = read_line();
        tokens = tokenize(line);

        if (builtincd(tokens)) {
            execute_cd(tokens);
        } else {
            execute(tokens);
        }

        if (strcmp(tokens[0], "exit") == 0) {
            break;
        }

        free(line);
        free(tokens);
    }

    exit(EXIT_SUCCESS);
}
