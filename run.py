import pathlib
import subprocess

JSON_DIR = r'./json/random'
EXEC = pathlib.Path('./build/mskd_cc')
input_dir = pathlib.Path(JSON_DIR)
rst_dir = pathlib.Path('./result')

if __name__ == "__main__":
    for json in filter(lambda x: x.is_file(), input_dir.iterdir()):
        # print(EXEC, json)
        cmd = [str(EXEC), str(json), str(pathlib.Path(rst_dir)/(json.name))]
        print(cmd)
        subprocess.run(cmd, stdout=subprocess.PIPE)

