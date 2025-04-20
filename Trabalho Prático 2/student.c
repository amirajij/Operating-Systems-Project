#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_MSG 1024

int main(int argc, char* argv[]) {

    if (argc != 4) {
        printf("Uso: %s <naluno> <aluno_inicial> <num_alunos>\n", argv[0]);
        return 1;
    }
    
    int nstud = atoi(argv[1]);
    int aluno_inicial = atoi(argv[2]);
    int num_alunos = atoi(argv[3]);
    
    // Cria named pipe para receber respostas
    char pipe_resposta[MAX_MSG];
    sprintf(pipe_resposta, "/tmp/student_%d", nstud);
    mkfifo(pipe_resposta, 0666);
    
    printf("student %d: aluno inicial=%d, número de alunos=%d\n", 
           nstud, aluno_inicial, num_alunos);
    
    // Prepara mensagem para o support_agent
    char msg[MAX_MSG];
    sprintf(msg, "%d %d %s", aluno_inicial, num_alunos, pipe_resposta);
    
    // Envia pedido
    int pipe_fd = open("/tmp/suporte", O_WRONLY);
    write(pipe_fd, msg, strlen(msg) + 1);
    close(pipe_fd);
    
    // Aguarda resposta
    pipe_fd = open(pipe_resposta, O_RDONLY);
    char mensagem_resposta[MAX_MSG];
    int posicao = 0;
    char caractere;
    
    while (read(pipe_fd, &caractere, 1) > 0 && caractere != '\0') {
        mensagem_resposta[posicao++] = caractere;
    }
    mensagem_resposta[posicao] = '\0';
    
    int alunos_inscritos = atoi(mensagem_resposta);
    printf("> student %d: alunos inscritos=%d\n", nstud, alunos_inscritos);
    
    // Limpa recursos
    close(pipe_fd);
    unlink(pipe_resposta);
    
    return 0;
}