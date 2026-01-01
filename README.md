[![bet CD](https://github.com/sg777/bet/actions/workflows/bet-cd.yml/badge.svg?branch=master)](https://github.com/sg777/bet/actions/workflows/bet-cd.yml)

# Pangea-Bet

**Pangea-Bet** is a decentralized poker platform built on the CHIPS blockchain. It implements secure, trustless mental poker using cryptographic techniques, with all game state and transactions recorded on-chain.

## Overview

Pangea-Bet enables decentralized poker games where players can join tables, play hands, and settle funds without trusting a central authority. The platform uses:

- **Verus IDs (VDXF)** for node communication and game state storage
- **CHIPS blockchain** for all transactions and payments
- **WebSockets** for real-time GUI communication
- **Cryptographic protocols** for secure card shuffling and dealing

## Architecture

### Communication Model

The platform uses **Verus IDs (VDXF)** for all inter-node communication, eliminating the need for static IP addresses or traditional socket-based communication:

- **Node Discovery**: Nodes discover each other through Verus IDs on the blockchain
- **State Storage**: Game state is stored in Verus ID contentmultimaps (CMM) using key-value pairs
- **Data Synchronization**: All nodes poll Verus IDs to retrieve game state updates
- **Reliability**: Player disconnections are handled gracefully by reading state from the blockchain

See the [Verus Migration Documentation](./docs/verus_migration/verus_migration.md) for detailed information.

### Payment Model

- **CHIPS-Only**: All payments are recorded as CHIPS blockchain transactions
- **On-Chain Settlement**: Game actions (bets, wins, payouts) are recorded on-chain
- **Multi-Signature Addresses**: Player funds are held in multi-sig addresses for security
- **Dispute Resolution**: Transaction history on-chain enables dispute resolution

### Node Types

The platform consists of three node types:

1. **Dealer Node**: Manages game tables, coordinates gameplay, collects commission
2. **Cashier Node**: Validates transactions, processes payouts, provides dispute resolution
3. **Player Node**: Connects to tables, participates in games, manages player funds

### Network Ports

Each node type uses configurable WebSocket ports for GUI communication:

- **Dealer**: Default port `9000` (configurable via `dealer_config.ini`)
- **Cashier**: Default port `9002` (configurable via `cashier_config.ini`)
- **Player**: Default port `9001` (configurable via `player_config.ini`)

All ports are configurable in their respective configuration files. The GUI is served via HTTP on the same port as the WebSocket server.

## Prerequisites

### System Requirements

- **Operating System**: Linux or macOS
- **CHIPS Node**: Running CHIPS daemon (`chipsd`) with blockchain synced
- **Verus IDs**: Valid Verus IDs registered on the CHIPS blockchain (for dealer/cashier)
- **Network**: WebSocket ports open for GUI access (default: 9000-9002)

### Verus ID Requirements

Dealer and cashier nodes require Verus IDs registered on the CHIPS blockchain. Players can join using their Verus IDs or by connecting to public tables.

The default identity structure:
```
sg777z.chips.vrsc@ (root identity)
├── dealer.sg777z.chips.vrsc@
│   └── <dealer_name>.sg777z.chips.vrsc@
├── cashier.sg777z.chips.vrsc@
└── <player_name>.sg777z.chips.vrsc@
```

See [ID Creation Process](./docs/verus_migration/id_creation_process.md) for details.

## Installation

### Quick Start with Docker (Recommended)

The easiest way to get started is using Docker:

```bash
# Pull the latest Docker image
docker pull sg777/bet:latest

# Run with host network (required for CHIPS connectivity)
docker run -it --net=host sg777/bet:latest

# Inside the container, start CHIPS daemon
cd && ./chips/src/chipsd &

# Start a player node
cd && ./bet/poker/bin/bet player

# Start a dealer node (requires Verus ID configuration)
cd && ./bet/poker/bin/bet dealer
```

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

All configuration files are located in `poker/config/`:

### Dealer Configuration (`dealer_config.ini`)

Key settings:
- `gui_ws_port`: WebSocket port for GUI (default: 9000)
- `chips_tx_fee`: Transaction fee for CHIPS operations (default: 0.0001)
- `dcv_commission`: Dealer commission percentage (default: 0.5%)
- `min_cashiers`: Minimum cashiers required (default: 2)
- `max_players`: Maximum players per table (default: 2)
- `big_blind`: Big blind amount in CHIPS
- `min_stake` / `max_stake`: Table stake limits in big blinds

### Player Configuration (`player_config.ini`)

Key settings:
- `gui_ws_port`: WebSocket port for GUI (default: 9001)
- `name`: Player display name
- `table_stake_size`: Stake size in big blinds (20-100 range)
- `max_allowed_dcv_commission`: Maximum acceptable dealer commission

### Cashier Configuration (`cashier_config.ini`)

Key settings:
- `gui_ws_port`: WebSocket port for GUI (default: 9002)
- Cashier node definitions with public keys and IPs

### Verus IDs and Keys Configuration (`verus_ids_keys.ini`)

Configure Verus identity names and VDXF keys used by the platform. Default values are defined in the code, but can be customized here.

See [Configuration Documentation](./docs/protocol/) for detailed configuration options.

## Usage

### Starting a Dealer Node

```bash
cd poker
./bin/bet dealer
```

The dealer will:
1. Connect to the CHIPS blockchain
2. Register/update its Verus ID with table information
3. Wait for players to join
4. Coordinate gameplay
5. Record all game actions on-chain

### Starting a Cashier Node

```bash
cd poker
./bin/cashierd
```

The cashier will:
1. Connect to the CHIPS blockchain
2. Monitor dealer Verus IDs for table information
3. Validate player transactions
4. Process payouts
5. Handle dispute resolution

### Starting a Player Node

```bash
cd poker
./bin/bet player
```

The player will:
1. Connect to the CHIPS blockchain
2. Discover available tables via dealer Verus IDs
3. Join a table using the configured stake size
4. Participate in games
5. Connect to GUI for gameplay

### Accessing the GUI

The GUI is served via HTTP on the WebSocket port of each node:

- **Dealer GUI**: `http://localhost:9000` (or configured port)
- **Cashier GUI**: `http://localhost:9002` (or configured port)
- **Player GUI**: `http://localhost:9001` (or configured port)

Players can also use cashier-hosted GUIs. After starting a player node, check the logs for available GUI URLs:

```
[config.c:bet_display_cashier_hosted_gui:236] http://<cashier-ip>:9002/
```

### Funding and Withdrawals

**Funding a player wallet**:
```bash
# Get a CHIPS address
chips-cli getnewaddress

# Send CHIPS to this address (requires funds in your CHIPS wallet)
```

**Withdrawing funds**:
```bash
cd poker
./bin/bet withdraw <amount> <destination_address>
```

**Important**: Always ensure you have sufficient CHIPS to cover:
- Table stake size
- Transaction fees (approximately 0.0001 CHIPS per game action)
- Reserve for multiple game actions (recommended: stake_size + 0.01 CHIPS)

### Game Flow

1. **Dealer Setup**: Dealer creates/updates table information on Verus ID
2. **Player Discovery**: Players discover tables by reading dealer Verus IDs
3. **Table Join**: Players join tables by creating pay-in transactions
4. **Gameplay**: Game state is stored in Verus IDs, all nodes poll for updates
5. **Settlement**: Winners receive payouts, dealer collects commission
6. **Dispute Resolution**: Transaction history enables dispute resolution

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
