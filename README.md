# Bitcask# Bitcask Key-Value Store in C++

A high-performance, log-structured hash table implementation based on the Bitcask paper by Basho Technologies.

## Overview

Bitcask is an append-only, log-structured storage engine that provides:
- **Fast reads and writes**: O(1) lookups via in-memory hash index
- **Crash recovery**: CRC checksums for data integrity
- **Efficient compaction**: Background merge process to reclaim space
- **Simple design**: Easy to understand and maintain

## Architecture

### Components

1. **Log Files**: Append-only files containing key-value entries
2. **Hash Index**: In-memory index mapping keys to file positions
3. **Merge Process**: Background compaction to remove deleted/stale data
4. **Hint Files**: Accelerate startup by providing pre-built index data

### Data Format

**Log Entry Format:**
```
| CRC (4B) | Timestamp (4B) | Key Size (4B) | Value Size (4B) | Key | Value |
```

**Hash Index Entry:**
```
Key -> { file_id, value_position, value_size, timestamp }
```

## Building

```bash
make clean
make
```

## Usage

### Set a key-value pair
```bash
./ccbitcask -db ./database set mykey myvalue
```

### Get a value
```bash
./ccbitcask -db ./database get mykey
# Output: myvalue
```

### Delete a key
```bash
./ccbitcask -db ./database del mykey
```

### Run merge/compaction
```bash
./ccbitcask -db ./database merge
```

## Design Decisions

### Why Append-Only Logs?
- **Performance**: Sequential writes are faster than random writes
- **Simplicity**: No in-place updates means simpler concurrency
- **Durability**: Every write is immediately persisted

### Why In-Memory Index?
- **Speed**: O(1) lookups without disk seeks
- **Trade-off**: Memory usage scales with number of unique keys (not total data)

### CRC-32 for Integrity
- Detects corruption from crashes or disk errors
- Validates log entries during recovery

## Interview Discussion Points

### Time Complexity
- **Write**: O(1) - append to log + update hash table
- **Read**: O(1) - hash lookup + single disk seek
- **Delete**: O(1) - write tombstone + update hash table
- **Merge**: O(n) - scan all non-active files

### Space Complexity
- **Memory**: O(k) where k = number of unique keys
- **Disk**: O(n) where n = total data written (before compaction)

### Concurrency
- Single writer model (one process at a time)
- Could extend to multi-reader, single-writer with file locking

### Crash Recovery
- CRC validation ensures data integrity
- Hint files accelerate rebuild of hash index
- Incomplete writes are detected and discarded

