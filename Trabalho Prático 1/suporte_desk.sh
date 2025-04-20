#!/bin/bash

# Valida os argumentos 
if [ $# -ne 2 ]; then # Verifica se o nº de argumentos != 2
  echo "Uso: $0 <numero_de_estudantes> <nome_do_pipe>"
  exit 1
fi

NUM_ESTUDANTES=$1 # Primeiro argumento passado
PIPE_NAME=$2 # Segundo argumento passado

# Verifica se o número de estudantes é válido
if [ "$NUM_ESTUDANTES" -le 0 ]; then # <=
  echo "Erro: Número de estudantes deve ser positivo."
  exit 1
fi

# Cria o named pipe se ainda não existir
if [ ! -p "$PIPE_NAME" ]; then
  echo "A criar o named pipe: $PIPE_NAME"
  mkfifo "$PIPE_NAME"
else
  echo "Named pipe $PIPE_NAME já existe."
fi

# Executa o nº de estudantes em background
for ((i=1; i<=NUM_ESTUDANTES; i++)); do
  ./student "$PIPE_NAME" "Pedido_$i" &
  echo "Estudante $i enviou um pedido."
done

# Executa o suporte_agente em background
./suporte_agente.sh "$PIPE_NAME" &

sleep 1

# Envia a mensagem quit para o named pipe
echo "quit" > "$PIPE_NAME"
echo "Mensagem "quit" enviada."

# Aguarda que o agente de suporte termine de processar
wait

# Remove o named pipe
rm "$PIPE_NAME"

echo "Todos os processos terminaram. A sair..."