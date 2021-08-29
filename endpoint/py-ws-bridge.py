#!/usr/bin/python
from websocket import create_connection
import sys

ws = create_connection("ws://<URL-WSS>:6969/")
ws.send(sys.argv[1])
ws.close()
