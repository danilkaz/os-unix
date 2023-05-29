import random

s = 0
for i in range(1, 1001):
    n = random.randint(0, 5000)
    if i % 40 == 0:
        print(-s)
        s = 0
    else:
        s += n
        print(n)
