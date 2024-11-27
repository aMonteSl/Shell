
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    MAX_LINE = 1024,
};

/*
Tengo que hacer un prototipo de shell, para empezar quiero leer con fgets de la entrada estandar y luego tokenizar con strtok_r
*/

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

// Funcion que comprueba si el comando es el builtin cd
int builtincd(char **tokens) {
    if (strcmp(tokens[0], "cd") == 0) {
        return 1;
    }
    return 0;
}

void execute_cd(char **tokens) {
    if (tokens[1] == NULL) {
        if (chdir(getenv("HOME")) != 0) {
            perror("cd");
        }
    } else {
        if (chdir(tokens[1]) != 0) {
            perror("cd");
        }
    }
}

int main(int argc, char *argv[]) {
    char *line;
    char **tokens;

    while (1) {
        // Llamar a funcion que devolvera el prompt que se ha leido de la entrada estandar
        line = read_line();
        printf("line: %s", line);
        // Ahora toca tokenizar la linea leida
        tokens = tokenize(line);
        for (int i = 0; tokens[i] != NULL; i++) {
            printf("token[%d]: %s\n", i, tokens[i]);
        }
        // Ver si el comando es el builtin cd
        if (builtincd(tokens)) {
            // Llamar a la funcion que se encargara de cambiar el directorio de trabajo
            execute_cd(tokens);
            // Imprimir el directorio de trabajo
            char *cwd = getcwd(NULL, 0);
            if (cwd != NULL) {
                printf("Directorio de trabajo: %s\n", cwd);
                free(cwd);
            } else {
                perror("getcwd");
            }
        }

        free(line);
        free(tokens);
        line = NULL;
    }

    exit(EXIT_SUCCESS);
}