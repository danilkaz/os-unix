cnt=$1
sleep=$2

if [ -z "$cnt" ]
then 
    cnt=100
fi

if [ -z "$sleep" ]
then 
    sleep=200
fi

rm -f /tmp/server.log /tmp/client_*.log

# Run clients
for i in $(seq 1 $cnt)
do
    make run_client sleep_time=$sleep > /tmp/client_$i.log &
    pids[${i}]=$!
done

# Wait clients
for pid in ${pids[*]}; do
    wait $pid
done

# Calculate times
start=$(cat /tmp/server.log | head -n1 | awk '{printf $1}' | xargs date +%s -d)
end=$(cat /tmp/server.log | tail -n1 | awk '{printf $1}' | xargs date +%s -d)

work_time=$(($end - $start))

# Calculate sleep time
max_client_sleep_time=0
for i in $(seq 1 $cnt)
do
    client_sleep_time=$(cat /tmp/client_$i.log)
    if [ "$client_sleep_time" -gt "$max_client_sleep_time" ]
    then
        max_client_sleep_time=$client_sleep_time
    fi
done

max_client_sleep_time=$(( $max_client_sleep_time / 1000 ))

echo "Efficiency check with $cnt clients, $sleep sleep time"
echo "Server work time = $work_time s, max client sleep time = $max_client_sleep_time s"
echo "Result = $(($work_time - $max_client_sleep_time))"
echo "----------------------------------------------------------------------------------"
