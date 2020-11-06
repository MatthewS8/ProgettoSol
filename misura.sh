#get number of clients as argument
NCLIENT=$1
SUPERVISOR_PATH=$2
CLIENT_PATH=$3
NEW_SUPERVISOR_PATH="Supervisor.log"
NEW_CLIENT_PATH="Client.log"
if [[ -f $NEW_SUPERVISOR_PATH ]]; then
  unlink $NEW_SUPERVISOR_PATH
fi
touch $NEW_SUPERVISOR_PATH
if [[ -f $NEW_CLIENT_PATH ]]; then
  unlink $NEW_CLIENT_PATH
fi
touch $NEW_CLIENT_PATH

echo "Estrazione dati"
awk '/SUPERVISOR ESTIMATE/ {print $5 " " $3 }' $SUPERVISOR_PATH | tail -$NCLIENT | sort -dk1 | uniq > $NEW_SUPERVISOR_PATH
awk '/SECRET/ {print $2 " " $4}' $CLIENT_PATH| head -$NCLIENT | sort -dk1 > $NEW_CLIENT_PATH

IFS=$'\n'
CORRECT=0
TOT=0
JIT_TOT=0
DELTA=0

for line in $(cat $NEW_SUPERVISOR_PATH);
    do
      ID=$(cut -d' ' -f1 <<< $line)
      STIMA=$(cut -d' ' -f2 <<< $line)
      CLIENT_LINE=$(grep $ID $NEW_CLIENT_PATH)
      CLIENT_ID=$(cut -d' ' -f1 <<< $CLIENT_LINE)
      CLIENT_SECRET=$(($(cut -d' ' -f2 <<< $CLIENT_LINE) + 0))
      ((TOT++))
      DELTA=$(($STIMA - $CLIENT_SECRET))
      printf "ID: %16s\tSECRET: %4d\tSTIMA: %5d\tDELTA: %4d\n" "$ID" "$CLIENT_SECRET" "$STIMA" "$DELTA"
      if [ $DELTA -lt 25 ] && [ $DELTA -gt -25 ]; then
          ((CORRECT++))
      fi
      ((JIT_TOT = JIT_TOT + DELTA))
    done
echo "Secret stimati correttamente: $CORRECT su $TOT"
if [ $TOT != '20' ]; then
  printf "%d clients non sono stati stimati\n" "$((20 - $TOT))"
fi
awk -v jit="$JIT_TOT" -v tot="$TOT" 'BEGIN {print "Media errore: " (jit/tot) "ms"; exit}'
