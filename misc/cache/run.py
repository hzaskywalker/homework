import os
import threading
filename = 'flag'
try:
    os.system('rm flag')
except:
    pass

os.system("gcc server.c -o server")
tasks = []
#TODO
def run_server():
    os.system("./server")
server = threading.Thread(group = None, target = run_server)
server.start()
tasks.append(server)

while True:
    if os.path.exists('flag'):
        break

print('server begin ... ')

for p in tasks:
    p.join()
