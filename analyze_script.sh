#!/bin/bash

# Enhanced bash-shxdumper Analysis Script
# Usage: ./analyze_script.sh <script_to_analyze> [output_directory]

set -e

SCRIPT_TO_ANALYZE="$1"
OUTPUT_DIR="${2:-/tmp/bash_analysis_$(date +%Y%m%d_%H%M%S)}"
BASH_BINARY="$(dirname "$0")/bash"

if [ -z "$SCRIPT_TO_ANALYZE" ]; then
    echo "Usage: $0 <script_to_analyze> [output_directory]"
    echo ""
    echo "Examples:"
    echo "  $0 ./suspicious_script.sh"
    echo "  $0 ./encrypted_binary /tmp/my_analysis"
    echo ""
    echo "Environment variables:"
    echo "  ANALYSIS_TIMEOUT=30    # Timeout in seconds (default: 30)"
    echo "  STRACE_ENABLED=1       # Enable strace logging (default: 1)"
    echo "  VERBOSE=1              # Verbose output (default: 0)"
    exit 1
fi

if [ ! -f "$SCRIPT_TO_ANALYZE" ]; then
    echo "Error: Script to analyze '$SCRIPT_TO_ANALYZE' not found"
    exit 1
fi

if [ ! -x "$BASH_BINARY" ]; then
    echo "Error: Enhanced bash binary not found at '$BASH_BINARY'"
    echo "Please build the enhanced bash first: make clean && make"
    exit 1
fi

# Configuration
TIMEOUT="${ANALYSIS_TIMEOUT:-30}"
STRACE_ENABLED="${STRACE_ENABLED:-1}"
VERBOSE="${VERBOSE:-0}"

# Create output directory
mkdir -p "$OUTPUT_DIR"
echo "Analysis output will be saved to: $OUTPUT_DIR"

# Setup logging environment
export LOG_FILE="$OUTPUT_DIR/bash_debug.log"
export ENABLE_FD_PROTECTION=1
export PREVENT_UNLINK=1
export LOG_ALL_READS=1
export LOG_ALL_WRITES=1

# Copy the script to analyze to output directory for safety
SCRIPT_COPY="$OUTPUT_DIR/$(basename "$SCRIPT_TO_ANALYZE")"
cp "$SCRIPT_TO_ANALYZE" "$SCRIPT_COPY"
chmod +x "$SCRIPT_COPY"

echo "=== Enhanced bash-shxdumper Analysis ==="
echo "Target script: $SCRIPT_TO_ANALYZE"
echo "Output directory: $OUTPUT_DIR"
echo "Timeout: ${TIMEOUT}s"
echo "Enhanced bash: $BASH_BINARY"
echo ""

# File information
echo "=== File Information ==="
file "$SCRIPT_TO_ANALYZE"
ls -la "$SCRIPT_TO_ANALYZE"
echo ""

# Try UPX extraction first
if command -v upx >/dev/null 2>&1; then
    echo "=== UPX Extraction Attempt ==="
    upx -d "$SCRIPT_COPY" -o "$OUTPUT_DIR/upx_extracted" 2>&1 || echo "Standard UPX extraction failed (expected for custom UPX)"
    echo ""
fi

# Start background monitoring
echo "=== Starting Analysis ==="
echo "Logs will be written to: $LOG_FILE"

# Create execution wrapper
cat > "$OUTPUT_DIR/execute_target.sh" << EOF
#!/bin/bash
cd "$OUTPUT_DIR"
echo "Executing target script with enhanced bash..."
echo "Working directory: \$(pwd)"
echo "PID: \$$"
echo "Time: \$(date)"
exec "./$(basename "$SCRIPT_TO_ANALYZE")"
EOF
chmod +x "$OUTPUT_DIR/execute_target.sh"

# Execute with enhanced monitoring
if [ "$STRACE_ENABLED" = "1" ] && command -v strace >/dev/null 2>&1; then
    echo "Running with strace and enhanced bash..."
    timeout "${TIMEOUT}s" strace -f -e trace=all -s 8192 -o "$OUTPUT_DIR/strace.log" \
        "$BASH_BINARY" "$OUTPUT_DIR/execute_target.sh" \
        2>&1 | tee "$OUTPUT_DIR/execution_output.log" || true
else
    echo "Running with enhanced bash only..."
    timeout "${TIMEOUT}s" "$BASH_BINARY" "$OUTPUT_DIR/execute_target.sh" \
        2>&1 | tee "$OUTPUT_DIR/execution_output.log" || true
fi

echo ""
echo "=== Analysis Complete ==="

# Summary
echo "=== Analysis Summary ==="
echo "Files created in $OUTPUT_DIR:"
ls -la "$OUTPUT_DIR/"

echo ""
echo "=== Log Analysis ==="
if [ -f "$LOG_FILE" ]; then
    echo "Enhanced bash log entries: $(wc -l < "$LOG_FILE")"
    echo ""
    echo "Most frequent log types:"
    grep -o '\[.*\].*\[.*\].*\[.*\].*\[\w\+\]' "$LOG_FILE" | \
        awk -F'[\\[\\]]' '{print $8}' | sort | uniq -c | sort -nr | head -10
    echo ""
    echo "Key operations detected:"
    grep -E '\[(CMD|EXEC|SYSCALL|FILE|FD)\]' "$LOG_FILE" | head -20
fi

if [ -f "$OUTPUT_DIR/strace.log" ]; then
    echo ""
    echo "Strace log size: $(wc -l < "$OUTPUT_DIR/strace.log") lines"
    echo "System calls with memfd:"
    grep -c "memfd_create" "$OUTPUT_DIR/strace.log" 2>/dev/null || echo "0"
    echo "Unlink attempts:"
    grep -c "unlink" "$OUTPUT_DIR/strace.log" 2>/dev/null || echo "0"
fi

# Look for extracted content
echo ""
echo "=== Extracted Content ==="
find "$OUTPUT_DIR" -name "*.bin" -o -name "*extracted*" -o -name "*dump*" | \
while read -r file; do
    echo "Found: $file ($(wc -c < "$file") bytes)"
    echo "  Type: $(file "$file")"
done

# Search for interesting strings in logs
echo ""
echo "=== Interesting Findings ==="
if [ -f "$LOG_FILE" ]; then
    echo "URLs/IPs found:"
    grep -oE 'https?://[^[:space:]]+|[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}' "$LOG_FILE" 2>/dev/null | sort -u | head -10
    
    echo ""
    echo "File operations:"
    grep '\[FILE\]' "$LOG_FILE" | head -10
    
    echo ""
    echo "Protected operations:"
    grep -E '(PROTECT|BLOCK)' "$LOG_FILE" | head -10
fi

echo ""
echo "=== Recommendations ==="
echo "1. Review the detailed logs in $OUTPUT_DIR/"
echo "2. Check execution_output.log for program output"
echo "3. Analyze strace.log for system call patterns"
echo "4. Look for any extracted binaries or scripts"
echo "5. Monitor network connections if URLs were found"

if [ "$VERBOSE" = "1" ]; then
    echo ""
    echo "=== Verbose Output ==="
    echo "Enhanced bash log (last 50 lines):"
    tail -50 "$LOG_FILE" 2>/dev/null || echo "No bash log available"
fi

echo ""
echo "Analysis saved to: $OUTPUT_DIR"