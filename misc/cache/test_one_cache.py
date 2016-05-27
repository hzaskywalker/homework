import os
import threading
import time
import subprocess
filename = 'flag'
try:
    os.system('rm flag')
except:
    pass

os.system('make')

tasks = []
def run(name):
    subprocess.call(["./{}".format(name)], shell = True)
server = threading.Thread(group = None, target = run, args=('server',))
server.start()
time.sleep(1)
tasks.append(server)
print('server begin ... ')

cache = threading.Thread(group = None, target = run, args=('cache 1',))
cache.start()
print('cache begin ... ')
time.sleep(1)
tasks.append(cache)

print('trying to strt test_cache')
test_cache = threading.Thread(group = None, target = run, args=('test_cache 1',))
test_cache.start()
tasks.append(test_cache)

for p in tasks:
    p.join()
