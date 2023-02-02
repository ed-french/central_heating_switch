import requests
import time

import datetime

URL="http://192.168.1.186/getfreeheap"


with open("heaplog.txt","a") as outfile:
    while True:

        try:
            res=requests.get(URL).content.decode()

        except Exception as e:
            print(f"Failed to fetch this time: {e}")

        else:
            mess=f'{datetime.datetime.now().isoformat()} {res.split("=")[1]}'
            print(mess)
            outfile.write(mess+"\n")

        time.sleep(5*60)
