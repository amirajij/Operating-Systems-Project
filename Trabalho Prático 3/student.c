#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define MAX_MSG 1024
#define NUM_DISCIPLINAS 10
#define DISCIPLINAS_POR_STUDENT 5

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        printf("Usage: %s <nstud> <aluno_inicial> <num_alunos> [pipe_name]\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    int nstud = atoi(argv[1]);
    int aluno_inicial = atoi(argv[2]);
    int num_alunos = atoi(argv[3]);

    char pipe_suporte[MAX_MSG];
    strcpy(pipe_suporte, argc == 5 ? argv[4] : "/tmp/suporte");

    printf("student %d: aluno inicial=%d, número de alunos=%d\n", nstud, aluno_inicial, num_alunos);

    char pipe_resposta[MAX_MSG];
    sprintf(pipe_resposta, "/tmp/student_%d", nstud);

    if (unlink(pipe_resposta) == -1 && errno != ENOENT) {
    perror("Aviso: Não foi possível remover o pipe existente");
    }

    if (mkfifo(pipe_resposta, 0666) == -1) {
    perror("Erro ao criar pipe de resposta");
    return 1;
    }

    int pipe_fd_envio = open(pipe_suporte, O_WRONLY);
    if (pipe_fd_envio == -1) {
        perror("Erro ao abrir pipe de envio");
        unlink(pipe_resposta);
        return 1;
    }

    // Processo para cada student
    for (int i = 0; i < num_alunos; i++) {
        int aluno = aluno_inicial + i;
        int disciplinas[DISCIPLINAS_POR_STUDENT];
        int horarios[DISCIPLINAS_POR_STUDENT];
        
        // Inscreve em 5 disciplinas aleatórias
        for (int j = 0; j < DISCIPLINAS_POR_STUDENT; j++) {
            int disciplina;
            int unique;
            do {
                unique = 1;
                disciplina = rand() % NUM_DISCIPLINAS;
                for (int k = 0; k < j; k++) {
                    if (disciplinas[k] == disciplina) {
                        unique = 0;
                        break;
                    }
                }
            } while (!unique);
            
            disciplinas[j] = disciplina;
            
            char msg[MAX_MSG];
            sprintf(msg, "%d %d %s", aluno, disciplina, pipe_resposta);
            write(pipe_fd_envio, msg, strlen(msg) + 1);
            
            int pipe_fd_recepcao = open(pipe_resposta, O_RDONLY | O_NONBLOCK);
            if (pipe_fd_recepcao == -1) {
                perror("Erro ao abrir pipe de recepção");
                close(pipe_fd_envio);
                unlink(pipe_resposta);
                return 1;
            }

            char resposta[MAX_MSG];
            int posicao = 0;
            char caractere;
            int retries = 0;
            while (retries < 1000) {
                ssize_t bytes_read = read(pipe_fd_recepcao, &caractere, 1);
                if (bytes_read > 0) {
                    if (caractere == '\0') break;
                    resposta[posicao++] = caractere;
                } else if (bytes_read == 0 || (bytes_read == -1 && errno == EAGAIN)) {
                    usleep(1000);  // Espera 10ms para tentar ler novamente
                    retries++;
                } else {
                    perror("Erro ao ler do pipe");
                    break;
                }
            }
            resposta[posicao] = '\0';

            close(pipe_fd_recepcao);
            
            horarios[j] = atoi(resposta);
        }
        
        printf("student %d, aluno %d: ", nstud, aluno);
        for (int j = 0; j < DISCIPLINAS_POR_STUDENT; j++) {
            printf("%d/%d%s", disciplinas[j], horarios[j], j < DISCIPLINAS_POR_STUDENT - 1 ? ", " : "\n");
        }
    }
    
    close(pipe_fd_envio);
    unlink(pipe_resposta);
    
    return 0;
}