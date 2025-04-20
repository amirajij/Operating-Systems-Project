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

typedef struct {
    int total_vagas;
    int vagas_disponiveis;
    int matriculados;
} Horario;

typedef struct {
    Horario** horarios;
    int num_disciplinas;
    int num_horarios;
    int num_alunos;
    int vagas_por_horario;
    int total_matriculados;
    int deve_encerrar;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} DadosSistema;

DadosSistema dados_sistema;

void imprimir_erro(const char* msg) {
    fprintf(stderr, "Erro: %s (errno: %d - %s)\n", msg, errno, strerror(errno));
}

int verificar_capacidade() {
    int capacidade_total = dados_sistema.num_horarios * dados_sistema.vagas_por_horario;
    if (capacidade_total < dados_sistema.num_alunos) {
        fprintf(stderr, "Erro: Capacidade insuficiente!\n");
        fprintf(stderr, "Total de vagas por disciplina: %d\n", capacidade_total);
        fprintf(stderr, "Número de alunos: %d\n", dados_sistema.num_alunos);
        fprintf(stderr, "Necessário aumentar número de horários ou lugares por horário\n");
        return 0;
    }
    return 1;
}

void imprimir_estatisticas() {
    printf("\n=== Resultados ===\n");
    printf("Total de alunos: %d\n", dados_sistema.num_alunos);
    
    int todos_matriculados = 1;
    
    for (int d = 0; d < dados_sistema.num_disciplinas; d++) {
        printf("\nDisciplina %d:\n", d + 1);
        int total_disciplina = 0;
        for (int h = 0; h < dados_sistema.num_horarios; h++) {
            int matriculados = dados_sistema.horarios[d][h].total_vagas - 
                             dados_sistema.horarios[d][h].vagas_disponiveis;
            printf("  Horário %d: %d alunos\n", h + 1, matriculados);
            total_disciplina += matriculados;
        }
        printf("  Total na disciplina: %d alunos\n", total_disciplina);
        
        if (total_disciplina != dados_sistema.num_alunos) {
            todos_matriculados = 0;
            printf("  AVISO: Nem todos os alunos estão inscritos nesta disciplina!\n");
        }
    }
    
    if (!todos_matriculados) {
        printf("\nAVISO: Existem alunos que não foram inscritos em todas as disciplinas!\n");
    } else {
        printf("\nSucesso: Todos os alunos foram inscritos em todas as disciplinas!\n");
    }
    
    printf("\n===========================\n");
}

int processar_matricula(int aluno_inicial, int num_alunos) {
    int matriculados = 0;
    pthread_mutex_lock(&dados_sistema.mutex);
    
    int* matriculas_por_disciplina = calloc(dados_sistema.num_disciplinas, sizeof(int));
    if (!matriculas_por_disciplina) {
        imprimir_erro("Falha na alocação de memória.");
        pthread_mutex_unlock(&dados_sistema.mutex);
        return 0;
    }
    
    for (int aluno = aluno_inicial; aluno < aluno_inicial + num_alunos; aluno++) {
        int aluno_totalmente_matriculado = 1;
        
        for (int d = 0; d < dados_sistema.num_disciplinas; d++) {
            int matriculado_na_disciplina = 0;
            
            for (int h = 0; h < dados_sistema.num_horarios && !matriculado_na_disciplina; h++) {
                if (dados_sistema.horarios[d][h].vagas_disponiveis > 0) {
                    dados_sistema.horarios[d][h].vagas_disponiveis--;
                    dados_sistema.horarios[d][h].matriculados++;
                    matriculas_por_disciplina[d]++;
                    matriculado_na_disciplina = 1;
                }
            }
            
            if (!matriculado_na_disciplina) {
                aluno_totalmente_matriculado = 0;
                break;
            }
        }
        
        if (aluno_totalmente_matriculado) {
            matriculados++;
            dados_sistema.total_matriculados++;
        } else {
            for (int d = 0; d < dados_sistema.num_disciplinas; d++) {
                if (matriculas_por_disciplina[d] > 0) {
                    for (int h = dados_sistema.num_horarios - 1; h >= 0; h--) {
                        if (dados_sistema.horarios[d][h].matriculados > 0) {
                            dados_sistema.horarios[d][h].matriculados--;
                            dados_sistema.horarios[d][h].vagas_disponiveis++;
                            matriculas_por_disciplina[d]--;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    free(matriculas_por_disciplina);
    
    if (dados_sistema.total_matriculados >= dados_sistema.num_alunos) {
        dados_sistema.deve_encerrar = 1;
        pthread_cond_broadcast(&dados_sistema.cond);
    }
    
    pthread_mutex_unlock(&dados_sistema.mutex);
    return matriculados;
}

void inicializar_sistema(int num_disciplinas, int num_horarios, int vagas_por_horario, int num_alunos) {
    if (num_disciplinas <= 0 || num_horarios <= 0 || vagas_por_horario <= 0 || num_alunos <= 0) {
        imprimir_erro("Parâmetros inválidos na inicialização!");
        exit(1);
    }

    dados_sistema.num_disciplinas = num_disciplinas;
    dados_sistema.num_horarios = num_horarios;
    dados_sistema.num_alunos = num_alunos;
    dados_sistema.total_matriculados = 0;
    dados_sistema.deve_encerrar = 0;
    
    dados_sistema.horarios = malloc(num_disciplinas * sizeof(Horario*));
    if (!dados_sistema.horarios) {
        imprimir_erro("Falha na alocação de memória!");
        exit(1);
    }

    for (int i = 0; i < num_disciplinas; i++) {
        dados_sistema.horarios[i] = malloc(num_horarios * sizeof(Horario));
        if (!dados_sistema.horarios[i]) {
            imprimir_erro("Falha na alocação de memória!");
            exit(1);
        }
        for (int j = 0; j < num_horarios; j++) {
            dados_sistema.horarios[i][j].total_vagas = vagas_por_horario;
            dados_sistema.horarios[i][j].vagas_disponiveis = vagas_por_horario;
            dados_sistema.horarios[i][j].matriculados = 0;
        }
    }
    
    if (pthread_mutex_init(&dados_sistema.mutex, NULL) != 0) {
        imprimir_erro("Falha na inicialização do mutex.");
        exit(1);
    }
    
    if (pthread_cond_init(&dados_sistema.cond, NULL) != 0) {
        imprimir_erro("Falha na inicialização da condition variable.");
        exit(1);
    }
}

void limpar_sistema() {
    for (int i = 0; i < dados_sistema.num_disciplinas; i++) {
        free(dados_sistema.horarios[i]);
    }
    free(dados_sistema.horarios);
    pthread_mutex_destroy(&dados_sistema.mutex);
    pthread_cond_destroy(&dados_sistema.cond);
}

void* thread_trabalhadora(void* arg) {
    int fd = *((int*)arg);
    char buffer[MAX_MSG];
    char msg[MAX_MSG];
    int posicao = 0;
    
    while (1) {
        pthread_mutex_lock(&dados_sistema.mutex);
        if (dados_sistema.deve_encerrar) {
            pthread_mutex_unlock(&dados_sistema.mutex);
            break;
        }
        pthread_mutex_unlock(&dados_sistema.mutex);
        
        pthread_mutex_lock(&dados_sistema.mutex);
        char letra;
        posicao = 0;
        ssize_t resultado_leitura;
        while ((resultado_leitura = read(fd, &letra, 1)) > 0 && letra != '\0') {
            buffer[posicao++] = letra;
        }
        buffer[posicao] = '\0';
        
        if (resultado_leitura < 0) {
            imprimir_erro("Erro na leitura do pipe.");
            pthread_mutex_unlock(&dados_sistema.mutex);
            continue;
        }
        pthread_mutex_unlock(&dados_sistema.mutex);
        
        if (posicao == 0) continue;
        
        int aluno_inicial, num_alunos;
        char pipe_resposta[MAX_MSG];
        if (sscanf(buffer, "%d %d %s", &aluno_inicial, &num_alunos, pipe_resposta) != 3) {
            imprimir_erro("Formato de mensagem inválido.");
            continue;
        }
        
        int matriculados = processar_matricula(aluno_inicial, num_alunos);
        
        int fd_resposta = open(pipe_resposta, O_WRONLY);
        if (fd_resposta < 0) {
            imprimir_erro("Não foi possível abrir o pipe de resposta.");
            continue;
        }
        
        sprintf(msg, "%d", matriculados);
        if (write(fd_resposta, msg, strlen(msg) + 1) < 0) {
            imprimir_erro("Erro ao escrever no pipe de resposta.");
        }
        close(fd_resposta);
    }
    
    return NULL;
}

void manipulador_sinal(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        printf("\nRecebido sinal de término. A terminar...\n");
        pthread_mutex_lock(&dados_sistema.mutex);
        dados_sistema.deve_encerrar = 1;
        pthread_cond_broadcast(&dados_sistema.cond);
        pthread_mutex_unlock(&dados_sistema.mutex);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <num_alunos> <num_disciplinas> <num_horarios> <vagas_por_horario>\n", argv[0]);
        return 1;
    }
    
    signal(SIGINT, manipulador_sinal);
    signal(SIGTERM, manipulador_sinal);
    
    int num_alunos = atoi(argv[1]);
    int num_disciplinas = atoi(argv[2]);
    int num_horarios = atoi(argv[3]);
    int vagas_por_horario = atoi(argv[4]);
    
    printf("Iniciando agente_suporte com:\n");
    printf("- Número de alunos: %d\n", num_alunos);
    printf("- Número de disciplinas: %d\n", num_disciplinas);
    printf("- Número de horários por disciplina: %d\n", num_horarios);
    printf("- Vagas por horário: %d\n", vagas_por_horario);
    
    dados_sistema.num_alunos = num_alunos;
    dados_sistema.num_horarios = num_horarios;
    dados_sistema.vagas_por_horario = vagas_por_horario;
    if (!verificar_capacidade()) {
        return 1;
    }

    inicializar_sistema(num_disciplinas, num_horarios, vagas_por_horario, num_alunos);
    
    if (mkfifo("/tmp/suporte", 0666) < 0 && errno != EEXIST) {
        imprimir_erro("Não foi possível criar o named pipe.");
        return 1;
    }
    
    int fd = open("/tmp/suporte", O_RDONLY);
    if (fd < 0) {
        imprimir_erro("Não foi possível abrir o named pipe.");
        return 1;
    }
    
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, thread_trabalhadora, &fd) != 0) {
            imprimir_erro("Falha ao criar thread.");
            return 1;
        }
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    imprimir_estatisticas();
    
    close(fd);
    limpar_sistema();
    
    printf("Agente de suporte finalizado com sucesso.\n");
    return 0;
}