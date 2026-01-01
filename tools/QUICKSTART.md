# Quick Start: Compile and Run CHIPS REST API Test

## Step 1: Install Dependencies

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential libcurl4-openssl-dev libcjson-dev
```

### macOS
```bash
brew install curl cjson
```

### CentOS/RHEL
```bash
sudo yum install gcc libcurl-devel
# For cJSON, you may need to compile from source or use alternative JSON library
```

## Step 2: Compile

### Option A: Simple compilation
```bash
cd tools
gcc -o chips_rest_api_test chips_rest_api_test.c -lcurl -lcjson
```

### Option B: Using Makefile
```bash
cd tools
make -f Makefile.test
```

### Option C: With specific include paths (if cJSON is in external/)
```bash
cd tools
gcc -I../external/cjson -o chips_rest_api_test chips_rest_api_test.c -lcurl -L../external/cjson -lcjson
```

## Step 3: Configure CHIPS RPC (if not already done)

Make sure your CHIPS daemon is running and configured for RPC:

**Check if chipsd is running:**
```bash
chips-cli getblockchaininfo
```

**If not configured, edit `~/.chips/chips.conf`:**
```ini
server=1
rpcuser=your_username
rpcpassword=your_password
rpcport=57776
rpcallowip=127.0.0.1
```

**Start chipsd if not running:**
```bash
chipsd &
```

## Step 4: Run the Program

### Basic usage (uses defaults):
```bash
./chips_rest_api_test
```
Default: `http://127.0.0.1:57776` with user `chipsuser` and password `chipspass`

### Specify RPC URL:
```bash
./chips_rest_api_test http://127.0.0.1:57776
```

### Specify URL and credentials:
```bash
./chips_rest_api_test http://127.0.0.1:57776 myuser mypass
```

### Example with actual credentials:
```bash
./chips_rest_api_test http://127.0.0.1:57776 chipsuser chipspass
```

## Expected Output

```
Connecting to CHIPS RPC endpoint: http://127.0.0.1:57776
RPC User: chipsuser

Calling getblockchaininfo...

Raw Response:
{"result":{"chain":"main","blocks":1234567,"headers":1234567,...},"error":null,"id":"1"}

=== CHIPS Blockchain Info ===
Chain: main
Blocks: 1234567
Headers: 1234567
Difficulty: 0.12345678
Median Time: 1234567890
Verification Progress: 1.0000
Initial Block Download: false

Full Response:
{...formatted JSON...}
```

## Troubleshooting

### Error: "curl/curl.h: No such file or directory"
```bash
sudo apt-get install libcurl4-openssl-dev  # Ubuntu/Debian
# or
brew install curl  # macOS
```

### Error: "cjson/cJSON.h: No such file or directory"
```bash
sudo apt-get install libcjson-dev  # Ubuntu/Debian
# or
brew install cjson  # macOS
```

### Error: "Connection refused"
- Make sure chipsd is running: `chips-cli getblockchaininfo`
- Check RPC port (default: 57776)
- Verify `server=1` in chips.conf

### Error: "Authentication failed"
- Verify RPC credentials match your chips.conf
- Check `rpcuser` and `rpcpassword` settings

### Error: "undefined reference to `curl_*`"
- Make sure libcurl is installed: `pkg-config --libs libcurl`
- Try: `gcc -o chips_rest_api_test chips_rest_api_test.c $(pkg-config --libs --cflags libcurl) -lcjson`

### Error: "undefined reference to `cJSON_*`"
- Install cJSON: `sudo apt-get install libcjson-dev`
- Or compile with path to cJSON if using local copy

## Verify Installation

Check if dependencies are available:
```bash
# Check curl
curl-config --version

# Check cJSON (if installed via package manager)
pkg-config --modversion libcjson 2>/dev/null || echo "cJSON not found via pkg-config"

# Check compiler
gcc --version
```

