import sys
import socket

UDP_IP = sys.argv[1]
UDP_PORT = 777

cmd = sys.argv[2]
val = sys.argv[3].decode('hex')
if cmd == 't':
	val = '{}{}{}'.format(val, sys.argv[4].decode('hex'), sys.argv[5].decode('hex'))

MESSAGE = '{}{}'.format(cmd, val)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.sendto(MESSAGE, (UDP_IP, UDP_PORT))


