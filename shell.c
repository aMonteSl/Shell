#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

enum {
    MAX_LINE = 1024,
};

void init_shell() {
    char *home_dir = getenv("HOME");
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
    char line[MAX_LINE] = "";
    char *pline = NULL;

    // Imprimir el prompt como "user@:current_directory$ "
    char *user = getenv("USER");

    if (user == NULL) {
        fprintf(stderr, "Error: USER environment variable not set.\n");
        exit(EXIT_FAILURE);
    }

    char *current_dir = getcwd(NULL, 0);
    if (current_dir == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    printf("%s@:%s$ ", user, current_dir);
    free(current_dir);

    if (fgets(line, MAX_LINE, stdin) == NULL) {
        return NULL; // Manejo de error o EOF.
    }

    // // Decir que numero de linea se ha leido y lo que se ha leido
    // printf("Linea leida: %s\n",line);


    pline = strdup(line);

    return pline;
}

char **tokenize(char *line) {
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

    // No liberamos line aquí porque tokens apunta a partes de line
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

int procbackground(char **tokens) {
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

void waitchild(int pid) {
    int pidwait;
    int status;

    do {
        pidwait = waitpid(pid, &status, 0);
        if (pidwait == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                printf("El proceso hijo terminó con un código de salida distinto de 0: %d\n", WEXITSTATUS(status));
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
            // Desplazar los tokens hacia adelante para llenar el espacio vacío
            for (int j = i; tokens[j] != NULL; j++) {
                tokens[j] = tokens[j + 2];
            }
            i--; // Ajustar el índice para verificar el nuevo token en la misma posición
        } else if (strcmp(tokens[i], ">") == 0) {
            *output_redirect = 1;
            *output_file = tokens[i + 1];
            // Desplazar los tokens hacia adelante para llenar el espacio vacío
            for (int j = i; tokens[j] != NULL; j++) {
                tokens[j] = tokens[j + 2];
            }
            i--; // Ajustar el índice para verificar el nuevo token en la misma posición
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
    return strncmp(command, "./", 2) == 0 || strncmp(command, "/", 1) == 0;
}

char *find_command_in_path(char *command) {
    char *path = getenv("PATH");
    if (path == NULL) {
        return NULL;
    }

    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        perror("strdup");
        return NULL; // Retornar NULL sin detener el programa.
    }

    char *full_path = malloc(MAX_LINE);
    if (full_path == NULL) {
        perror("malloc");
        free(path_copy); // Liberar la copia antes de retornar.
        return NULL;
    }

    char *dir = strtok(path_copy, ":");
    while (dir != NULL) {
        snprintf(full_path, MAX_LINE, "%s/%s", dir, command);
        if (access(full_path, X_OK) == 0) {
            free(path_copy); // Liberar recursos antes de retornar.
            return full_path;
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    free(full_path);
    return NULL; // Retornar NULL si el comando no se encuentra.
}


int isenvassignment(const char *token) {
    return strchr(token, '=') != NULL;
}

int putenv(char *env) {
    char *env_assignment = strdup(env); // Duplicar la cadena para no modificar el original
    char *key = strtok(env_assignment, "=");
    char *value = strtok(NULL, "=");

    if (key && value) {
        setenv(key, value, 1); // 1 para sobrescribir si ya existe
    } else {
        fprintf(stderr, "Invalid environment variable assignment: %s\n", env);
    }
    free(env_assignment); // Liberar la memoria duplicada
    return 0; // No ejecutar más comandos
}

void handleassigmentenv(char **tokens) {
    if (isenvassignment(tokens[0])) {
        // printf("Setting environment variable: %s\n", tokens[0]);
        // En caso de que me hagan por ejemplo un MIESCRITORIO=$HOME/Escritorio
        // Tendre que sustituir antes los valores de las variables de entorno en caso de que las haya
        putenv(tokens[0]);
    }
}

void
replaceenvvars(char **tokens) {
    for (int i = 0; tokens[i] != NULL; i++) {
        if (tokens[i][0] == '$') {
            char *env_value = getenv(tokens[i] + 1);
            if (env_value != NULL) {
                tokens[i] = env_value;
            } else {
                // Si no tiene valor, el token se reemplaza por una cadena vacía
                tokens[i] = "";
            }
        }
    }
}

void
executecommand(char **tokens, int background, char *line) {
    int input_redirect = 0;
    int output_redirect = 0;
    char *input_file = NULL;
    char *output_file = NULL;

    handle_redirection(tokens, &input_redirect, &input_file, &output_redirect, &output_file);

    // Reemplazar las variables de entorno en los tokens
    // replaceenvvars(tokens);

    if (input_redirect) {
        redirectinput(input_file);
    } else if (background) {
        redirectinput("/dev/null");
    }

    if (output_redirect) {
        redirectoutput(output_file);
    }

    if (islocalcommand(tokens[0])) {
        execv(tokens[0], tokens);
    } else {
        char *command_path = find_command_in_path(tokens[0]);
        if (command_path != NULL) {
            execv(command_path, tokens);
            free(command_path);
        } else {
            fprintf(stderr, "Command not found: %s\n", tokens[0]);
            free(line);
            free(tokens);
            exit(EXIT_FAILURE);
        }
    }
    perror("execv");

    // Liberar la memoria duplicada
    // for (int i = 0; tokens[i] != NULL; i++) {
    //     if (tokens[i][0] == '$') {
    //         free(tokens[i]);
    //     }
    // }

    free(line); // Liberar memoria antes de salir
    free(tokens); // Liberar memoria antes de salir
    exit(EXIT_FAILURE);
}

void execute(char **tokens, char *line) {
    int pidchild;
    int background = procbackground(tokens);

    switch (pidchild = fork()) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);
        break;
    case 0:
        executecommand(tokens, background, line);
        exit(EXIT_FAILURE);
        break;
    }

    if (background) {
        printf("[Proceso en segundo plano iniciado con PID %d]\n", pidchild);
    } else {
        waitchild(pidchild);
    }
}

void checkbackgroundchilds() {
    int status;
    int pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[Proceso en segundo plano con PID %d terminado]\n", pid);
    }
}


int main(int argc, char *argv[]) {
    char **tokens = NULL;
    char *line = NULL;

    printf("Shell iniciada\n");
    init_shell();
    // Tengo que ver cual es la entrada estandar de mi programa, por si me han hecho cat miarchivo.txt | ./shell o ./shell < miarchivo.txt
    // Si es asi, debo de no imprimir el prompt
    // No puedo usar la libreria isatty porque mis profesores me han dicho que no la use
    

    while (1) {

        checkbackgroundchilds(); // Verificar procesos en segundo plano
        line = read_line();
        if (line == NULL) {
            free(line);
            free(tokens);
            break;
        } else if (line[0] == '\n') {
            free(line);
            continue;
        }

        
        tokens = tokenize(line);
        replaceenvvars(tokens);

        if (strcmp(tokens[0], "exit") == 0) {
            free(line);
            free(tokens);
            break;
        }

        if (builtincd(tokens)) {
            execute_cd(tokens);
        } else if (isenvassignment(tokens[0])) {
            handleassigmentenv(tokens);
        } else {
            execute(tokens, line);
        }
        

        free(line);
        free(tokens);
        line = NULL;
        tokens = NULL;
        
    }

    printf("Shell finalizada\n");
    

    exit(EXIT_SUCCESS);
}