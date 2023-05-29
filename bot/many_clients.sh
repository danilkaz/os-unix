# Run clients
for i in {1..100}
do
    make run_client > /dev/null &
    pids[${i}]=$!
done

# Wait clients
for pid in ${pids[*]}; do
    wait $pid
done
