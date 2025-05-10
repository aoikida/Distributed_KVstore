#!/bin/bash

# Kill any previous node processes
pkill -f './node1'
pkill -f './node2'

# Start both nodes in the background
./node1 &
NODE1_PID=$!
sleep 1
./node2 &
NODE2_PID=$!
sleep 1

pass() { echo -e "\033[32mPASS:\033[0m $1"; }
fail() { echo -e "\033[31mFAIL:\033[0m $1"; }

# Test 1: SET on node1, GET on node2
echo "SET A 1" | nc localhost 5008 > /dev/null
sleep 1
RESULT=$(echo "GET A" | nc localhost 5009)
if [ "$RESULT" = "1" ]; then
    pass "SET A=1 on node1, GET on node2"
else
    fail "SET A=1 on node1, GET on node2 (expected '1', got '$RESULT')"
fi

# Wait for anti-entropy to run (5 seconds interval)
echo "Waiting for anti-entropy synchronization..."
sleep 6

# Test 2: SET on node2, GET on node1
echo "SET B 2" | nc localhost 5009 > /dev/null
sleep 1
RESULT=$(echo "GET B" | nc localhost 5008)
if [ "$RESULT" = "2" ]; then
    pass "SET B=2 on node2, GET on node1"
else
    fail "SET B=2 on node2, GET on node1 (expected '2', got '$RESULT')"
fi

# Wait for anti-entropy to run again
echo "Waiting for anti-entropy synchronization..."
sleep 6

# Test 3: GET on node1 for key A (should still be 1)
RESULT=$(echo "GET A" | nc localhost 5008)
if [ "$RESULT" = "1" ]; then
    pass "GET A on node1 after anti-entropy"
else
    fail "GET A on node1 after anti-entropy (expected '1', got '$RESULT')"
fi

# Cleanup
kill $NODE1_PID
kill $NODE2_PID
