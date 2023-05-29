rm -f /tmp/server.log

# Build and run server
make build_server
make run_server &

# Build client
make build_client generate_numbers

# Run clients
./many_clients.sh

# Part 1
echo "Checking state after 100 clients"
echo '0\n' | socat - UNIX-CONNECT:/tmp/server.sock > res.txt
cat res.txt
rm res.txt

# Part 2
for i in {1..5}
do
    echo "Many runs of many clients: ${i}"
    ./many_clients.sh
done

# Check heap
cat /tmp/server.log | grep sbrk | (head -n1 && tail -n1)

# Kill server
pkill server

# Part 3
# Efficiency checks
for cnt in {20..100..20}
do
    for sleep in {0..1000..200}
    do
        ./efficiency_check.sh $cnt $sleep
    done
done

# Kill server
pkill server