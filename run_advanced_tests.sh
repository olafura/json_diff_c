#!/bin/bash
# Run advanced testing suite for json_diff

echo "JSON Diff Advanced Testing Suite"
echo "================================="
echo

# Run basic tests first
echo "1. Running basic test suite..."
make test
if [ $? -ne 0 ]; then
    echo "❌ Basic tests failed!"
    exit 1
fi
echo "✅ Basic tests passed!"
echo

# Run property tests (these may find issues)
echo "2. Running property tests..."
echo "   (These may reveal issues with specific edge cases)"
./builddir/test_properties
prop_result=$?
if [ $prop_result -ne 0 ]; then
    echo "❌ Property tests found issues!"
    echo "   This indicates there may be bugs that need investigation."
else
    echo "✅ Property tests passed!"
fi
echo

# Run quick generative tests
echo "3. Running quick generative tests (100 iterations)..."
./builddir/test_generative --tests 100
gen_result=$?
if [ $gen_result -ne 0 ]; then
    echo "❌ Generative tests found issues!"
    echo "   This indicates potential bugs with randomly generated data."
else
    echo "✅ Quick generative tests passed!"
fi
echo

# Summary
echo "Test Summary:"
echo "============="
echo "Basic tests: ✅ PASSED"
if [ $prop_result -eq 0 ]; then
    echo "Property tests: ✅ PASSED"
else
    echo "Property tests: ❌ FAILED (needs investigation)"
fi

if [ $gen_result -eq 0 ]; then
    echo "Generative tests: ✅ PASSED"
else
    echo "Generative tests: ❌ FAILED (needs investigation)"
fi

if [ $prop_result -eq 0 ] && [ $gen_result -eq 0 ]; then
    echo
    echo "🎉 All tests passed! The implementation appears robust."
    exit 0
else
    echo
    echo "⚠️  Some advanced tests failed. This suggests there are edge cases"
    echo "   or properties that need to be addressed in the implementation."
    echo
    echo "To run more extensive testing:"
    echo "  ./builddir/test_generative --tests 1000   # More iterations"
    echo "  ./builddir/test_generative --tests 5000   # Extensive testing"
    exit 1
fi