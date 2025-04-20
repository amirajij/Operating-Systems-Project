#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define MAX_MSG 1024
#define NUM_THREADS 4
#define MAX_DISCIPLINAS 10

volatile int deve_terminar = 0;

typedef struct AlunoNode {
    int aluno_id;
    struct AlunoNode* prev;
    struct AlunoNode* next;
} AlunoNode;

typedef struct {
    int total_vagas;
    int vagas_disponiveis;
    AlunoNode* primeiro_aluno;
    AlunoNode* ultimo_aluno;
} Horario;

typedef struct {
    int disciplina;
    int aluno;
    char pipe_resposta[MAX_MSG];
} PedidoMatricula;

typedef struct {
    Horario** horarios;
    int num_disciplinas;
    int num_horarios;
    int num_alunos;
    int vagas_por_horario;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_rwlock_t rwlock;
} DadosSistema;

DadosSistema dados_sistema;

// Constroi uma string com os horários em que um aluno está matriculado
void pedir_horarios(int aluno, char* pipe_resposta) {
    char resposta[MAX_MSG] = {0};
    char temp[32];
    sprintf(resposta, "%d", aluno);

    pthread_rwlock_rdlock(&dados_sistema.rwlock);
    for (int d = 0; d < dados_sistema.num_disciplinas; d++) {
        for (int h = 0; h < dados_sistema.num_horarios; h++) {
            AlunoNode* node = dados_sistema.horarios[d][h].primeiro_aluno;
            while (node) {
                if (node->aluno_id == aluno) {
                    sprintf(temp, ",%d/%d", d, h);
                    strcat(resposta, temp);
                    break;
                }
                node = node->next;
            }
        }
    }
    pthread_rwlock_unlock(&dados_sistema.rwlock);

    int fd_resposta = open(pipe_resposta, O_WRONLY);
    if (fd_resposta >= 0) {
        write(fd_resposta, resposta, strlen(resposta) + 1);
        close(fd_resposta);
    }
}

// Escreve ficheiro .csv com os horários dos alunos
void gravar_em_ficheiro(const char* nome_ficheiro, char* pipe_resposta) {    
    FILE* file = fopen(nome_ficheiro, "w");
    if (!file) {
        perror("Erro ao abrir o arquivo");
        int fd_resposta = open(pipe_resposta, O_WRONLY);
        if (fd_resposta >= 0) {
            write(fd_resposta, "-1", 3);
            close(fd_resposta);
        }
        return;
    }

    int num_alunos = 0;
    pthread_rwlock_wrlock(&dados_sistema.rwlock);  

    for (int aluno = 0; aluno < dados_sistema.num_alunos; aluno++) {
        fprintf(file, "%d", aluno);

        for (int d = 0; d < MAX_DISCIPLINAS; d++) {
            fprintf(file, ",");
            if (d < dados_sistema.num_disciplinas) {
                int horario_inscrito = -1;
                for (int h = 0; h < dados_sistema.num_horarios; h++) {
                    AlunoNode* node = dados_sistema.horarios[d][h].primeiro_aluno;
                    while (node) {
                        if (node->aluno_id == aluno) {
                            horario_inscrito = h;
                            break;
                        }
                        node = node->next;
                    }
                    if (horario_inscrito != -1) break;
                }
                if (horario_inscrito != -1) {
                    fprintf(file, "%d", horario_inscrito);
                }
            }
        }
        fprintf(file, "\n");
        num_alunos++;
    }

    pthread_rwlock_unlock(&dados_sistema.rwlock);

    fclose(file);

    char resposta[16];
    sprintf(resposta, "%d", num_alunos);
    int fd_resposta = open(pipe_resposta, O_WRONLY);
    if (fd_resposta >= 0) {
        write(fd_resposta, resposta, strlen(resposta) + 1);
        close(fd_resposta);
    } else {
        perror("Erro ao abrir pipe de resposta");
    }
}

// Move aluno de um horário para outro e atualiza os dados
void mudar_horario_aluno(int aluno, int disciplina, int novo_horario, const char* pipe_resposta) {
    char mensagem[MAX_MSG];
    int sucesso = 0;
    pthread_rwlock_wrlock(&dados_sistema.rwlock);

    do {
        // Verificar se a disciplina e o novo horário são válidos
        if (disciplina < 0 || disciplina >= dados_sistema.num_disciplinas ||
            novo_horario < 0 || novo_horario >= dados_sistema.num_horarios) {
            sprintf(mensagem, "Disciplina ou horário inválido");
            break;
        }

        // Procurar o aluno no horário atual
        int horario_atual = -1;
        AlunoNode *node_atual = NULL;
        for (int h = 0; h < dados_sistema.num_horarios; h++) {
            AlunoNode* node = dados_sistema.horarios[disciplina][h].primeiro_aluno;
            while (node) {
                if (node->aluno_id == aluno) {
                    horario_atual = h;
                    node_atual = node;
                    break;
                }
                node = node->next;
            }
            if (horario_atual != -1) break;
        }

        // Verificar se o aluno está matriculado na disciplina
        if (horario_atual == -1 || node_atual == NULL) {
            sprintf(mensagem, "Aluno não está matriculado nesta disciplina");
            break;
        }

        // Verificar se há vagas no novo horário
        if (dados_sistema.horarios[disciplina][novo_horario].vagas_disponiveis == 0) {
            sprintf(mensagem, "Não há vagas disponíveis no novo horário");
            break;
        }

        // Se o novo horário é diferente do atual, fazer a mudança
        if (horario_atual != novo_horario) {
            // Remover o aluno do horário atual
            if (node_atual->prev) node_atual->prev->next = node_atual->next;
            else dados_sistema.horarios[disciplina][horario_atual].primeiro_aluno = node_atual->next;
            if (node_atual->next) node_atual->next->prev = node_atual->prev;
            else dados_sistema.horarios[disciplina][horario_atual].ultimo_aluno = node_atual->prev;
            dados_sistema.horarios[disciplina][horario_atual].vagas_disponiveis++;

            // Adicionar o aluno ao novo horário
            node_atual->prev = dados_sistema.horarios[disciplina][novo_horario].ultimo_aluno;
            node_atual->next = NULL;
            if (dados_sistema.horarios[disciplina][novo_horario].ultimo_aluno) {
                dados_sistema.horarios[disciplina][novo_horario].ultimo_aluno->next = node_atual;
            } else {
                dados_sistema.horarios[disciplina][novo_horario].primeiro_aluno = node_atual;
            }
            dados_sistema.horarios[disciplina][novo_horario].ultimo_aluno = node_atual;
            dados_sistema.horarios[disciplina][novo_horario].vagas_disponiveis--;

            sucesso = 1;
            sprintf(mensagem, "Horário atualizado com sucesso");
        } else {
            sprintf(mensagem, "O aluno já está neste horário");
        }
    } while (0);

    pthread_rwlock_unlock(&dados_sistema.rwlock);

    int fd_resposta = open(pipe_resposta, O_WRONLY);
    if (fd_resposta >= 0) {
        write(fd_resposta, mensagem, strlen(mensagem) + 1);
        close(fd_resposta);
    } else {
        perror("Erro ao abrir pipe de resposta");
    }
}

// Conta o número de alunos inscritos numa disciplina
void contar_alunos_disciplina(int disciplina, const char* pipe_resposta) {
    int total_alunos = 0;
    char mensagem[MAX_MSG];

    pthread_rwlock_rdlock(&dados_sistema.rwlock);

    if (disciplina >= 0 && disciplina < dados_sistema.num_disciplinas) {
        for (int h = 0; h < dados_sistema.num_horarios; h++) {
            total_alunos += dados_sistema.horarios[disciplina][h].total_vagas - dados_sistema.horarios[disciplina][h].vagas_disponiveis;
        }
        sprintf(mensagem, "Número de alunos inscritos na disciplina %d -> %d", disciplina, total_alunos);
    } else {
        sprintf(mensagem, "Disciplina inválida");
    }

    pthread_rwlock_unlock(&dados_sistema.rwlock);

    int fd_resposta = open(pipe_resposta, O_WRONLY);
    if (fd_resposta >= 0) {
        write(fd_resposta, mensagem, strlen(mensagem) + 1);
        close(fd_resposta);
    } else {
        perror("Erro ao abrir pipe de resposta");
    }
}

// Processa os pedidos do admin
void* processar_admin(void* arg) {
    int fd_admin = open("/tmp/admin", O_RDONLY);
    if (fd_admin < 0) {
        perror("Error ao abrir admin pipe");
        return NULL;
    }

    char buffer[MAX_MSG];
    while (!deve_terminar) {
        ssize_t bytes_lidos = read(fd_admin, buffer, sizeof(buffer) - 1);
        if (bytes_lidos <= 0) continue;
        
        buffer[bytes_lidos] = '\0';

        int codop;
        char pipe_resposta[MAX_MSG];
        sscanf(buffer, "%d", &codop);

        switch (codop) {
            case 1: { // Pedir_horários
                int aluno;
                sscanf(buffer, "%d,%d,%s", &codop, &aluno, pipe_resposta);
                pedir_horarios(aluno, pipe_resposta);
                break;
            }
            case 2: { // Gravar_em_ficheiro 
                char nome_ficheiro[MAX_MSG];
                sscanf(buffer, "%d,%[^,],%s", &codop, nome_ficheiro, pipe_resposta);
                gravar_em_ficheiro(nome_ficheiro, pipe_resposta);
                break;
            }
            case 3: { // Terminar
                sscanf(buffer, "%d,%s", &codop, pipe_resposta);
                int fd_resposta = open(pipe_resposta, O_WRONLY);
                if (fd_resposta >= 0) {
                    write(fd_resposta, "Ok", 3); // Responde com "Ok"
                    close(fd_resposta);
                }
                deve_terminar = 1;
                break;
            }
            case 4: { // Mudar horário de um aluno
                int aluno, disciplina, novo_horario;
                sscanf(buffer, "%d,%d,%d,%d,%s", &codop, &aluno, &disciplina, &novo_horario, pipe_resposta);
                mudar_horario_aluno(aluno, disciplina, novo_horario, pipe_resposta);
                break;
            }
            case 5: { // Número de alunos inscritos numa disciplina
            int disciplina;
            sscanf(buffer, "%d,%d,%s", &codop, &disciplina, pipe_resposta);
            contar_alunos_disciplina(disciplina, pipe_resposta);
            break;
        }
        }
    }

    close(fd_admin);
    return NULL;
}

// Inicializa a estrutura de dados do sistema com base nos parâmetros fornecidos, aloca memória para os horários e inicializa os locks
void inicializar_sistema(int num_alunos, int num_disciplinas, int num_horarios, int vagas_por_horario) {
    dados_sistema.num_alunos = num_alunos;
    dados_sistema.num_disciplinas = num_disciplinas;
    dados_sistema.num_horarios = num_horarios;
    dados_sistema.vagas_por_horario = vagas_por_horario;

    dados_sistema.horarios = malloc(num_disciplinas * sizeof(Horario*));
    for (int d = 0; d < num_disciplinas; d++) {
        dados_sistema.horarios[d] = malloc(num_horarios * sizeof(Horario));
        for (int h = 0; h < num_horarios; h++) {
            dados_sistema.horarios[d][h].total_vagas = vagas_por_horario;
            dados_sistema.horarios[d][h].vagas_disponiveis = vagas_por_horario;
            dados_sistema.horarios[d][h].primeiro_aluno = NULL;
            dados_sistema.horarios[d][h].ultimo_aluno = NULL;
        }
    }

    pthread_mutex_init(&dados_sistema.mutex, NULL);
    pthread_cond_init(&dados_sistema.cond, NULL);
    pthread_rwlock_init(&dados_sistema.rwlock, NULL);  
}

// Matricula um aluno em uma disciplina com vagas no horário
int matricular_aluno(PedidoMatricula pedido) {
    int horario_alocado = -1;

    pthread_rwlock_wrlock(&dados_sistema.rwlock);

    for (int h = 0; h < dados_sistema.num_horarios; h++) {
        if (dados_sistema.horarios[pedido.disciplina][h].vagas_disponiveis > 0) {
            Horario* horario = &dados_sistema.horarios[pedido.disciplina][h];

            AlunoNode* novo_aluno = malloc(sizeof(AlunoNode));
            novo_aluno->aluno_id = pedido.aluno;
            novo_aluno->prev = horario->ultimo_aluno;
            novo_aluno->next = NULL;

            if (horario->ultimo_aluno) {
                horario->ultimo_aluno->next = novo_aluno;
            } else {
                horario->primeiro_aluno = novo_aluno;
            }
            horario->ultimo_aluno = novo_aluno;

            horario->vagas_disponiveis--;
            horario_alocado = h;
            break;
        }
    }

    pthread_rwlock_unlock(&dados_sistema.rwlock);
    return horario_alocado;
}

// Processa os pedidos do student
void* processar_matriculas(void* arg) {
    int fd_pipe = *((int*)arg);
    char buffer[MAX_MSG];

    while (1) {
        ssize_t bytes_lidos = read(fd_pipe, buffer, sizeof(buffer) - 1);
        if (bytes_lidos <= 0) break;
        
        buffer[bytes_lidos] = '\0';

        PedidoMatricula pedido;
        if (sscanf(buffer, "%d %d %s", &pedido.aluno, &pedido.disciplina, pedido.pipe_resposta) != 3) {
            fprintf(stderr, "Formato de mensagem inválido\n");
            continue;
        }

        int horario = matricular_aluno(pedido);

        int fd_resposta = open(pedido.pipe_resposta, O_WRONLY);
        if (fd_resposta >= 0) {
            char resposta[16];
            sprintf(resposta, "%d", horario);
            write(fd_resposta, resposta, strlen(resposta) + 1);
            close(fd_resposta);
        }
    }

    return NULL;
}

// Liberta a memória alocada e destrois os locks
void limpar_sistema() {
    for (int d = 0; d < dados_sistema.num_disciplinas; d++) {
        for (int h = 0; h < dados_sistema.num_horarios; h++) {
            AlunoNode* atual = dados_sistema.horarios[d][h].primeiro_aluno;
            while (atual) {
                AlunoNode* proximo = atual->next;
                free(atual);
                atual = proximo;
            }
        }
        free(dados_sistema.horarios[d]);
    }
    free(dados_sistema.horarios);

    pthread_mutex_destroy(&dados_sistema.mutex);
    pthread_cond_destroy(&dados_sistema.cond);
    pthread_rwlock_destroy(&dados_sistema.rwlock);
}

// Inicializa o sistema e cria as threads de admin e student
int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <num_alunos> <num_disciplinas> <num_horarios> <vagas_por_horario>\n", argv[0]);
        return 1;
    }

    int num_alunos = atoi(argv[1]);
    int num_disciplinas = atoi(argv[2]);
    int num_horarios = atoi(argv[3]);
    int vagas_por_horario = atoi(argv[4]);

    printf("Configurações:\n");
    printf("> Alunos: %d\n", num_alunos);
    printf("> Disciplinas: %d\n", num_disciplinas);
    printf("> Horários por disciplina: %d\n", num_horarios);
    printf("> Vagas por horário: %d\n", vagas_por_horario);

    inicializar_sistema(num_alunos, num_disciplinas, num_horarios, vagas_por_horario);

    mkfifo("/tmp/admin", 0666);
    pthread_t admin_threads[NUM_THREADS] ;
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&admin_threads[i], NULL, processar_admin, NULL);
    }

    mkfifo("/tmp/suporte", 0666);
    int fd_pipe = open("/tmp/suporte", O_RDONLY);
    
    pthread_t student_threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&student_threads[i], NULL, processar_matriculas, &fd_pipe);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(student_threads[i], NULL);
    }

    while (!deve_terminar) {
        sleep(1);
    }

    close(fd_pipe);
    limpar_sistema();
    unlink("/tmp/suporte");

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(admin_threads[i], NULL);
    }

    unlink("/tmp/admin");

    return 0;
}