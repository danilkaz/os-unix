ser: build_server run_server

cli: build_client generate_numbers run_client

build_server:
	gcc server.c -o server

run_server:
	@./server

build_client:
	gcc client.c -o client

generate_numbers:
	[ ! -f numbers ] && python3 generate_numbers.py >numbers || true

run_client:
	@./client $(sleep_time) <numbers
