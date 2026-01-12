# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O2
INCLUDES = -Iinclude

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = .

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Target executable
TARGET = $(BIN_DIR)/ccbitcask

# Default target
all: $(TARGET)

# Create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Link object files to create executable
$(TARGET): $(OBJECTS) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET)
	@echo "Build complete: $(TARGET)"

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	@echo "Clean complete"

# Clean and rebuild
rebuild: clean all

# Run basic tests
test: $(TARGET)
	@echo "Running basic tests..."
	@rm -rf test_db
	@./$(TARGET) -db test_db set key1 "Hello World"
	@./$(TARGET) -db test_db set key2 "Bitcask Test"
	@echo "Testing get key1:"
	@./$(TARGET) -db test_db get key1
	@echo "Testing get key2:"
	@./$(TARGET) -db test_db get key2
	@echo "Testing list:"
	@./$(TARGET) -db test_db list
	@echo "Testing delete:"
	@./$(TARGET) -db test_db del key1
	@./$(TARGET) -db test_db get key1
	@echo "Tests complete!"

# Install (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Uninstall (optional)
uninstall:
	rm -f /usr/local/bin/ccbitcask

# Help
help:
	@echo "Bitcask Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the project (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  rebuild  - Clean and build"
	@echo "  test     - Run basic functionality tests"
	@echo "  install  - Install to /usr/local/bin"
	@echo "  help     - Show this help message"

.PHONY: all clean rebuild test install uninstall help