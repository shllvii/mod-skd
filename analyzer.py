import pathlib
import json
import seaborn

rst_dir = pathlib.Path('./result')
prod_dir = pathlib.Path('./production')

if __name__=='__main__':
    table = []
    for jf in filter(lambda x: x.is_file(), rst_dir.iterdir()):
        with open(jf, 'r') as fr: 
            jsn = json.load(fr)
            jsn['Name'] = jf.stem
            table.append(jsn)
    
    table.sort(key=lambda jsn: int(jsn['Name'].split('_')[0][4:]))
    with open(prod_dir/"report.csv", "w") as fw:
        fw.write('Case,#Mod,#Port,$\sigma depth$,$\sigma lat$,MTime(ms),BTime(ms)\n')
        for row in table:
            name, N, NP, sd, sl, mt, bt = row['Name'], row['N'], row['NP'], row['sum_depth'], row['sum_latency'], row['mskd_duration'], row['buff_duration']
            fw.write(f'{name},{N},{NP},{sd},{sl},{mt},{bt}\n')
    with open(prod_dir/"table.tex", "w") as fw:
        fw.write(
'''
\\begin{table}[h]
    \\centering
    \\caption{Evaluation results}
    \\label{tab:eval}
    \resizebox{!}{0.9\linewidth}{
    \\begin{tabular}{ccccccc}
        \\hline
        Case & \\#Mod & \\#Port & $\sum$ depth & $\sum$ lat & MTime & BTime   \\\\
'''
        )

        for row in table:
            name, N, NP, sd, sl, mt, bt = row['Name'], row['N'], row['NP'], row['sum_depth'], row['sum_latency'], row['mskd_duration'], row['buff_duration']
            name = name.split('_')[0]
            fw.write(f'\t\t{name} & {N} & {NP} & {sd} & {sl} & {mt} & {bt}\\\\\n')
        fw.write(
'''
        \\hline
    \\end{tabular}
    }
\\end{table}
'''
        )





    


