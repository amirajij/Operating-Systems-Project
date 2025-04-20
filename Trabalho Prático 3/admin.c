#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define MAX_MSG 1024

// Envia o pedido para o named pipe
void enviar_pedido(const char* pedido, const char* admin_pipe) {
    int fd = open("/tmp/admin", O_WRONLY);
    if (fd == -1) {
        perror("Error ao abrir admin pipe");
        exit(1);
    }
    write(fd, pedido, strlen(pedido) + 1);
    close(fd);
}

// Recebe a resposta do named pipe, lê e fecga o pipe
void receber_resposta(const char* admin_pipe, char* resposta, int max_size) {
    int fd = open(admin_pipe, O_RDONLY);
    if (fd == -1) {
        perror("Erro ao abrir pipe de resposta");
        exit(1);
    }
    ssize_t bytes_lidos = read(fd, resposta, max_size - 1);
    if (bytes_lidos > 0) {
        resposta[bytes_lidos] = '\0';
    } else {
        strcpy(resposta, "Erro ao ler resposta");
    }
    close(fd);
}

// Solicita um número de aluno e envia um pedido para obter os horários
void pedir_horarios(int admin_id) {
    char admin_pipe[MAX_MSG];
    sprintf(admin_pipe, "/tmp/admin_%d", admin_id);
    
    int num_aluno;
    printf("Introduza o aluno: ");
    scanf("%d", &num_aluno);
    
    char pedido[MAX_MSG];
    sprintf(pedido, "1,%d,%s", num_aluno, admin_pipe);
    
    enviar_pedido(pedido, admin_pipe);
    
    char resposta[MAX_MSG];
    receber_resposta(admin_pipe, resposta, MAX_MSG);
    
    printf("Horário do aluno %d: %s\n", num_aluno, resposta);
}

// Pede o nome do ficheiro e envia um pedido para gravar os horários em ficheiro
void gravar_em_ficheiro(int admin_id) {
    char admin_pipe[MAX_MSG];
    sprintf(admin_pipe, "/tmp/admin_%d", admin_id);
    
    char filename[MAX_MSG];
    printf("Introduza o nome do ficheiro (ex: alunos.csv): ");
    scanf("%s", filename);
    
    char pedido[MAX_MSG];
    sprintf(pedido, "2,%s,%s", filename, admin_pipe);

    enviar_pedido(pedido, admin_pipe);

    char resposta[MAX_MSG];
    receber_resposta(admin_pipe, resposta, MAX_MSG);
    
    printf("Resposta recebida: %s\n", resposta);
    
    int num_alunos = atoi(resposta);
    if (num_alunos == -1) {
        printf("Erro ao escrever no ficheiro\n");
    } else {
        
        // Exibir conteúdo do arquivo
        FILE *file = fopen(filename, "r");
        if (file) {
            char line[MAX_MSG];
            printf("\nConteúdo do arquivo %s:\n", filename);
            while (fgets(line, sizeof(line), file)) {
                printf("%s", line);
            }
            fclose(file);
        } else {
            printf("Não foi possível abrir o arquivo para leitura.\n");
        }
    }
}

// Solicita informações sobre um aluno e envia um pedido para mudar o horário
void mudar_horario(int admin_id) {
    char admin_pipe[MAX_MSG];
    sprintf(admin_pipe, "/tmp/admin_%d", admin_id);
    
    int num_aluno, disciplina, novo_horario;
    printf("Introduza o número do aluno: ");
    scanf("%d", &num_aluno);
    printf("Introduza o número da disciplina: ");
    scanf("%d", &disciplina);
    printf("Introduza o novo horário: ");
    scanf("%d", &novo_horario);
    
    char pedido[MAX_MSG];
    sprintf(pedido, "4,%d,%d,%d,%s", num_aluno, disciplina, novo_horario, admin_pipe);
    
    enviar_pedido(pedido, admin_pipe);
    
    char resposta[MAX_MSG];
    receber_resposta(admin_pipe, resposta, MAX_MSG);
    
    printf("Resposta: %s\n", resposta);
}

// Solicita número da disciplina e envia um pedido para contar o número de alunos inscritos
void contar_alunos_disciplina(int admin_id) {
    char admin_pipe[MAX_MSG];
    sprintf(admin_pipe, "/tmp/admin_%d", admin_id);
    
    int disciplina;
    printf("Introduza o número da disciplina: ");
    scanf("%d", &disciplina);
    
    char pedido[MAX_MSG];
    sprintf(pedido, "5,%d,%s", disciplina, admin_pipe);
    
    enviar_pedido(pedido, admin_pipe);
    
    char resposta[MAX_MSG];
    receber_resposta(admin_pipe, resposta, MAX_MSG);
    
    printf("Resposta: %s\n", resposta);
}

// Envia um pedido para terminar o support_agent
void terminar(int admin_id) {
    char admin_pipe[MAX_MSG];
    sprintf(admin_pipe, "/tmp/admin_%d", admin_id);
    
    char pedido[MAX_MSG];
    sprintf(pedido, "3,%s", admin_pipe);
    
    enviar_pedido(pedido, admin_pipe);
    
    char resposta[MAX_MSG];
    receber_resposta(admin_pipe, resposta, MAX_MSG);
    
    printf("Resposta: %s\n", resposta);
}

// Cria o pipe e processa as escolhas do menu
int main() {
    int admin_id = getpid();
    char admin_pipe[MAX_MSG];
    sprintf(admin_pipe, "/tmp/admin_%d", admin_id);
    
    mkfifo(admin_pipe, 0666);
    
    int escolha;
    do {
        printf("\nAdmin Menu:\n");
        printf("1. Pedir horários\n");
        printf("2. Gravar em ficheiro\n");
        printf("3. Terminar support_agent\n");
        printf("4. Mudar horário de um aluno\n");
        printf("5. Contar alunos inscritos numa disciplina\n");
        printf("0. Sair\n");
        printf("Introduza a sua opção: ");
        scanf("%d", &escolha);
        
        switch(escolha) {
            case 1:
                pedir_horarios(admin_id);
                break;
            case 2:
                gravar_em_ficheiro(admin_id);
                break;
            case 3:
                terminar(admin_id);
                break;
            case 4:
                mudar_horario(admin_id);
                break;
            case 5:
            contar_alunos_disciplina(admin_id);
            break;
            case 0:
                printf("A sair...\n");
                break;
            default:
                printf("Escolha inválida, tente novamente.\n");
        }
    } while (escolha != 0);
    
    unlink(admin_pipe);
    return 0;
}