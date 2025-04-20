#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_SIZE 80

int main(int argc, char *argv[]) {
    
    // Verifica se os argumentos foram passados
    if (argc != 3) {
        printf("Uso: %s <nome_do_pipe> <pedido>\n", argv[0]);
        return 1;
    }

    const char *nome_pipe = argv[1];
    const char *pedido = argv[2];
    
    // Prepara o buffer com o pedido e o newline
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%s\n", pedido); // Cria uma string formatada com o pedido com \n
    
    // Abre o named pipe para escrever
    int fd = open(nome_pipe, O_WRONLY);
    if (fd == -1) {
        perror("Falha ao abrir o pipe");
        return 1;
    }

    // Escreve o pedido no pipe
    ssize_t escritaFormatada = write(fd, buffer, strlen(buffer)); // Escreve no buffer
    if (escritaFormatada == -1) {
        perror("Falha ao escrever no pipe");
        close(fd);
        return 1;
    }

    printf("Pedido '%s' enviado para o pipe '%s'\n", pedido, nome_pipe);

    // Fecha o pipe
    close(fd);

    return 0;
}