import os
import threading
import time
import subprocess
filename = 'flag'
try:
    os.system('rm flag')
except:
    pass

os.system('make clean')
os.system('make')

tasks = []
def run(name):
    subprocess.call(["./{}".format(name)], shell = True)
server = threading.Thread(group = None, target = run, args=('server',))
server.start()
time.sleep(1)
tasks.append(server)
print('server begin ... ')

cache0 = threading.Thread(group = None, target = run, args=('cache 0',))
cache1 = threading.Thread(group = None, target = run, args=('cache 1',))
cache0.start()
cache1.start()
print('cache begin ... ')
time.sleep(1)
tasks+=[cache0, cache1]

print('trying to start test_cache')
test_cache0 = threading.Thread(group = None, target = run, args=('test_cache 0',))
test_cache1 = threading.Thread(group = None, target = run, args=('test_cache 1',))
test_cache0.start()
test_cache1.start()
test_cache3 = threading.Thread(group = None, target = run, args=('step_test',))
#test_cache3.start()
tasks+=[test_cache0, test_cache1]

for p in tasks:
    p.join()
