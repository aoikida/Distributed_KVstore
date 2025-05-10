# Distributed Key-Value Store with Anti-Entropy Synchronization

A simple distributed key-value store with two nodes that communicate via TCP sockets and use anti-entropy for eventual consistency.

## Architecture

The system consists of two main components:

1. **Node System**: Each node can process client requests (get, set, delete) and propagate updates to the other node.
2. **Anti-Entropy Mechanism**: Ensures eventual consistency between nodes by periodically synchronizing data.

### Components

- `KeyValueStore`: The core data structure that stores key-value pairs with timestamps
- `Node`: Handles client requests and communication between nodes
- `AntiEntropyManager`: Manages periodic synchronization to ensure consistency

## Features

- **Basic Operations**: GET, SET, DEL operations for key-value pairs
- **Immediate Propagation**: When a client updates a key, the change is immediately propagated to the other node
- **Anti-Entropy Synchronization**: Periodically reconciles differences between nodes to ensure consistency
- **Conflict Resolution**: Uses timestamps to resolve conflicts (last-write-wins)
- **Fault Tolerance**: Retries when communication between nodes fails

## How It Works

### Basic Operations

1. **GET**: Retrieves a value for a given key
2. **SET**: Sets a value for a key with a timestamp
3. **DEL**: Deletes a key-value pair with a timestamp

### Propagation

When a client updates a value (SET/DEL), the node that receives the request:
1. Updates its local store
2. Propagates the update to the peer node with the original timestamp

### Anti-Entropy Process

Every 5 seconds, each node:
1. Fetches all keys from its peer
2. Sends all its keys to the peer
3. This ensures that even if immediate propagation fails, the system will eventually become consistent

### Conflict Resolution

When both nodes update the same key, the update with the most recent timestamp wins. This ensures that the system converges to a consistent state.

## How to Run

### Prerequisites

- C++17 compatible compiler
- Boost libraries
- CMake

### Building the Project

```bash
cmake .
make
```

### Running the Nodes

```bash
# In one terminal
./node1

# In another terminal
./node2
```

### Running Tests

The test script verifies functionality by testing basic operations, anti-entropy synchronization, conflict resolution, and bidirectional sync:

```bash
./test_kvstore.sh
```

## Implementation Details

### Node Communication

Nodes communicate using TCP sockets via Boost.Asio. A node can:
- Accept client connections and process commands
- Connect to another node to propagate updates
- Perform periodic anti-entropy synchronization

### Timestamps

Each key-value pair has an associated timestamp (milliseconds since epoch) used for:
- Determining which update is more recent
- Resolving conflicts between nodes

## Limitations and Future Improvements

- Currently limited to two nodes
- No persistent storage (in-memory only)
- No partition tolerance (not fully CAP compliant)
- Could be extended to use Merkle trees for more efficient synchronization
- Could implement a more sophisticated conflict resolution mechanism (e.g., vector clocks)

## License

This project is open source and available under the MIT License.
