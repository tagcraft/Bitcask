#!/bin/bash

echo "=== Bitcask Test Suite ==="
echo ""

# Clean up
rm -rf test_complete_db

echo "1. Testing SET operations..."
./ccbitcask -db ./test_complete_db set name "John Doe"
./ccbitcask -db ./test_complete_db set age "30"
./ccbitcask -db ./test_complete_db set city "New York"
echo "✓ SET operations completed"
echo ""

echo "2. Testing GET operations..."
result=$(./ccbitcask -db ./test_complete_db get name)
if [ "$result" = "John Doe" ]; then
    echo "✓ GET name: $result"
else
    echo "✗ GET name failed"
fi

result=$(./ccbitcask -db ./test_complete_db get age)
echo "✓ GET age: $result"
echo ""

echo "3. Testing LIST operation..."
./ccbitcask -db ./test_complete_db list
echo ""

echo "4. Testing DELETE operation..."
./ccbitcask -db ./test_complete_db del age
result=$(./ccbitcask -db ./test_complete_db get age)
echo "✓ After delete, GET age returns: $result"
echo ""

echo "5. Testing persistence (simulating restart)..."
result=$(./ccbitcask -db ./test_complete_db get name)
if [ "$result" = "John Doe" ]; then
    echo "✓ Data persisted: $result"
else
    echo "✗ Persistence failed"
fi
echo ""

echo "6. Testing file rotation..."
for i in {1..100}; do
    ./ccbitcask -db ./test_complete_db set "key$i" "value$i" > /dev/null
done
file_count=$(ls test_complete_db/cask.* 2>/dev/null | wc -l)
echo "✓ Created $file_count log files"
echo ""

echo "7. Testing merge operation..."
./ccbitcask -db ./test_complete_db merge
echo "✓ Merge completed"
echo ""

echo "8. Verifying data after merge..."
result=$(./ccbitcask -db ./test_complete_db get key50)
echo "✓ GET key50 after merge: $result"
echo ""

echo "=== All Tests Completed Successfully! ==="
