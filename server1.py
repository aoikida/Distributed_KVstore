import socket
import threading

class KeyValueStore:
    def __init__(self):
        self.store = {}
        self.lock = threading.Lock()

    def get(self, key):
        with self.lock:
            return self.store.get(key)

    def set(self, key, value):
        with self.lock:
            self.store[key] = value
            return True

    def delete(self, key):
        with self.lock:
            return self.store.pop(key, None)

class Node:
    def __init__(self, host, port, peer_host=None, peer_port=None):
        self.host = host
        self.port = port
        self.peer_host = peer_host
        self.peer_port = peer_port
        self.kv_store = KeyValueStore()
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)
        print(f"Node started at {self.host}:{self.port}")

    def handle_client(self, client_socket):
        while True:
            data = client_socket.recv(1024).decode('utf-8')
            if not data:
                break
            
            parts = data.split()
            command = parts[0].upper()
            
            if command == 'GET':
                key = parts[1]
                value = self.kv_store.get(key)
                client_socket.send(str(value).encode('utf-8'))
            elif command == 'SET':
                key, value = parts[1], parts[2]
                self.kv_store.set(key, value)
                self.propagate_update(data)
                client_socket.send(b'OK')
            elif command == 'DELETE':
                key = parts[1]
                self.kv_store.delete(key)
                self.propagate_update(data)
                client_socket.send(b'OK')
            else:
                client_socket.send(b'Invalid command')
        
        client_socket.close()

    def propagate_update(self, command):
        if self.peer_host and self.peer_port:
            try:
                peer_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                peer_socket.connect((self.peer_host, self.peer_port))
                peer_socket.send(command.encode('utf-8'))
                peer_socket.close()
            except Exception as e:
                print(f"Failed to propagate update: {e}")

    def start(self):
        while True:
            client_socket, addr = self.server_socket.accept()
            print(f"Connection from {addr}")
            client_thread = threading.Thread(
                target=self.handle_client,
                args=(client_socket,)
            )
            client_thread.start()

if __name__ == "__main__":
    node = Node('127.0.0.1', 5000, '127.0.0.1', 5001)
    node.start()
