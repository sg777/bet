[![bet CD](https://github.com/sg777/bet/actions/workflows/bet-cd.yml/badge.svg?branch=master)](https://github.com/sg777/bet/actions/workflows/bet-cd.yml)

# Pangea-Bet

**Pangea-Bet** is a decentralized poker platform built on the CHIPS blockchain. It implements secure, trustless mental poker using cryptographic techniques, with all game state and transactions recorded on-chain.

## Overview

Pangea-Bet is a decentralized poker platform built on the CHIPS blockchain. It uses Verus IDs for node communication and game state storage, with all transactions recorded on-chain.

### Node Types

1. **Dealer**: Manages tables and coordinates gameplay
2. **Player**: Joins tables and plays games
3. **Cashier**: Validates transactions and processes payouts

### Ports

- **GUI HTTP**: Port `1234` (serves web interface)
- **WebSocket**: Ports `9000` (dealer), `9001` (player), `9002` (cashier) - configurable in respective config files

## Prerequisites

- Linux or macOS
- Running CHIPS daemon (`chipsd`) with blockchain synced
- Verus IDs registered on CHIPS blockchain (for dealer/cashier)
- Ports `1234` and `9000-9002` open

## Installation

### Building from Source

1. **Clone the repository**:
   ```bash
   git clone https://github.com/sg777/bet.git
   cd bet
   git submodule update --init --recursive
   ```

2. **Build the project**:
   ```bash
   cd poker
   make
   ```

3. **Build dependencies** (if needed):
   ```bash
   make --directory ../external/iniparser
   ```

See [Compilation Documentation](./docs/protocol/compile.md) for detailed build instructions.

### Pre-compiled Binaries

Pre-compiled binaries are available in the [Releases](https://github.com/sg777/bet/releases) section.

## Configuration

Configuration files are in `poker/config/`. See [Configuration Documentation](./docs/protocol/) for details.

## Usage

### Starting Nodes

```bash
# Dealer
cd poker && ./bin/bet dealer

# Cashier
cd poker && ./bin/cashierd

# Player
cd poker && ./bin/bet player
```

### Accessing the GUI

Open `http://localhost:1234` in your browser after starting any node.

### Funding and Withdrawals

**Get address and fund**:
```bash
chips-cli getnewaddress
# Send CHIPS to this address
```

**Withdraw**:
```bash
cd poker && ./bin/bet withdraw <amount> <destination_address>
```

**Note**: Ensure sufficient CHIPS for stake size + transaction fees (~0.0001 CHIPS per action).

## Documentation

- [Verus Migration Guide](./docs/verus_migration/verus_migration.md) - Comprehensive guide to Verus ID architecture
- [ID Creation Process](./docs/verus_migration/id_creation_process.md) - How to create and manage Verus IDs
- [Keys and Data Management](./docs/verus_migration/ids_keys_data.md) - VDXF key structure and data storage
- [GUI Message Formats](./docs/protocol/GUI_MESSAGE_FORMATS.md) - WebSocket API documentation
- [Protocol Documentation](./docs/protocol/) - Detailed protocol specifications
- [Glossary](./docs/protocol/glossary.md) - Terminology and abbreviations

## Technical Details

### Underlying Blockchain

Pangea-Bet runs on [CHIPS](https://github.com/chips-blockchain/chips), a Public Blockchain as a Service (PBaaS) chain on the Verus network. CHIPS features:

- **10-second block time** for fast transaction confirmations
- **Notarization** to Bitcoin blockchain for security
- **Verus ID integration** for identity-based operations
- **Low transaction fees** (~0.0001 CHIPS per transaction)

### Security Model

- **Mental Poker**: Cryptographic protocols ensure card shuffling is verifiable and secure
- **Multi-Signature Wallets**: Player funds are held in multi-sig addresses
- **On-Chain Verification**: All game actions are recorded on-chain for auditability
- **Dispute Resolution**: Transaction history enables transparent dispute resolution

### Performance Considerations

- **Block Latency**: Game actions are recorded on-chain, introducing ~10-20 second latency per action
- **Transaction Costs**: Each game action costs ~0.0001 CHIPS in transaction fees
- **State Polling**: Nodes poll Verus IDs to retrieve game state updates
- **Scalability**: Multiple tables can run simultaneously, each using separate Verus IDs

## Contributing

Contributions are welcome! Please ensure:

1. Code follows existing style and patterns
2. All compiler warnings are addressed (`-Werror` is enabled)
3. Memory safety is maintained (no leaks, no buffer overflows)
4. Documentation is updated for significant changes

## License

See LICENSE file for details.

## Support

For issues, questions, or contributions:
- **GitHub Issues**: [Report issues](https://github.com/sg777/bet/issues)
- **Documentation**: See `docs/` directory for detailed documentation

---

**Note**: This is active development software. Always test thoroughly before using with real funds.
