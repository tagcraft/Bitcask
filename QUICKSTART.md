# Bitcask Quick Start Guide

## Step 1: Build the Project

```bash
# In your GitHub Codespace terminal
make clean
make
```

You should see:
```
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -Iinclude -c src/hash_index.cpp -o obj/hash_index.o
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -Iinclude -c src/log_file.cpp -o obj/log_file.o
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -Iinclude -c src/bitcask.cpp -o obj/bitcask.o
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -Iinclude -c src/main.cpp -o obj/main.o
g++ -std=c++17 -Wall -Wextra -pedantic -O2 obj/hash_index.o obj/log_file.o obj/bitcask.o obj/main.o -o ./ccbitcask
Build complete: ./ccbitcask
```

## Step 2: Run Basic Tests

```bash
make test
```

## Step 3: Manual Testing

### Create a database and add data
```bash
./ccbitcask -db ./mydb set user:1 "Alice"
./ccbitcask -db ./mydb set user:2 "Bob"
./ccbitcask -db ./mydb set user:3 "Charlie"
```

### Read data
```bash
./ccbitcask -db ./mydb get user:1
# Output: Alice

./ccbitcask -db ./mydb get user:2
# Output: Bob
```

### List all keys
```bash
./ccbitcask -db ./mydb list
# Output:
# user:1
# user:2
# user:3
```

### Delete a key
```bash
./ccbitcask -db ./mydb del user:2
./ccbitcask -db ./mydb get user:2
# Output: (nil)
```

### Test persistence
```bash
# Set some data
./ccbitcask -db ./testdb set persistent "This survives restart"

# Check the database directory
ls -lh testdb/

# Restart (new process) and get the data
./ccbitcask -db ./testdb get persistent
# Output: This survives restart
```

## Step 4: Test File Rotation

```bash
# Create a script to add many entries
for i in {1..100}; do
    ./ccbitcask -db ./bigdb set key$i "value$i"
done

# Check how many log files were created
ls -lh bigdb/
# You should see multiple cask.* files

# Verify data is still accessible
./ccbitcask -db ./bigdb get key50
# Output: value50
```

## Step 5: Test Merge (Compaction)

```bash
# Add data
for i in {1..50}; do
    ./ccbitcask -db ./mergedb set key$i "original_value_$i"
done

# Update some keys (creates duplicate entries in log)
for i in {1..25}; do
    ./ccbitcask -db ./mergedb set key$i "updated_value_$i"
done

# Delete some keys
for i in {26..30}; do
    ./ccbitcask -db ./mergedb del key$i
done

# Check file sizes before merge
du -sh mergedb/

# Run merge
./ccbitcask -db ./mergedb merge

# Check file sizes after merge (should be smaller)
du -sh mergedb/

# Verify data integrity
./ccbitcask -db ./mergedb get key1
# Output: updated_value_1

./ccbitcask -db ./mergedb get key26
# Output: (nil)  (deleted)

./ccbitcask -db ./mergedb get key40
# Output: original_value_40
```

## Step 6: Inspect Binary Files

Use `xxd` to examine the log files:

```bash
xxd testdb/cask.0 | head -20
```

You'll see the binary format with CRC, timestamps, key sizes, etc.

## Common Issues and Solutions

### Issue: "Permission denied"
```bash
chmod +x ccbitcask
```

### Issue: "directory not found"
The database directory is created automatically on first use.

### Issue: Compilation errors
Make sure you have g++ with C++17 support:
```bash
g++ --version
# Should be 7.0 or higher
```

## Next Steps

1. **Add more entries** to test file rotation thoroughly
2. **Examine binary files** with xxd to understand the format
3. **Test crash recovery** by killing the process mid-write
4. **Implement additional features** like:
   - Statistics (number of keys, disk usage)
   - Batch operations
   - Key expiration (TTL)
   - Redis RESP protocol server

## Understanding the Files

```bash
# Log files contain actual data
mydb/cask.0
mydb/cask.1
...

# Hint files speed up startup
mydb/cask.0.hint
mydb/cask.1.hint
...
```

## Performance Testing

```bash
# Measure write performance
time for i in {1..1000}; do
    ./ccbitcask -db ./perfdb set key$i "value_$i" > /dev/null
done

# Measure read performance
time for i in {1..1000}; do
    ./ccbitcask -db ./perfdb get key$i > /dev/null
done
```

## Debugging

Add print statements in the code to trace execution:

```cpp
std::cerr << "DEBUG: Opening file: " << filepath_ << std::endl;
```

Then recompile and run to see the debug output.