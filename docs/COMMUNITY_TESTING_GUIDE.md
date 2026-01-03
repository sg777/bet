# Pangea-Bet Community Testing Guide

## Overview

Pangea-Bet is a decentralized poker platform built on the Verus (CHIPS) blockchain. This guide will walk you through setting up and testing your own poker game using the Verus identity system.

## Prerequisites

### Software Requirements
- Linux system (Ubuntu 18.04+ recommended)
- Verus daemon with CHIPS PBaaS chain
- Git, GCC, Make, and standard build tools

### Blockchain Requirements
- **CHIPS wallet** with at least 5 CHIPS for testing
- **Verus IDs** (explained below)

---

## Understanding the Node Types

| Node | Role | Description |
|------|------|-------------|
| **Dealer** | Game Host | Manages the poker table, shuffles deck, deals cards, tracks betting |
| **Player** | Participant | Joins tables, receives cards, places bets |
| **Cashier** | Settlement | Processes payins, handles payouts, resolves disputes |

---

## Verus Identity Requirements

### Required Identities

To run your own poker game, you need the following Verus IDs:

#### 1. Poker System ID (Shared)
- **ID Format**: `poker.yournamespace@` or use existing `poker.sg777z.chips.vrsc@`
- **Purpose**: Root namespace for all poker identities
- **Keys Stored**:
  - `dealers_key` - List of registered dealers
  - `tables_key` - List of active tables

#### 2. Dealer ID
- **ID Format**: `d1.poker.yournamespace@` (or your own naming)
- **Purpose**: Identifies the dealer node
- **Keys Stored**:
  - Table configuration
  - Game state
  - Deck information

#### 3. Table ID
- **ID Format**: `t1.poker.yournamespace@`
- **Purpose**: Represents a poker table
- **Keys Stored**:
  - Game ID
  - Player information
  - Betting state
  - Card reveal data

#### 4. Cashier ID
- **ID Format**: `cashier.yournamespace@`
- **Purpose**: Handles funds and settlements
- **Requirements**: Must be able to receive CHIPS payments

#### 5. Player IDs (one per player)
- **ID Format**: `p1.yournamespace@`, `p2.yournamespace@`
- **Purpose**: Each player needs their own Verus ID
- **Keys Stored**:
  - Join requests
  - Betting actions
  - Card reveals

### Identity Ownership Requirements

| Identity | Who Needs to Own/Control |
|----------|-------------------------|
| Poker ID | Dealer operator (or shared) |
| Dealer ID | Dealer node operator |
| Table ID | Dealer node operator |
| Cashier ID | Cashier node operator |
| Player IDs | Individual players |

---

## Configuration Files

### Location
All configuration files are in `poker/config/`

### 1. Verus IDs and Keys Configuration
**File**: `poker/config/verus_ids_keys.ini`

```ini
[verus]
; RPC connection to Verus daemon (CHIPS chain)
rpc_url = http://127.0.0.1:22778
rpc_user = your_rpc_user
rpc_password = your_rpc_password

[ids]
; Your poker namespace
poker_id = poker.yournamespace
dealer_id = d1
table_id = t1

[keys]
; Key names used for storing data
dealers_key = dealers
tables_key = tables
player_info_key = player_info
game_state_key = game_state
```

### 2. Dealer Configuration
**File**: `poker/config/verus_dealer.ini`

```ini
[dealer]
dealer_id = d1
table_id = t1

[table]
max_players = 2
small_blind = 0.01
big_blind = 0.02
min_stake = 0.1
max_stake = 1.0
```

### 3. Player Configuration
**File**: `poker/config/verus_player_p1.ini`

```ini
[player]
; Your player Verus ID (without the full domain)
verus_pid = p1

; Table to join
table_id = t1

; Amount to buy-in with (in CHIPS)
payin_amount = 0.5

; Your payin transaction ID (leave empty for first game)
txid = 
```

### 4. RPC Credentials
**File**: `poker/.rpccredentials`

```
rpc_url = http://127.0.0.1:22778
rpc_user = your_rpc_user
rpc_password = your_rpc_password
```

---

## Building the Software

```bash
# Clone the repository
git clone https://github.com/sg777/bet.git
cd bet

# Build external dependencies
cd external/iniparser && make && cd ../..
cd libs/crypto && make && cd ../..

# Build the poker application
cd poker && make
```

The binary will be at `poker/bin/bet`

---

## Running the Nodes

### Important: Node Startup Order

1. **Dealer** first (resets table and waits for players)
2. Wait 30 seconds for blockchain confirmation
3. **Player 1** joins
4. Wait 30 seconds
5. **Player 2** joins
6. **Cashier** can start anytime

### Starting the Dealer

```bash
cd /path/to/bet/poker

# Start dealer with table reset (fresh game)
./bin/bet dealer reset

# Or rejoin existing game
./bin/bet dealer
```

### Starting a Player

```bash
cd /path/to/bet/poker

# Start player with AUTO betting (for testing)
./bin/bet player config/verus_player_p1.ini --auto

# Start player with CLI betting (interactive)
./bin/bet player config/verus_player_p1.ini --cli

# Start player with GUI betting (websocket, future)
./bin/bet player config/verus_player_p1.ini --gui
```

### Starting the Cashier

```bash
cd /path/to/bet/poker

# Start cashier for table t1
./bin/bet cashier t1
```

---

## Betting Modes

### AUTO Mode (Default)
- Posts blinds automatically
- Auto-calls or checks based on game state
- Best for automated testing

### CLI Mode (Interactive)
- Prompts you for actions on your turn
- Available actions:
  - `fold` or `f` - Fold your hand
  - `check` or `x` - Check (if no bet to call)
  - `call` or `c` - Call the current bet
  - `raise 0.05` or `r 0.05` - Raise to specified amount
  - `allin` or `a` - Go all-in

Example CLI session:
```
═══════════════════════════════════════════
  🎯 YOUR TURN - Player 0 (p1)             
  ⏰ Time remaining: 59 seconds           
═══════════════════════════════════════════
  Action: bet
  Round: 0, Pot: 0.0300 CHIPS
  Minimum to call: 0.0100 CHIPS
  Options: [fold] [call] [raise] 
  Your funds: 0.4900 CHIPS
═══════════════════════════════════════════

  Enter action (fold/check/call/raise/allin): call
  → Calling 0.0100 CHIPS...
```

---

## Game Flow

### 1. Table Setup (Dealer)
- Dealer resets table
- Creates new game ID
- Waits for players

### 2. Player Join
- Players send payin (0.5 CHIPS) to cashier
- Players update their ID with join request
- Dealer confirms players

### 3. Deck Shuffling
- Each player generates their deck keys
- Dealer shuffles and blinds the deck
- All crypto data stored on blockchain

### 4. Card Dealing
- Hole cards dealt to each player
- Flop (3 cards), Turn, River dealt

### 5. Betting Rounds
- Pre-flop betting (after hole cards)
- Flop betting
- Turn betting
- River betting

### 6. Showdown & Settlement
- Best hand wins
- Cashier distributes funds
- Game ends

---

## Timeout Mechanism

Players have 60 seconds OR 6 blocks (whichever is longer) to act. If a player doesn't respond:
- They are automatically **folded**
- Game continues with remaining players

---

## Rejoin Support

### If Dealer Crashes
```bash
# Dealer can rejoin existing game
./bin/bet dealer

# Dealer will:
# - Detect game in progress
# - Load deck keys from local SQLite DB
# - Continue from current state
```

### If Player Crashes
```bash
# Player can rejoin existing game
./bin/bet player config/verus_player_p1.ini --cli

# Player will:
# - Detect they're already in the table
# - Load deck keys from local SQLite DB
# - Continue playing
```

---

## Troubleshooting

### Common Issues

#### "Error in updating the game state"
- Verus identity update failed
- Check your RPC credentials
- Ensure you have permission to update the identity
- Wait for previous transaction to confirm

#### "Player already exists in the table"
- You've already joined - just proceed
- The node will handle this automatically

#### "Payin not confirmed"
- Wait for blockchain confirmation (1-2 minutes)
- Check transaction on explorer

#### "Deck decoding error"
- Deck size mismatch between nodes
- Ensure all nodes use same CARDS_MAXCARDS value
- May need to restart with fresh game

### Checking Logs
```bash
# Real-time dealer log
tail -f poker/logs/dealer.log

# Real-time player log
tail -f poker/logs/player1.log

# Check for errors
grep -i error poker/logs/*.log
```

### Killing All Nodes
```bash
pkill -9 -f "bin/bet"
```

---

## Testing Checklist

### Basic Game Test
- [ ] Dealer starts and resets table
- [ ] Player 1 joins successfully
- [ ] Player 2 joins successfully
- [ ] Deck shuffling completes
- [ ] Hole cards dealt to both players
- [ ] Pre-flop betting works
- [ ] Flop dealt
- [ ] Flop betting works
- [ ] Turn dealt
- [ ] Turn betting works
- [ ] River dealt
- [ ] River betting works
- [ ] Showdown determines winner
- [ ] Settlement sends funds

### Rejoin Test
- [ ] Kill dealer during card dealing, rejoin works
- [ ] Kill player during betting, rejoin works
- [ ] Game continues after rejoin

### Timeout Test
- [ ] Player doesn't act for 60+ seconds
- [ ] Player is auto-folded
- [ ] Game continues with remaining players

---

## Support

For issues and questions:
- GitHub Issues: https://github.com/sg777/bet/issues
- Discord: [Your Discord Link]

---

## Version Information

- **Version**: 1.0-beta
- **Deck Size**: 14 cards (2-player optimized)
- **Blockchain**: Verus CHIPS PBaaS
- **Last Updated**: January 2026

