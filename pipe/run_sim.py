import os
import threading
import time
import subprocess
import sys
tasks = []
os.system('make')
def run(name):
    print(name)
    subprocess.call(["./{}".format(name)], shell = True)
server = threading.Thread(group = None, target = run, args=('../misc/cache/server',))
server.start()
time.sleep(1)
tasks.append(server)
print('server begin ... ')

cache0 = threading.Thread(group = None, target = run, args=('../misc/cache/cache 0',))
cache1 = threading.Thread(group = None, target = run, args=('../misc/cache/cache 1',))
cache0.start()
cache1.start()
print('cache begin ... ')
time.sleep(1)

psim = threading.Thread(group = None, target = run, args=('psim ' + sys.argv[1],))
psim.start()
tasks+=[psim]

for p in tasks:
    p.join()
