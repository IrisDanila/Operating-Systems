#!/bin/bash

MALICIOUS_DIR_NAME="MaliciousFiles"

# Function to check for malicious words
check_malicious_words() {
    local file="$1"
    local malicious_words=("corrupted" "attack" "risk" "malicious" "malware" "virus")

    while IFS= read -r line; do
        for word in "${malicious_words[@]}"; do
            if grep -q -i -w "$word" <<< "$line"; then
                echo "Malicious word found: $word"
                return 1 # Found malicious word
            fi
        done
    done < "$file"

    return 0 # No malicious word found
}

# Check file permissions and run checks if no permissions
check_permissions_and_run_checks() {
    local file="$1"

    # Check for read permissions
    if [ ! -r "$file" ]; then
        echo "File $file has no read permissions."
    fi

    # Check for write permissions
    if [ ! -w "$file" ]; then
        echo "File $file has no write permissions."
    fi
    
    chmod ugo+rw "$file"

    # Get file statistics
    num_lines=$(wc -l < "$file")
    num_words=$(wc -w < "$file")
    num_chars=$(wc -c < "$file")

    # Output file statistics to stdout
    echo "Number of lines: $num_lines"
    echo "Number of words: $num_words"
    echo "Number of characters: $num_chars"

    ##### Check for malicious criteria
    if [ "$num_lines" -lt 3 ]; then
        echo "File $file has less than 3 lines, considered malicious."
        chmod ugo-rw "$file"
        exit 2 # Malicious file found
    fi

    if [ "$num_words" -gt 1000 ]; then
        echo "File $file has more than 1000 words, considered malicious."
        chmod ugo-rw "$file"
        exit 2 # Malicious file found
    fi

    if [ "$num_chars" -gt 2000 ]; then
        echo "File $file has more than 2000 characters, considered malicious."
        chmod ugo-rw "$file"
        exit 2 # Malicious file found
    fi

    if grep -q -P '[^\x00-\x7F]' "$file"; then
        echo "File $file contains non-ASCII characters, considered malicious."
        chmod ugo-rw "$file"
        exit 2 # Malicious file found
    fi
}



# Main script
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <file>"
    exit 1
fi

file="$1"

if [ ! -f "$file" ]; then
    echo "Error: File $file does not exist."
    exit 1
fi

check_permissions_and_run_checks "$file"
