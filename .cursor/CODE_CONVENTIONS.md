# Code Conventions and Conventions

## Naming Conventions

### Functions
- **Player functions**: `bet_player_*` (e.g., `bet_player_join_table()`)
- **Dealer functions**: `bet_dcv_*` or `bet_bvv_*` (e.g., `bet_dcv_create_table()`)
- **Cashier functions**: `bet_process_*` or `bet_check_*` (e.g., `bet_process_payin()`)
- **Crypto functions**: `crypto_account_*` (e.g., `crypto_account_pubkey()`)
- **Game functions**: `bet_game_*` or `poker_*` (e.g., `bet_game_start()`)
- **Helper functions**: Lowercase with underscores (e.g., `jstr()`, `jint()`)

### Variables
- **Global state**: `bet_*` (e.g., `bet_player`, `bet_dcv`)
- **Local variables**: `snake_case` (e.g., `player_info`, `table_id`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `CARDS_MAXPLAYERS`, `BET_PLAYERTIMEOUT`)

### Types
- **Structs**: `snake_case` (e.g., `struct privatebet_info`, `struct crypto_account_sig`)
- **Enums**: `snake_case` (e.g., `bet_error_t`, `bet_dcv_state`)

### Files
- **Source files**: `snake_case.c` (e.g., `player.c`, `cashier.c`)
- **Header files**: `snake_case.h` (e.g., `player.h`, `cashier.h`)

## Code Style

### Indentation
- Use **tabs** for indentation (not spaces)
- Standard tab width: 8 spaces equivalent

### Braces
```c
// Function braces on new line
int32_t function_name(void)
{
    // code
}

// Control flow braces on same line
if (condition) {
    // code
} else {
    // code
}
```

### Line Length
- Prefer lines under 100 characters
- Break long lines at logical points
- Align continuation lines appropriately

### Comments
```c
// Single-line comments for brief explanations
/* Multi-line comments for detailed explanations
   spanning multiple lines */

// Function documentation
/**
 * Brief description
 * 
 * @param arg1 Description
 * @return Description
 */
```

## Memory Management

### Allocation
```c
// Always use calloc for zero-initialized memory
char *buffer = calloc(size, sizeof(char));
if (buffer == NULL) {
    dlg_error("Allocation failed");
    return ERR_MEMORY_ALLOC;
}
```

### Deallocation
```c
// Always free in reverse order of allocation
if (buffer) free(buffer);
if (json) cJSON_Delete(json);  // Use cJSON_Delete for cJSON objects
```

### Error Path Cleanup
```c
int32_t function(void)
{
    char *buf1 = NULL;
    char *buf2 = NULL;
    cJSON *json = NULL;
    
    buf1 = calloc(size1, sizeof(char));
    if (buf1 == NULL) {
        return ERR_MEMORY_ALLOC;
    }
    
    buf2 = calloc(size2, sizeof(char));
    if (buf2 == NULL) {
        free(buf1);  // Cleanup on error
        return ERR_MEMORY_ALLOC;
    }
    
    json = cJSON_Parse(data);
    if (json == NULL) {
        free(buf1);  // Cleanup on error
        free(buf2);
        return ERR_JSON_PARSING;
    }
    
    // ... use resources ...
    
    // Cleanup before return
    if (buf1) free(buf1);
    if (buf2) free(buf2);
    if (json) cJSON_Delete(json);
    return OK;
}
```

## String Handling

### Safe String Operations
```c
// NEVER use strcpy or sprintf
// BAD:
strcpy(dest, src);
sprintf(buffer, "format %s", str);

// GOOD:
strncpy(dest, src, dest_size - 1);
dest[dest_size - 1] = '\0';  // Ensure null termination

snprintf(buffer, buffer_size, "format %s", str);
```

### String Length Calculation
```c
// Calculate required size before allocation
int required = snprintf(NULL, 0, "format %s", str) + 1;
char *buffer = calloc(required, sizeof(char));
if (buffer == NULL) return ERR_MEMORY_ALLOC;
snprintf(buffer, required, "format %s", str);
```

## Error Handling

### Return Values
```c
// Always check return values
int32_t retval = function_call();
if (retval != OK) {
    dlg_error("Function failed: %s", bet_err_str(retval));
    return retval;
}
```

### Error Propagation
```c
// Propagate errors up the call stack
int32_t caller_function(void)
{
    int32_t retval = callee_function();
    if (retval != OK) {
        return retval;  // Don't modify error codes
    }
    return OK;
}
```

### Error Logging
```c
// Use appropriate log level
dlg_error("Error message: %s", details);    // Errors
dlg_warn("Warning message: %s", details);   // Warnings
dlg_info("Info message: %s", details);     // Info
dlg_debug("Debug message: %s", details);    // Debug
```

## JSON Handling

### Parsing
```c
cJSON *json = cJSON_Parse(data);
if (json == NULL) {
    dlg_error("JSON parsing failed");
    return ERR_JSON_PARSING;
}

// Use helper functions
const char *str_value = jstr(json, "key");
int32_t int_value = jint(json, "key");
double double_value = jdouble(json, "key");

// Always delete when done
cJSON_Delete(json);
```

### Creation
```c
cJSON *json = cJSON_CreateObject();
cJSON_AddStringToObject(json, "key", "value");
cJSON_AddNumberToObject(json, "number", 42);

char *json_string = cJSON_Print(json);
// ... use json_string ...
free(json_string);  // cJSON_Print allocates memory
cJSON_Delete(json);
```

## Function Design

### Function Signatures
```c
// Return error codes (int32_t)
int32_t function_name(type1 arg1, type2 arg2);

// Return pointers (NULL on error)
type *function_name(type1 arg1);

// Return boolean (true/false)
bool function_name(type1 arg1);
```

### Parameter Validation
```c
int32_t function_name(char *arg1, int32_t arg2)
{
    // Validate parameters early
    if (arg1 == NULL) {
        dlg_error("arg1 is NULL");
        return ERR_ARGS_NULL;
    }
    
    if (arg2 < 0) {
        dlg_error("arg2 is invalid: %d", arg2);
        return ERR_INVALID_DATA;
    }
    
    // ... function body ...
}
```

## Includes

### Include Order
```c
// 1. System includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 2. External library includes
#include <cjson/cJSON.h>
#include <dlg/dlg.h>

// 3. Project includes (alphabetical)
#include "bet.h"
#include "cards.h"
#include "common.h"
#include "err.h"
```

### Include Guards
```c
#ifndef HEADER_H
#define HEADER_H
// ... header content ...
#endif
```

## Constants

### Magic Numbers
```c
// BAD: Magic numbers
if (count > 9) { ... }

// GOOD: Named constants
#define MAX_PLAYERS 9
if (count > MAX_PLAYERS) { ... }
```

### Constants Location
- Game constants: `poker/include/common.h`
- Error codes: `poker/include/err.h`
- Module-specific: In module header file

## Code Organization

### File Structure
```c
/******************************************************************************
 * Copyright notice
 ******************************************************************************/

// Includes
#include "header.h"

// Forward declarations
static void helper_function(void);

// Static variables
static int32_t module_state = 0;

// Public functions
int32_t public_function(void)
{
    // ...
}

// Static/private functions
static void helper_function(void)
{
    // ...
}
```

### Module Organization
- One module per file (e.g., `player.c` for player functionality)
- Related functions grouped together
- Helper functions at the end of file

## Testing

### Test Functions
```c
// Test functions in test.c
void test_function_name(void)
{
    // Setup
    // Execute
    // Verify
    // Cleanup
}
```

### Debugging
```c
// Use dlg_debug for detailed debugging
dlg_debug("Variable value: %d", variable);

// Use conditional compilation for debug code
#ifdef DEBUG
    // Debug-only code
#endif
```

## Git Conventions

### Commit Messages
```
Type: Brief description

Detailed explanation if needed.

Files changed:
- path/to/file1.c: What changed
- path/to/file2.c: What changed
```

### Commit Types
- `Refactor:` - Code restructuring
- `Fix:` - Bug fixes
- `Feature:` - New features
- `Cleanup:` - Code cleanup
- `Docs:` - Documentation updates

## Best Practices

### Do's
✅ Always check return values
✅ Always free allocated memory
✅ Use safe string functions (strncpy, snprintf)
✅ Validate input parameters
✅ Use appropriate error codes
✅ Log errors appropriately
✅ Document complex logic

### Don'ts
❌ Never use strcpy or sprintf
❌ Never ignore return values
❌ Never leak memory
❌ Never use uninitialized variables
❌ Never access NULL pointers
❌ Never use magic numbers
❌ Never commit debug code

