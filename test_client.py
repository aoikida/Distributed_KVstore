#!/usr/bin/env python3
"""
Test client for distributed key-value store.

This client provides a simple interface to interact with the distributed KV store
and includes automated test scenarios to validate system behavior.
"""

import socket
import time
import sys
import argparse
import threading
import random
import string

# Default node addresses
NODE1 = ('localhost', 5008)
NODE2 = ('localhost', 5009)

class KVClient:
    """Client for interacting with the distributed key-value store."""
    
    def __init__(self, host='localhost', port=5008, timeout=2):
        """Initialize the client with specified host and port."""
        self.host = host
        self.port = port
        self.timeout = timeout
    
    def send_command(self, command):
        """Send a command to the KV store and return the response."""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.settimeout(self.timeout)
                sock.connect((self.host, self.port))
                sock.sendall(command.encode())
                response = sock.recv(1024).decode().strip()
                return response
        except socket.error as e:
            print(f"Socket error: {e}")
            return None
    
    def set(self, key, value):
        """Set a key-value pair."""
        return self.send_command(f"SET {key} {value}")
    
    def get(self, key):
        """Get the value for a key."""
        return self.send_command(f"GET {key}")
    
    def delete(self, key):
        """Delete a key."""
        return self.send_command(f"DEL {key}")


class TestRunner:
    """Runs automated tests against the distributed KV store."""
    
    def __init__(self):
        """Initialize test clients for both nodes."""
        self.client1 = KVClient(host=NODE1[0], port=NODE1[1])
        self.client2 = KVClient(host=NODE2[0], port=NODE2[1])
        self.results = {'passed': 0, 'failed': 0}
    
    def _random_key(self, prefix="", length=8):
        """Generate a random key for testing."""
        chars = string.ascii_letters + string.digits
        return prefix + ''.join(random.choice(chars) for _ in range(length))
    
    def _assert(self, condition, message):
        """Assert a condition and record the result."""
        if condition:
            print(f"✅ PASS: {message}")
            self.results['passed'] += 1
        else:
            print(f"❌ FAIL: {message}")
            self.results['failed'] += 1
    
    def test_basic_operations(self):
        """Test basic SET, GET, DEL operations."""
        print("\n=== Testing Basic Operations ===")
        
        test_key = self._random_key("basic_")
        test_val = "test_value"
        
        # Test SET
        result = self.client1.set(test_key, test_val)
        self._assert(result == "OK", f"SET operation returned: {result}")
        
        # Test GET
        result = self.client1.get(test_key)
        self._assert(result == test_val, f"GET operation returned: {result}")
        
        # Test DEL
        result = self.client1.delete(test_key)
        self._assert(result == "OK", f"DEL operation returned: {result}")
        
        # Verify deletion
        result = self.client1.get(test_key)
        self._assert(result == "" or "not found" in result.lower(), 
                   f"GET after DELETE returned: {result}")
    
    def test_anti_entropy(self):
        """Test anti-entropy synchronization between nodes."""
        print("\n=== Testing Anti-Entropy Synchronization ===")
        
        test_key = self._random_key("sync_")
        test_val = "sync_value"
        
        # Set a key on node 1
        self.client1.set(test_key, test_val)
        print(f"Set {test_key}={test_val} on Node 1")
        
        # Wait for anti-entropy to propagate (5 seconds in the implementation)
        print("Waiting for anti-entropy synchronization (6 seconds)...")
        time.sleep(6)
        
        # Verify the value is on node 2
        result = self.client2.get(test_key)
        self._assert(result == test_val, 
                   f"Anti-entropy sync: Node 2 returned {result} for key {test_key}")
    
    def test_conflict_resolution(self):
        """Test conflict resolution with concurrent updates."""
        print("\n=== Testing Conflict Resolution ===")
        
        test_key = self._random_key("conflict_")
        val1 = "original_value"
        val2 = "updated_value"
        
        # Set initial value on node 1
        self.client1.set(test_key, val1)
        print(f"Set {test_key}={val1} on Node 1")
        
        # Brief pause to ensure different timestamps
        time.sleep(1)
        
        # Set conflicting value on node 2
        self.client2.set(test_key, val2)
        print(f"Set {test_key}={val2} on Node 2")
        
        # Wait for anti-entropy
        print("Waiting for anti-entropy synchronization (6 seconds)...")
        time.sleep(6)
        
        # Check both nodes - they should have the most recent value
        val_on_node1 = self.client1.get(test_key)
        val_on_node2 = self.client2.get(test_key)
        
        print(f"Value on Node 1: {val_on_node1}")
        print(f"Value on Node 2: {val_on_node2}")
        
        self._assert(val_on_node1 == val2, 
                   f"Conflict resolution: Node 1 has {val_on_node1} instead of {val2}")
        self._assert(val_on_node2 == val2, 
                   f"Conflict resolution: Node 2 has {val_on_node2} instead of {val2}")
    
    def test_bidirectional_sync(self):
        """Test bidirectional synchronization."""
        print("\n=== Testing Bidirectional Synchronization ===")
        
        key1 = self._random_key("bi1_")
        key2 = self._random_key("bi2_")
        val1 = "value_from_node1"
        val2 = "value_from_node2"
        
        # Set different keys on different nodes
        self.client1.set(key1, val1)
        print(f"Set {key1}={val1} on Node 1")
        
        self.client2.set(key2, val2)
        print(f"Set {key2}={val2} on Node 2")
        
        # Wait for anti-entropy
        print("Waiting for anti-entropy synchronization (6 seconds)...")
        time.sleep(6)
        
        # Check both keys on both nodes
        n1k1 = self.client1.get(key1)  # Node 1, Key 1
        n1k2 = self.client1.get(key2)  # Node 1, Key 2
        n2k1 = self.client2.get(key1)  # Node 2, Key 1
        n2k2 = self.client2.get(key2)  # Node 2, Key 2
        
        self._assert(n1k1 == val1, f"Node 1 has {key1}={n1k1}")
        self._assert(n1k2 == val2, f"Node 1 has {key2}={n1k2}")
        self._assert(n2k1 == val1, f"Node 2 has {key1}={n2k1}")
        self._assert(n2k2 == val2, f"Node 2 has {key2}={n2k2}")
    
    def test_concurrent_updates(self):
        """Test concurrent updates to the same key."""
        print("\n=== Testing Concurrent Updates ===")
        
        test_key = self._random_key("concurrent_")
        num_updates = 10
        node1_updates = 0
        node2_updates = 0
        
        def update_from_node1():
            nonlocal node1_updates
            for i in range(num_updates):
                val = f"node1_update_{i}"
                self.client1.set(test_key, val)
                node1_updates += 1
                time.sleep(0.2)  # Small delay between updates
        
        def update_from_node2():
            nonlocal node2_updates
            for i in range(num_updates):
                val = f"node2_update_{i}"
                self.client2.set(test_key, val)
                node2_updates += 1
                time.sleep(0.2)  # Small delay between updates
        
        # Start concurrent update threads
        thread1 = threading.Thread(target=update_from_node1)
        thread2 = threading.Thread(target=update_from_node2)
        
        thread1.start()
        thread2.start()
        
        thread1.join()
        thread2.join()
        
        # Wait for anti-entropy to complete
        print("Waiting for anti-entropy synchronization after concurrent updates...")
        time.sleep(6)
        
        # Check that both nodes have the same value
        val_on_node1 = self.client1.get(test_key)
        val_on_node2 = self.client2.get(test_key)
        
        print(f"Final value on Node 1: {val_on_node1}")
        print(f"Final value on Node 2: {val_on_node2}")
        
        self._assert(val_on_node1 == val_on_node2, 
                    "Concurrent updates: nodes have different values")
        
        self._assert(val_on_node1.startswith("node") and val_on_node2.startswith("node"),
                    "Concurrent updates: final value is not from either node")
        
        print(f"Made {node1_updates} updates from Node 1 and {node2_updates} updates from Node 2")
    
    def run_all_tests(self):
        """Run all test cases."""
        self.test_basic_operations()
        self.test_anti_entropy()
        self.test_conflict_resolution()
        self.test_bidirectional_sync()
        self.test_concurrent_updates()
        
        print("\n=== Test Summary ===")
        print(f"Passed: {self.results['passed']}")
        print(f"Failed: {self.results['failed']}")
        
        return self.results['failed'] == 0


def check_node_availability():
    """Check if the KV store nodes are running."""
    client1 = KVClient(host=NODE1[0], port=NODE1[1])
    client2 = KVClient(host=NODE2[0], port=NODE2[1])
    
    # Try a simple command to check availability
    test_key = "availability_check"
    
    print("Checking node availability...")
    
    node1_available = client1.set(test_key, "test") is not None
    node2_available = client2.set(test_key, "test") is not None
    
    if node1_available and node2_available:
        print("Both nodes are available.")
        return True
    
    if not node1_available:
        print(f"Node 1 (localhost:{NODE1[1]}) is not responding.")
    
    if not node2_available:
        print(f"Node 2 (localhost:{NODE2[1]}) is not responding.")
    
    print("\nPlease ensure both nodes are running:")
    print("  ./node1  # in one terminal")
    print("  ./node2  # in another terminal")
    
    return False


def interactive_mode():
    """Run an interactive client session."""
    parser = argparse.ArgumentParser(description="Connect to a specific KV store node")
    parser.add_argument('--node', type=int, choices=[1, 2], default=1,
                        help="Node to connect to (1 or 2), default is 1")
    
    args = parser.parse_args()
    node = NODE1 if args.node == 1 else NODE2
    
    client = KVClient(host=node[0], port=node[1])
    print(f"Connected to Node {args.node} at {node[0]}:{node[1]}")
    print("Type 'help' for available commands or 'exit' to quit.")
    
    while True:
        try:
            cmd = input(f"kv-client({args.node})> ").strip()
            
            if cmd.lower() in ('exit', 'quit'):
                break
            
            if cmd.lower() == 'help':
                print("\nAvailable commands:")
                print("  SET <key> <value>  - Store a key-value pair")
                print("  GET <key>          - Retrieve a value by key")
                print("  DEL <key>          - Delete a key-value pair")
                print("  exit                - Exit the client")
                continue
            
            if not cmd:
                continue
            
            parts = cmd.split()
            if not parts:
                continue
            
            command = parts[0].upper()
            
            if command == 'SET' and len(parts) >= 3:
                key = parts[1]
                value = ' '.join(parts[2:])
                response = client.set(key, value)
                print(f"Response: {response}")
            
            elif command == 'GET' and len(parts) == 2:
                key = parts[1]
                response = client.get(key)
                print(f"Response: {response}")
            
            elif command == 'DEL' and len(parts) == 2:
                key = parts[1]
                response = client.delete(key)
                print(f"Response: {response}")
            
            else:
                print("Invalid command or syntax. Type 'help' for available commands.")
        
        except KeyboardInterrupt:
            print("\nExiting...")
            break
        
        except Exception as e:
            print(f"Error: {e}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "test":
        # Run automated tests
        if check_node_availability():
            runner = TestRunner()
            success = runner.run_all_tests()
            sys.exit(0 if success else 1)
        else:
            sys.exit(1)
    else:
        # Run interactive mode
        if check_node_availability():
            interactive_mode()
        else:
            sys.exit(1)
