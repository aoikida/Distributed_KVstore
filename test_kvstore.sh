#!/bin/bash

# Test script for distributed key-value store with anti-entropy synchronization

# Colors for better readability
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Starting test for distributed key-value store...${NC}"

# Function to check if a process is running on a specific port
check_port() {
    lsof -i:$1 > /dev/null 2>&1
    return $?
}

# Function to wait for a node to start
wait_for_node() {
    local port=$1
    local max_attempts=10
    local attempt=1
    
    echo -n "Waiting for node on port $port to start"
    
    while [ $attempt -le $max_attempts ]; do
        echo -n "."
        sleep 1
        if check_port $port; then
            echo -e " ${GREEN}ONLINE${NC}"
            return 0
        fi
        attempt=$((attempt+1))
    done
    
    echo -e " ${RED}FAILED${NC}"
    echo "Node on port $port failed to start within the timeout period."
    return 1
}

# Function to kill processes using specific ports
kill_node() {
    local port=$1
    local pid=$(lsof -t -i:$port 2>/dev/null)
    
    if [ -n "$pid" ]; then
        echo "Terminating node on port $port (PID: $pid)..."
        kill -9 $pid
    fi
}

# Function to test basic key-value operations
test_basic_operations() {
    echo -e "\n${YELLOW}Testing basic key-value operations...${NC}"
    
    # Test SET operation
    result=$(echo "SET testkey testvalue" | nc -w 1 localhost 5008)
    if [ "$result" == "OK" ]; then
        echo -e "${GREEN}✓${NC} SET operation successful"
    else
        echo -e "${RED}✗${NC} SET operation failed: $result"
    fi
    
    # Test GET operation
    result=$(echo "GET testkey" | nc -w 1 localhost 5008)
    if [ "$result" == "testvalue" ]; then
        echo -e "${GREEN}✓${NC} GET operation successful"
    else
        echo -e "${RED}✗${NC} GET operation failed: $result"
    fi
    
    # Test DEL operation
    result=$(echo "DEL testkey" | nc -w 1 localhost 5008)
    if [ "$result" == "OK" ]; then
        echo -e "${GREEN}✓${NC} DEL operation successful"
    else
        echo -e "${RED}✗${NC} DEL operation failed: $result"
    fi
    
    # Verify key was deleted
    result=$(echo "GET testkey" | nc -w 1 localhost 5008)
    if [ -z "$result" ] || [ "$result" == "Key not found" ]; then
        echo -e "${GREEN}✓${NC} Key deletion verified"
    else
        echo -e "${RED}✗${NC} Key still exists after deletion: $result"
    fi
}

# Function to test anti-entropy synchronization
test_anti_entropy() {
    echo -e "\n${YELLOW}Testing anti-entropy synchronization...${NC}"
    
    # Set a key on node 1
    echo "Setting key on node 1..."
    echo "SET synckey syncvalue" | nc -w 1 localhost 5008
    
    # Wait for anti-entropy process to run (takes about 5 seconds based on implementation)
    echo "Waiting for anti-entropy synchronization (6 seconds)..."
    sleep 6
    
    # Check if key exists on node 2
    result=$(echo "GET synckey" | nc -w 1 localhost 5009)
    if [ "$result" == "syncvalue" ]; then
        echo -e "${GREEN}✓${NC} Anti-entropy synchronization successful"
    else
        echo -e "${RED}✗${NC} Anti-entropy synchronization failed: Key not found on node 2"
    fi
}

# Function to test conflict resolution
test_conflict_resolution() {
    echo -e "\n${YELLOW}Testing conflict resolution...${NC}"
    
    # Set a key on node 1
    echo "Setting key 'conflictkey' on node 1..."
    echo "SET conflictkey value1" | nc -w 1 localhost 5008
    
    # Wait a moment to ensure different timestamps
    sleep 1
    
    # Set the same key with a different value on node 2
    echo "Setting the same key with a different value on node 2..."
    echo "SET conflictkey value2" | nc -w 1 localhost 5009
    
    # Wait for anti-entropy process to run
    echo "Waiting for anti-entropy synchronization (6 seconds)..."
    sleep 6
    
    # Check the value on both nodes - it should be the latest update (value2)
    result1=$(echo "GET conflictkey" | nc -w 1 localhost 5008)
    result2=$(echo "GET conflictkey" | nc -w 1 localhost 5009)
    
    echo "Value on node 1: $result1"
    echo "Value on node 2: $result2"
    
    if [ "$result1" == "value2" ] && [ "$result2" == "value2" ]; then
        echo -e "${GREEN}✓${NC} Conflict resolution successful (latest update wins)"
    else
        echo -e "${RED}✗${NC} Conflict resolution failed: Inconsistent values across nodes"
    fi
}

# Function to run bidirectional sync test
test_bidirectional_sync() {
    echo -e "\n${YELLOW}Testing bidirectional synchronization...${NC}"
    
    # Set different keys on different nodes
    echo "Setting key1 on node 1..."
    echo "SET bikey1 bivalue1" | nc -w 1 localhost 5008
    
    echo "Setting key2 on node 2..."
    echo "SET bikey2 bivalue2" | nc -w 1 localhost 5009
    
    # Wait for anti-entropy process to run
    echo "Waiting for anti-entropy synchronization (6 seconds)..."
    sleep 6
    
    # Check if both keys are available on both nodes
    result1_1=$(echo "GET bikey1" | nc -w 1 localhost 5008)
    result1_2=$(echo "GET bikey1" | nc -w 1 localhost 5009)
    result2_1=$(echo "GET bikey2" | nc -w 1 localhost 5008)
    result2_2=$(echo "GET bikey2" | nc -w 1 localhost 5009)
    
    if [ "$result1_1" == "bivalue1" ] && [ "$result1_2" == "bivalue1" ]; then
        echo -e "${GREEN}✓${NC} Key1 synchronized across both nodes"
    else
        echo -e "${RED}✗${NC} Key1 synchronization failed"
    fi
    
    if [ "$result2_1" == "bivalue2" ] && [ "$result2_2" == "bivalue2" ]; then
        echo -e "${GREEN}✓${NC} Key2 synchronized across both nodes"
    else
        echo -e "${RED}✗${NC} Key2 synchronization failed"
    fi
}

# Main test execution
echo "Building nodes..."
make clean > /dev/null
make > /dev/null

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed. Please fix the compilation errors before running tests.${NC}"
    exit 1
fi

# Clean up any existing nodes
kill_node 5008
kill_node 5009

# Start both nodes in background
echo "Starting node 1 (port 5008)..."
./node1 > node1.log 2>&1 &

echo "Starting node 2 (port 5009)..."
./node2 > node2.log 2>&1 &

# Wait for nodes to start
wait_for_node 5008 || { kill_node 5008; kill_node 5009; exit 1; }
wait_for_node 5009 || { kill_node 5008; kill_node 5009; exit 1; }

# Run all the tests
test_basic_operations
test_anti_entropy
test_conflict_resolution
test_bidirectional_sync

# Clean up
echo -e "\n${YELLOW}Cleaning up...${NC}"
kill_node 5008
kill_node 5009

echo -e "\n${GREEN}All tests completed.${NC}"
echo "Check node1.log and node2.log for details on node operation."
