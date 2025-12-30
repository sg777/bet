# Crypto777 Library

This library provides cryptographic primitives and utilities for the poker application.

## File Organization

### Cryptographic Algorithms
| File | Description |
|------|-------------|
| `curve25519.c` | Curve25519 elliptic curve implementation |
| `curve25519-donna.c` | Donna implementation of Curve25519 |
| `tweetnacl.c` | TweetNaCl - compact crypto library (signing, encryption) |
| `scrypt.c` | Scrypt key derivation function |
| `SaM.c` | Secure and Merge utilities |

### OS Portable Layer
| File | Description |
|------|-------------|
| `OS_portable.c` | Cross-platform OS abstractions |
| `OS_portable.h` | Header with portable function declarations |
| `OS_nonportable.c` | Platform-specific implementations |
| `OS_time.c` | Time utilities (TAI, UTC conversions) |
| `iguana_OS.c` | Legacy iguana OS utilities |

### Encoding & Serialization  
| File | Description |
|------|-------------|
| `iguana_serdes.c` | Serialization/deserialization for blockchain data |
| `ramcoder.c` | RAM-based encoding/compression |
| `inet.c` | Network/internet utilities |

### Utilities
| File | Description |
|------|-------------|
| `iguana_utils.c` | Bit manipulation, hashing wrappers, hex conversion |
| `cJSON.c` | JSON parsing utilities |
| `bitcoind_RPC.c` | Bitcoin RPC client |

## Key Functions Used by Poker

### Curve25519 (Key Exchange)
- `curve25519()` - Core Curve25519 scalar multiplication
- `curve25519_keypair()` - Generate public/private keypair
- `curve25519_shared()` - Compute shared secret

### bits256 Operations (256-bit integers)
- `bits256_str()` - Convert to hex string
- `bits256_conv()` - Convert from hex string  
- `bits256_doublesha256()` - Double SHA256 hash
- `bits256_cmp()` - Compare two 256-bit values

### Hashing
- `calc_sha256()` - SHA256 hash
- `calc_rmd160_sha256()` - RIPEMD160(SHA256(data))

### Hex/String Utilities
- `decode_hex()` - Hex string to bytes
- `init_hexbytes_noT()` - Bytes to hex string
- `clonestr()` - Duplicate string

## Dependencies

This library depends on headers from:
- `../../includes/` - curve25519.h, tweetnacl.h, cJSON.h, etc.
- `../iguana/` - iguana777.h (legacy, provides type definitions)

## Building

```bash
cd libs/crypto777
make
```

This produces `libcrypto777.a` which is linked by the poker application.

