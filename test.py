import socket
import time

sent = False

f = open("cmaj.mid", mode="rb")
data = f.read()
print(data)
f.close()

print("Running TCP Socket server...")
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(('0.0.0.0', 1235 ))
s.listen(0)                 
 
while True:
    client, addr = s.accept()
    print(f"Connection from {addr} has been established!")
    client.settimeout(5)
    while True:
        content = client.recv(1024)
        if len(content) ==0:
           break
        if str(content,'utf-8') == '\r\n':
            continue
        else:
            print(str(content,'utf-8'))
            client.send(data)
            sent = True
            break
    client.close()
    if sent == True:
        s.close()
    break
