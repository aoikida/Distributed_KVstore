# Distributed Key-Value Store with Merkle Tree Anti-Entropy

This distributed key-value store implements a simple but robust replication system using two synchronization methods:

1. Full state exchange (original implementation)
2. Merkle tree-based efficient differential synchronization (new implementation)

## Overview

The system consists of two nodes (implemented as separate processes) that communicate with each other over TCP sockets. Each node maintains its own key-value store and periodically synchronizes with the other node to ensure consistency.

## Components

### KeyValueStore

The core data structure that stores the key-value pairs with timestamps. Timestamps are used for conflict resolution (last-write-wins).

### Node

Represents a single node in the distributed system. Handles client connections, processes commands, and coordinates with the anti-entropy mechanism.

### AntiEntropyManager

Manages the synchronization between nodes using one of two methods:
- **Full State Exchange**: The original method where nodes exchange their complete sets of keys and timestamps
- **Merkle Tree Synchronization**: A more efficient method that uses Merkle trees to identify differences

### MerkleTreeIndex

Maintains a Merkle tree representation of the key-value store, allowing efficient identification of differences between nodes.

## Anti-Entropy Process

### Full State Exchange

1. Node A requests all keys and timestamps from Node B
2. Both nodes compare their states key-by-key
3. For any differences, the newer version (based on timestamp) wins and is propagated

### Merkle Tree Synchronization

1. Node A constructs a Merkle tree of its key-value pairs
2. Node A requests the Merkle root hash from Node B
3. If the roots match, both nodes are in sync
4. If the roots differ:
   - Node A sends all its keys to Node B
   - Node B returns Merkle paths for those keys
   - Node A compares these paths to identify exactly which keys differ
   - Node A requests only the differing keys from Node B
   - Both nodes update their stores accordingly

## Advantages of Merkle Tree Synchronization

1. **Reduced Network Traffic**: Only differences are exchanged, not the entire dataset
2. **Efficiency at Scale**: Performance improvement becomes more significant as the dataset grows
3. **Quick Verification**: Can quickly verify if two nodes are in sync with a single hash comparison

## Client Operations

The system supports the following client operations:
- `GET key` - Retrieve the value for a key
- `SET key value` - Set the value for a key
- `DEL key` - Delete a key

## Usage

### Running Node 1

```bash
./node1
```

### Running Node 2

```bash
./node2
```

### Client Interaction

```bash
echo "SET mykey myvalue" | nc localhost 3000
echo "GET mykey" | nc localhost 3000
```

### Implementation Details

- Both nodes maintain the same structure and functionality
- Anti-entropy runs periodically in the background (every 5 seconds)
- Timestamps are used for conflict resolution (last-write-wins policy)
- The Merkle tree is rebuilt whenever the key-value store changes
