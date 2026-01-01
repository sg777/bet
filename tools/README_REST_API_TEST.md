# CHIPS REST API Test Program

Simple C program to test CHIPS REST API calls using JSON-RPC over HTTP.

## Purpose

This is a proof-of-concept program demonstrating how to call CHIPS blockchain RPC methods using REST API (HTTP/JSON-RPC) instead of CLI commands. This serves as a reference implementation for migrating from CLI-based commands to REST API.

## Prerequisites

### Required Libraries

1. **libcurl** - HTTP client library
   - Ubuntu/Debian: `sudo apt-get install libcurl4-openssl-dev`
   - macOS: `brew install curl` (usually pre-installed)
   - CentOS/RHEL: `sudo yum install libcurl-devel`

2. **cJSON** - JSON parsing library
   - Ubuntu/Debian: `sudo apt-get install libcjson-dev`
   - macOS: `brew install cjson`
   - Or use the bundled version in `../external/cjson` if available

## Compilation

### Option 1: Using system libraries (recommended)

```bash
cd tools
gcc -o chips_rest_api_test chips_rest_api_test.c -lcurl -lcjson
```

### Option 2: Using Makefile

```bash
cd tools
make -f Makefile.test
```

### Option 3: Using local cJSON (if available in external/)

```bash
cd tools
make -f Makefile.test local-cjson
```

## Usage

### Basic Usage

```bash
./chips_rest_api_test [rpc_url] [rpc_user] [rpc_password]
```

### Examples

1. **Using defaults** (http://127.0.0.1:57776, chipsuser/chipspass):
   ```bash
   ./chips_rest_api_test
   ```

2. **Specify RPC URL only**:
   ```bash
   ./chips_rest_api_test http://127.0.0.1:57776
   ```

3. **Specify URL and credentials**:
   ```bash
   ./chips_rest_api_test http://127.0.0.1:57776 myuser mypass
   ```

4. **Remote CHIPS node**:
   ```bash
   ./chips_rest_api_test http://192.168.1.100:57776 chipsuser chipspass
   ```

## CHIPS RPC Configuration

Make sure your CHIPS daemon (`chipsd`) is configured to accept RPC connections:

**In `~/.chips/chips.conf` or your config file:**
```ini
server=1
rpcuser=your_rpc_user
rpcpassword=your_rpc_password
rpcport=57776
rpcallowip=127.0.0.1
# For remote access (use with caution):
# rpcbind=0.0.0.0
# rpcallowip=192.168.1.0/24
```

**Default CHIPS RPC port:** 57776

## What It Does

The program:
1. Connects to CHIPS RPC endpoint via HTTP
2. Sends JSON-RPC request for `getblockchaininfo` method
3. Authenticates using HTTP Basic Auth (RPC user/password)
4. Parses JSON response
5. Displays formatted blockchain information

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

## JSON-RPC Format

The program uses standard JSON-RPC 1.0 format:

```json
{
  "jsonrpc": "1.0",
  "id": "1",
  "method": "getblockchaininfo",
  "params": []
}
```

## Extending the Program

To call other RPC methods, modify the `chips_rpc_request()` call:

```c
// Example: Get balance
char *response = chips_rpc_request(url, user, pass, "getbalance", "[]");

// Example: Get new address
char *response = chips_rpc_request(url, user, pass, "getnewaddress", "[]");

// Example: Get block by hash
char *response = chips_rpc_request(url, user, pass, "getblock", 
                                   "[\"000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f\"]");
```

## Integration Notes

This demonstrates the pattern that would be used in the main codebase:

1. Replace `popen()` calls with `chips_rpc_request()` function calls
2. Replace CLI command building with JSON-RPC payload construction
3. Handle HTTP errors instead of CLI exit codes
4. Parse JSON responses directly (already using cJSON in codebase)

## Security Notes

- **Never commit RPC credentials** to version control
- Use environment variables or secure configuration files for production
- Consider using HTTPS for remote connections (requires SSL setup)
- Restrict RPC access via firewall rules (`rpcallowip`)

## Troubleshooting

### "Connection refused"
- Ensure `chipsd` is running: `chips-cli getblockchaininfo`
- Check RPC port is correct (default: 57776)
- Verify `server=1` in chips.conf

### "Authentication failed"
- Verify RPC user/password match chips.conf
- Check `rpcuser` and `rpcpassword` settings

### "cJSON not found"
- Install libcjson-dev package
- Or compile with local cJSON: `make -f Makefile.test local-cjson`

### "libcurl not found"
- Install libcurl development package
- On macOS, curl is usually pre-installed

## Next Steps

For full migration:
1. Create `poker/src/chips_rpc.c` module with reusable functions
2. Integrate with existing codebase error handling
3. Add connection pooling and retry logic
4. Support for batch RPC requests
5. Comprehensive error handling and logging

