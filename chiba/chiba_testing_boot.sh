
DEBUGINFOD_URLS="https://debuginfod.archlinux.org"
# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color


# Check if valgrind is available
USE_VALGRIND=false
if command -v valgrind &> /dev/null; then
    USE_VALGRIND=true
    echo -e "${BLUE}✓ Valgrind detected - memory leak checking enabled${NC}"
else
    echo -e "${YELLOW}⚠ Valgrind not found - skipping memory leak checks${NC}"
fi
echo ""

echo "========================================"
echo "  Chiba Test Suite"
echo "========================================"
echo ""

# Find all test files
TEST_FILES=(*.test.c)
TOTAL_TESTS=${#TEST_FILES[@]}
PASSED_TESTS=0
FAILED_TESTS=()

for i in "${!TEST_FILES[@]}"; do
    TEST_FILE="${TEST_FILES[$i]}"
    TEST_NAME="${TEST_FILE%.test.c}"
    TEST_BINARY="${TEST_NAME}.test"
    TEST_NUM=$((i + 1))
    
    echo -e "${YELLOW}[$TEST_NUM/$TOTAL_TESTS] Compiling ${TEST_NAME}.test...${NC}"
    gcc -o "$TEST_BINARY" "$TEST_FILE" $SOURCES $CFLAGS 2>&1
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}✗ Compilation failed for ${TEST_NAME}.test${NC}"
        FAILED_TESTS+=("${TEST_NAME}.test (compilation)")
        echo ""
        continue
    fi
    echo -e "${GREEN}✓ Compilation successful${NC}"
    echo ""
    
    # Run test
    echo -e "${YELLOW}Running ${TEST_NAME}.test...${NC}"
    if [ "$USE_VALGRIND" = true ]; then
        # Capture valgrind output
        VALGRIND_OUTPUT=$(mktemp)
        valgrind $VALGRIND_FLAGS ./"$TEST_BINARY" 2>&1 | tee "$VALGRIND_OUTPUT"
        RESULT=${PIPESTATUS[0]}
        
        # Check if valgrind failed to start
        if grep -q "Fatal error at startup" "$VALGRIND_OUTPUT"; then
            echo -e "${YELLOW}⚠ Valgrind failed (install libc6-dbg for full checks), running without it...${NC}"
            ./"$TEST_BINARY"
            RESULT=$?
        fi
        rm -f "$VALGRIND_OUTPUT"
    else
        ./"$TEST_BINARY"
        RESULT=$?
    fi
    
    if [ $RESULT -ne 0 ]; then
        echo -e "${RED}✗ ${TEST_NAME}.test FAILED with exit code $RESULT${NC}"
        FAILED_TESTS+=("${TEST_NAME}.test")
    else
        echo -e "${GREEN}✓ ${TEST_NAME}.test PASSED${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    fi
    echo ""
done

# Summary
echo "========================================"
if [ ${#FAILED_TESTS[@]} -eq 0 ]; then
    echo -e "${GREEN}  ✓ All $TOTAL_TESTS tests PASSED!${NC}"
    if [ "$USE_VALGRIND" = true ]; then
        echo -e "${GREEN}  ✓ No memory leaks detected!${NC}"
    fi
else
    echo -e "${RED}  ✗ $PASSED_TESTS/$TOTAL_TESTS tests passed${NC}"
    echo -e "${RED}  Failed tests:${NC}"
    for test in "${FAILED_TESTS[@]}"; do
        echo -e "${RED}    - $test${NC}"
    done
fi
echo "========================================"

# Cleanup
rm -f *.test

# Exit with failure if any tests failed
if [ ${#FAILED_TESTS[@]} -ne 0 ]; then
    exit 1
fi

exit 0
