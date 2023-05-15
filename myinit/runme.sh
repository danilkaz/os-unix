# Build app1
cd app1
go build -o /tmp .
touch /tmp/app1in
touch /tmp/app1out
cd ..

# Build app2
cd app2
go build -o /tmp .
touch /tmp/app2in
touch /tmp/app2out
cd ..

# Build app3
cd app3
go build -o /tmp .
touch /tmp/app3in
touch /tmp/app3out
cd ..

# Create config
rm -f /tmp/config
echo '/tmp/app1 /tmp/app1in /tmp/app1out' >> /tmp/config
echo '/tmp/app2 /tmp/app2in /tmp/app2out' >> /tmp/config
echo '/tmp/app3 /tmp/app3in /tmp/app3out' >> /tmp/config

# Delete logs
rm -f /tmp/myinit.log

# Build myinit
make build
./myinit /tmp/config

# First check
sleep 1
echo "First check: need 3 subprocesses, found $(ps x | grep 'app[0-9]' | wc -l)"

pkill app2

# Second check
sleep 1
echo "Second check: need 3 subprocesses, found $(ps x | grep 'app[0-9]' | wc -l)"

# Change config
rm -f /tmp/config
echo '/tmp/app1 /tmp/app1in /tmp/app1out' >> /tmp/config

pkill -SIGHUP myinit

# Third check
sleep 1
echo "Third check: need 1 subprocess, found $(ps x | grep 'app[0-9]' | wc -l)"

# Check logs 
echo "Check logs:"
cat /tmp/myinit.log

# Shutdown
pkill myinit
pkill app1