#!/bin/bash

# Verifica o número de argumentos passados ( 3 argumentos obrigatórios)
if [ $# -ne 3 ]; then
    echo "Uso: $0 <NALUN> <NLUG> <NSTUD>"
    exit 1
fi

# Nº total de alunos
NALUN=$1

# Nº de disciplinas
NDISCIP=10

# Nº de lugares na sala
NLUG=$2

# Nº de processos student
NSTUD=$3

# Calcula número de horários necessários
# Garante que há vagas suficientes para todos os alunos
NHOR=$(( (NALUN + NLUG - 1) / NLUG ))

# Verifica se o name pipe existe, remove se existir
[ -p /tmp/suporte ] && rm /tmp/suporte
mkfifo /tmp/suporte

# Inicia support_agent
./support_agent $NALUN $NDISCIP $NHOR $NLUG &

# Calcula alunos por cada processo student
ALUNOS_POR_STUDENT=$(( (NALUN + NSTUD - 1) / NSTUD ))

# Inicia os processos student
for ((i=1; i<=NSTUD; i++)); do
    ALUNO_INICIAL=$(( (i-1) * ALUNOS_POR_STUDENT )) # Identifica em que aluno o student começa
    NUM_ALUNOS=$ALUNOS_POR_STUDENT
    
    # Inclui apenas os alunos que restarem, verificando se ultrapassa o nº total de alunos
    if [ $(( ALUNO_INICIAL + NUM_ALUNOS )) -gt $NALUN ]; then
        NUM_ALUNOS=$(( NALUN - ALUNO_INICIAL ))
    fi
    
    ./student $i $ALUNO_INICIAL $NUM_ALUNOS &
done

# Aguarda todos os processos acabarem
wait

# Remove named pipe
rm -f /tmp/suporte

echo "Todos os processos terminaram com sucesso."