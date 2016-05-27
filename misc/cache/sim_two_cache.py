f = open('cache_record.txt', 'r').readlines()

mem = [0] * 10000
result = [[], []]

for lines in f:
    cpu, TYPE, pos, val = map(int, lines.strip().split(' '))
    if TYPE == 0:
        result[cpu].append(mem[pos])
    else:
        mem[pos] = val

def check(name, result):
    ans = list( map( lambda x:int(x.strip()), open(name).readlines() ) )
    flag = True
    if len(ans) != len(result):
        print(name, len(ans), len(result))
        flag = False
    tot = 0
    for a, b in zip(ans, result):
        tot+=1
        if a!=b:
            print(tot, a, b)
            flag = False
    if flag:
        print('Passed test example')
    else:
        print('Wrong')

check('ans_0.txt', result[0])
check('ans_1.txt', result[1])
