from psutil import cpu_percent

for _ in range(700):
    print(cpu_percent(interval=0.01))