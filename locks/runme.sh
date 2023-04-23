make build

for i in {1..10} 
do 
    ./lock myfile &
    pids[${i}]=$!
done

sleep 300

killall -SIGINT lock

for pid in ${pids[*]}; do
    wait $pid
done

cat stats.txt