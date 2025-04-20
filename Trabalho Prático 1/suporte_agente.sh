#!/bin/bash

# Verifica os argumentos
if [ $# -ne 1 ]; then
  echo "Uso: $0 <nome_do_pipe>"
  exit 1
fi

PIPE_NAME=$1

# Verifica se o named pipe existe
if [ ! -p "$PIPE_NAME" ]; then
  echo "Erro: O pipe $PIPE_NAME não existe."
  exit 1
fi

echo "Pipe $PIPE_NAME encontrado, iniciar o agente de suporte."
  
# Loop para processar os pedidos
while true; do

  if IFS= read -r PEDIDO; then

    # Verifica se recebeu o quit
    if [ "$PEDIDO" == "quit" ]; then
      echo "Recebido "quit", a sair..."
      break
    fi

    # Processamento do pedido entre 1 a 5 seg.
    echo "Pedido recebido: $PEDIDO"
    TEMPO_ESPERA=$((RANDOM % 5 + 1)) # (0-4)(+1)
    sleep $TEMPO_ESPERA
    echo "Pedido $PEDIDO tratado."

  fi
  
done < "$PIPE_NAME"

echo "Agente de suporte terminou. Todos os pedidos foram processados."