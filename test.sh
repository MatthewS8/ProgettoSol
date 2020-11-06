#!/bin/env bash

SUPERVISOR_PATH="supervisor.log"
CLIENT_PATH="client.log"

if [[ -f ${SUPERVISOR_PATH} ]]; then
    unlink ${SUPERVISOR_PATH}
fi
touch ${SUPERVISOR_PATH}
if [[ -f ${CLIENT_PATH} ]]; then
    unlink ${CLIENT_PATH}
fi
touch ${CLIENT_PATH}

echo "Avvio SUPERVISOR"
./supervisor 8  1>>${SUPERVISOR_PATH} &
sleep 2

echo "Avvio 20 CLIENT"
for x in $(seq 10);
    do
      ./client 5 8 20  1>>${CLIENT_PATH} &
      ./client 5 8 20  1>>${CLIENT_PATH} &
      sleep 1
    done

for i in $(seq 6);
    do
      printf "\nMando il %d SIGINT al SUPERVISOR\n" "$i"
      pkill -INT supervisor
      sleep 10
    done

echo -e "\nDoppio SIGINT al SUPERVISOR"
pkill -INT supervisor
pkill -INT supervisor

sleep 1
echo "Risultati misura"
bash misura.sh 20 $SUPERVISOR_PATH $CLIENT_PATH
