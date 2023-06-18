#! python3

import os
import copy
from random import randint, shuffle
import pathlib
import json

MAX_DEPTH=8
MAX_TRIPCOUNT = 3 # 2^k
MAX_INNERMOST_II=3
MAX_LOOP_BIAS=3
MAX_BIAS = 20


DIR = pathlib.Path("./json/random")

Nlist = [(64 * (i//8+1)) for i in range(8*8)]
# Nlist = [8]

if __name__=='__main__':
    # os.system(f"rm -f {DIR}/*")
    for (ID, N) in enumerate(Nlist):
        NP = 0
        port_list = []
        port_dangl = []
        conn_list = []
        from_list = []
        to_list = []
        port_of_mod = [[] for _ in range(N)]
        
        for mod in range(N):
            # 0-2 inputs
            num_in = max(min(randint(0, 2), len(port_dangl)), len(port_dangl) // (N-mod))
            
            pick = randint(0, len(port_dangl)-num_in)
            for (j, p) in enumerate(port_dangl[pick:pick+num_in]):
                depth0, depth1 = randint(1, MAX_DEPTH), randint(1, MAX_DEPTH)
                # let depth0 <  depth1
                if depth0 > depth1:
                    depth0, depth1 = depth1, depth0

                nlist1 = [1<<randint(1, MAX_TRIPCOUNT) for _ in range(depth1)]
                nlist0 = copy.deepcopy(nlist1) 
                shuffle(nlist1)

                # reduce len(nlist1) to depth1
                for _ in range(depth1 - depth0):
                    a, b = nlist0[-1], nlist0[-2]
                    nlist0 = nlist0[0:-2] + [a*b]
                
                ilist0 = [randint(1, MAX_INNERMOST_II-1)]
                for i in range(depth0-1):
                    ilist0.append(ilist0[-1] * nlist0[-i-1]) #+ randint(0, MAX_LOOP_BIAS)) # alleviate negative loops
                ilist0 = ilist0[::-1]

                ilist1 = [randint(ilist0[-1], min(MAX_INNERMOST_II, ilist0[-1]+3))]
                for i in range(depth1-1):
                    ilist1.append(ilist1[-1] * nlist1[-i-1] + randint(0, MAX_LOOP_BIAS))
                ilist1 = ilist1[::-1]

                # if randint(0, 9) >= 2:
                # depth0, depth1 = depth1, depth0
                # nlist0, nlist1 = nlist1, nlist0
                # ilist0, ilist1 = ilist1, ilist0
                
                bias0 = randint(0, MAX_BIAS-1)
                bias1 = randint(bias0, MAX_BIAS)

                # if ID==0:
                #     print(p[0], NP, depth0, depth1)
                # p -> xxx0; NP++ -> xxx1
                # port_list: 0 -> pid, 1 -> module, 2 -> depth, 3 -> nlist, 4 -> interval, 5 -> bias
                port_list.append((p[0], p[1], depth0, nlist0, ilist0, bias0))
                port_list.append((NP, mod, depth1, nlist1, ilist1, bias1))
                from_list.append(p[0])
                to_list.append(NP)
                port_of_mod[mod].append(NP)
                NP += 1


            port_dangl = port_dangl[:pick] + port_dangl[pick+num_in:]

            # 0-2 outputs
            
            num_out = min(max(randint(0, 2), 1 if mod*mod < N else 0), (N-mod-1)*2-len(port_dangl))
            for i in range(num_out):
                port_dangl.append((NP, mod))
                port_of_mod[mod].append(NP)
                NP += 1

        assert len(port_dangl) == 0, "no dangling ports"
        assert len(from_list) == NP//2, "two ports per connection"
        port_list.sort(key=lambda p: p[0])

        # print(port_of_mod)

        def get_latency(mod):
            lat = 0
            for pid in port_of_mod[mod]:
                lat = max(lat, port_list[pid][3][0] * port_list[pid][4][0] + port_list[pid][5])
            lat += randint(0, MAX_BIAS)
            return lat

        # if ID==0:
        #     print(port_list[:3])
        with open(DIR/f'case{ID}_n_{N}.json', 'w') as f:
            cycle_list = [{'N': port_list[p][2], 'n': port_list[p][3], 'I': port_list[p][4], 'b': port_list[p][5]} for p in range(NP)] 
            dic = {'N': N, 'NP': NP, 'ND': NP//2, 'from': from_list, 'to': to_list, 'mp': [p[1] for p in port_list], 'latency': [get_latency(m) for m in range(N)], 'cycle': cycle_list}
            json.dump(dic, f, indent=2)