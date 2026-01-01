# Pangea-Bet Documentation

This directory contains comprehensive documentation for the Pangea-Bet platform. Documentation is organized into logical sections based on your needs.

## Quick Navigation

### Getting Started
- **[Installation & Setup](./protocol/compile.md)** - Building from source and installation
- **[Pre-compiled Binaries](./protocol/release.md)** - Download and setup pre-built binaries
- **[Node Configuration](#configuration-guides)** - Configure dealer, player, and cashier nodes

### Configuration Guides
- **[Dealer Configuration](./protocol/dealer_configuration.md)** - Dealer node setup and settings
- **[Player Configuration](./protocol/player_configuration.md)** - Player node setup and settings
- **[Cashier Configuration](./protocol/cashier_configuration.md)** - Cashier node setup and settings

### Architecture & Protocol
- **[System Architecture](./verus_migration/architecture.md)** - Layered architecture and RPC abstraction
- **[Verus Migration Guide](./verus_migration/verus_migration.md)** - Overview of Verus ID-based architecture
- **[ID Creation Process](./verus_migration/id_creation_process.md)** - Creating and managing Verus IDs
- **[Keys and Data Structure](./verus_migration/ids_keys_data.md)** - VDXF key structure and data storage
- **[Node Communication](./protocol/node_communication.md)** - How nodes communicate via Verus IDs
- **[Transaction Flow](./protocol/tx_flow.md)** - Transaction types and flow during gameplay
- **[GUI Message Formats](./protocol/GUI_MESSAGE_FORMATS.md)** - WebSocket API for GUI communication

### Advanced Topics
- **[Game State Management](./verus_migration/game_state.md)** - How game state is stored and retrieved
- **[Dispute Resolution](./verus_migration/dispute_resolution.md)** - Handling disputes and transaction verification
- **[Player Rejoining](./verus_migration/player_rejoining.md)** - Player disconnection and reconnection
- **[Dealer Rejoining](./verus_migration/dealer_rejoining.md)** - Dealer disconnection scenarios

### Reference
- **[Glossary](./protocol/glossary.md)** - Terminology and abbreviations
- **[Transaction Types](./protocol/tx_types.md)** - Detailed transaction reference
- **[Upgrade Guide](./protocol/upgrade.md)** - API compatibility and upgrade notes

## Documentation Structure

```
docs/
├── README.md (this file)
├── protocol/              # Protocol specifications and general documentation
│   ├── compile.md         # Build and installation instructions
│   ├── release.md         # Pre-compiled binaries and releases
│   ├── dealer_configuration.md
│   ├── player_configuration.md
│   ├── cashier_configuration.md
│   ├── node_communication.md
│   ├── tx_flow.md
│   ├── GUI_MESSAGE_FORMATS.md
│   └── ...
└── verus_migration/       # Verus ID architecture and migration
    ├── architecture.md    # System architecture and RPC abstraction
    ├── verus_migration.md # Migration overview
    ├── id_creation_process.md
    ├── ids_keys_data.md
    ├── game_state.md
    └── ...
```

## Finding What You Need

**New to Pangea-Bet?**
1. Start with [Installation & Setup](./protocol/compile.md)
2. Configure your node type: [Dealer](./protocol/dealer_configuration.md) | [Player](./protocol/player_configuration.md) | [Cashier](./protocol/cashier_configuration.md)
3. Understand the architecture: [Verus Migration Guide](./verus_migration/verus_migration.md)

**Setting up a node?**
- [Dealer Setup](./protocol/dealer_configuration.md)
- [Player Setup](./protocol/player_configuration.md)
- [Cashier Setup](./protocol/cashier_configuration.md)

**Understanding how it works?**
- [Verus Migration Overview](./verus_migration/verus_migration.md)
- [Node Communication](./protocol/node_communication.md)
- [Transaction Flow](./protocol/tx_flow.md)

**Developing or integrating?**
- [System Architecture](./verus_migration/architecture.md) - Understand the layered design
- [GUI Message Formats](./protocol/GUI_MESSAGE_FORMATS.md)
- [API Reference](./protocol/bet-api.md)
- [Transaction Types](./protocol/tx_types.md)

**Troubleshooting?**
- [Common Setup Errors](./protocol/setup_comon_errors.md)
- [Dispute Resolution](./verus_migration/dispute_resolution.md)
- [Player Rejoining](./verus_migration/player_rejoining.md)

---

For the main project README, see [../README.md](../README.md).

