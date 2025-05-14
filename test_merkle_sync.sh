#!/bin/bash

# Test script for Merkle tree-based synchronization
echo "Starting Merkle tree anti-entropy test..."

# Clean up any existing log files
rm -f node1.log node2.log

# Start node1 in the background
echo "Starting node1..."
./build/node1 > node1.log 2>&1 &
NODE1_PID=$!

# Wait for node1 to start
sleep 2

# Start node2 in the background
echo "Starting node2..."
./build/node2 > node2.log 2>&1 &
NODE2_PID=$!

# Wait for node2 to start
sleep 2

# Set some key-value pairs on node1
echo "Setting keys on node1..."
echo "SET key1 value1" | nc localhost 3000
echo "SET key2 value2" | nc localhost 3000
echo "SET key3 value3" | nc localhost 3000

# Wait for anti-entropy to run
echo "Waiting for anti-entropy to synchronize nodes..."
sleep 6

# Verify that node2 has the same keys
echo "Verifying synchronization on node2..."
echo -n "GET key1: " && echo "GET key1" | nc localhost 3001
echo -n "GET key2: " && echo "GET key2" | nc localhost 3001
echo -n "GET key3: " && echo "GET key3" | nc localhost 3001

# Set a key on node2
echo "Setting a new key on node2..."
echo "SET key4 value4" | nc localhost 3001

# Wait for anti-entropy to run
echo "Waiting for anti-entropy to synchronize nodes..."
sleep 6

# Verify that node1 has the new key
echo "Verifying node1 has the new key..."
echo -n "GET key4: " && echo "GET key4" | nc localhost 3000

# Clean up
echo "Cleaning up..."
kill $NODE1_PID
kill $NODE2_PID

echo "Test completed!"
