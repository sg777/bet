# GUI Simulator Testing Guide

## Overview

The GUI Simulator is an automated testing tool that simulates a frontend GUI client connecting to the Pangea-Bet backend WebSocket server. This allows you to test the backend-GUI integration without manually clicking through the actual GUI for every test iteration.

**Why This is Essential:**
- **Rapid Testing**: Test complete game flows in seconds instead of minutes
- **Automated Responses**: Simulator auto-responds to betting prompts
- **Reproducible Tests**: Same test scenarios every time
- **Parallel Testing**: Can run multiple simulated players simultaneously
- **Interruption Testing**: Easily test reconnection, timeouts, and edge cases
- **CI/CD Ready**: Can be integrated into automated test pipelines

## Files

### Core Simulator
- **Location**: `/root/bet/tools/gui_simulator.js`
- **Purpose**: Node.js script that connects to backend WebSocket and simulates GUI interactions
- **Dependencies**: `ws` module (installed in `/root/bet/tools/node_modules/`)

### Test Automation Script
- **Location**: `/root/bet/tools/test_gui_flow.sh`
- **Purpose**: Bash script that orchestrates full test scenarios (starts nodes, launches simulators, monitors logs)

## Quick Start

### 1. Manual Simulator Test

Connect simulator to a running player backend:

```bash
cd /root/bet/tools
node gui_simulator.js 9001 check
```

**Parameters:**
- `9001` - WebSocket port (9001 for player 1, 9002 for player 2)
- `check` - Action mode (see below)

### 2. Automated Full Test

Run complete test with all nodes:

```bash
cd /root/bet/tools
./test_gui_flow.sh quick check
```

**Test Modes:**
- `quick` - Single player, 60 second test (default)
- `full` - Two players, 180 second test
- `stress` - Multiple games, 300 second test

## Simulator Action Modes

The simulator can be configured to respond to betting prompts in different ways:

### 1. **check** (Default, Conservative)
- Always checks if possible
- Calls if checking isn't available
- Never raises or folds voluntarily
- Best for: Testing game flow without aggressive betting

```bash
node gui_simulator.js 9001 check
```

### 2. **call** (Passive)
- Always calls to match the current bet
- Never folds (unless broke)
- Never raises
- Best for: Testing pot accumulation

```bash
node gui_simulator.js 9001 call
```

### 3. **random** (Realistic)
- Weighted random actions:
  - 40% check (when possible)
  - 40% call
  - 10% raise
  - 10% fold
- Best for: Simulating real player behavior

```bash
node gui_simulator.js 9001 random
```

### 4. **fold** (Edge Case Testing)
- Always folds
- Best for: Testing player elimination and single-player scenarios

```bash
node gui_simulator.js 9001 fold
```

## What the Simulator Does

### On Connection
1. Connects to backend WebSocket server
2. Sends initial handshake messages:
   - `backend_status` - Request backend status
   - `walletInfo` - Request wallet information
   - `get_bal_info` - Request balance information
3. Enters monitoring mode

### During Game
- **Receives `deal` messages**: Logs hole cards and board cards
- **Receives `betting` messages**:
  - If it's another player's action: logs the action
  - If it's this player's turn: auto-responds based on action mode
- **Receives `finalInfo`**: Logs showdown results and winners

### Auto-Response Example
When it's the player's turn to bet:
```
[3:54:41 AM] 🎯 YOUR TURN TO ACT
                Player: 0
                Pot: 0.04
                To Call: 0.01
                Min Raise: 0.04
                🤖 Auto-Response: CALL
                Amount: 0.01
```

The simulator automatically:
1. Waits 1.5 seconds (simulates thinking time)
2. Selects action based on configured mode
3. Calculates appropriate bet amount
4. Sends response to backend in correct JSON format

## Testing Scenarios

### Scenario 1: Basic Connection Test
**Goal**: Verify WebSocket connection and message exchange

```bash
# Start player backend
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini --gui &

# Wait for backend to initialize
sleep 5

# Connect simulator
cd /root/bet/tools
timeout 30 node gui_simulator.js 9001 check
```

**Expected Output:**
```
✅ Connected to backend!
📤 Sending initial requests...
   → backend_status
   → walletInfo
   → get_bal_info
```

### Scenario 2: Full Game Flow (Single Player)
**Goal**: Test complete game from start to finish

```bash
cd /root/bet/tools
./test_gui_flow.sh quick check
```

**Expected Events:**
1. Dealer, cashier, and player 1 start
2. Player joins table (blockchain payin confirmation)
3. Simulator connects and monitors
4. Game progresses: deal → betting → showdown
5. Logs saved to `/tmp/gui_test_*/`

### Scenario 3: Two-Player Game
**Goal**: Test multi-player game with different strategies

```bash
cd /root/bet/tools
./test_gui_flow.sh full random
```

**Features:**
- Player 1 and Player 2 both in GUI mode
- Two simulators running simultaneously (ports 9001, 9002)
- Random betting actions create realistic game flow
- Full 180-second test duration

### Scenario 4: Interruption Testing
**Goal**: Test reconnection and resilience

```bash
# Start backend
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini --gui &
sleep 10

# Start simulator
cd /root/bet/tools
node gui_simulator.js 9001 check &
SIM_PID=$!

# Let it run for 20 seconds
sleep 20

# Kill simulator
kill $SIM_PID

# Wait 10 seconds
sleep 10

# Reconnect
node gui_simulator.js 9001 check
```

**What to Verify:**
- Backend handles disconnection gracefully
- Reconnection establishes successfully
- Game state remains consistent
- No memory leaks or crashes

## Message Format Reference

### Messages Sent by Simulator

#### Initial Handshake
```json
{"method": "backend_status"}
{"method": "walletInfo"}
{"method": "get_bal_info"}
```

#### Betting Response
```json
{
  "method": "betting",
  "playerid": 0,
  "round": 0,
  "min_amount": 0.01,
  "possibilities": ["fold", "check", "call", "raise"],
  "action": "call",
  "bet_amount": 0.01
}
```

### Messages Received from Backend

#### Deal (Hole Cards)
```json
{
  "method": "deal",
  "deal": {
    "holecards": ["Ah", "Kd"],
    "balance": 0.49
  }
}
```

#### Deal (Board)
```json
{
  "method": "deal",
  "deal": {
    "board": ["Qh", "Jc", "9s"]
  }
}
```

#### Betting Round
```json
{
  "method": "betting",
  "action": "round_betting",
  "playerid": 0,
  "round": 0,
  "pot": 0.04,
  "toCall": 0.01,
  "minRaiseTo": 0.04,
  "possibilities": ["fold", "check", "call", "raise"],
  "player_funds": [0.49, 0.48]
}
```

#### Showdown
```json
{
  "method": "finalInfo",
  "winners": [0],
  "win_amount": 0.04,
  "showInfo": {
    "allHoleCardsInfo": [["Ah", "Kd"], ["Qc", "Js"]],
    "boardCardInfo": ["Qh", "Jc", "9s", "8h", "7d"]
  }
}
```

## Monitoring and Debugging

### Real-Time Log Monitoring

Watch simulator output:
```bash
tail -f /tmp/gui_test_*/simulator_p1.log
```

Watch backend WebSocket activity:
```bash
tail -f /root/bet/poker/logs/player1_gui.log | grep -E "\[GUI\]|WebSocket|Recv from GUI"
```

### Common Issues

#### 1. "Cannot find module 'ws'"
**Solution:**
```bash
cd /root/bet/tools
npm install ws
```

#### 2. "Connection refused"
**Cause**: Backend not running or wrong port
**Solution:**
```bash
# Check if backend is running
ps aux | grep "bin/bet.*--gui"

# Check WebSocket port
netstat -tlnp | grep 9001
```

#### 3. "Backend is not ready to write data to the GUI"
**Cause**: Normal - player hasn't joined game yet (waiting for blockchain confirmations)
**Solution**: Wait for payin transaction to confirm (usually 10-30 seconds)

#### 4. Simulator connects but receives no messages
**Cause**: Game hasn't started or backend thread issue
**Solution**:
```bash
# Check backend logs for errors
grep -i error /root/bet/poker/logs/player1_gui.log

# Verify WebSocket callback
grep "LWS_CALLBACK_ESTABLISHED" /root/bet/poker/logs/player1_gui.log
```

## Advanced Usage

### Testing Multiple Concurrent Games

```bash
# Terminal 1: Game 1 (Player 1)
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini --gui &
cd /root/bet/tools && node gui_simulator.js 9001 random

# Terminal 2: Game 2 (Player 2, different table)
cd /root/bet/poker
./bin/bet player config/verus_player_p2.ini --gui &
cd /root/bet/tools && node gui_simulator.js 9002 call
```

### Custom Simulator Script

Create your own simulator for specific test cases:

```javascript
const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:9001');

ws.on('open', () => {
  console.log('Connected!');
  ws.send(JSON.stringify({ method: 'backend_status' }));
});

ws.on('message', (data) => {
  const msg = JSON.parse(data);
  console.log('Received:', msg.method);
  
  // Custom betting logic
  if (msg.method === 'betting' && msg.action === 'round_betting') {
    // Always raise 2x
    const response = {
      method: 'betting',
      playerid: msg.playerid,
      action: 'raise',
      bet_amount: (msg.toCall || 0) * 2
    };
    ws.send(JSON.stringify(response));
  }
});
```

## Integration with CI/CD

The simulator can be integrated into continuous integration pipelines:

```bash
#!/bin/bash
# ci_test.sh

set -e

# Start nodes
./test_gui_flow.sh quick check

# Check results
if grep -q "Test Complete" /tmp/gui_test_*/simulator_p1.log; then
    echo "✅ GUI integration tests passed"
    exit 0
else
    echo "❌ GUI integration tests failed"
    exit 1
fi
```

## Performance Metrics

The simulator can help measure:
- **Latency**: Time between backend message and GUI response
- **Throughput**: Messages per second
- **Reliability**: Connection stability over long tests
- **Memory**: Backend memory usage during extended sessions

Example metrics from a typical test:
```
Duration:          180s
Messages received: 45
Betting actions:   12
Connection drops:  0
Average latency:   <100ms
```

## Future Enhancements

Potential improvements to the simulator:
1. **Configurable delays**: Simulate slow/fast players
2. **Error injection**: Test error handling (malformed JSON, invalid actions)
3. **Load testing**: Multiple simultaneous connections
4. **Replay mode**: Record and replay specific game sequences
5. **GUI comparison**: Verify backend messages match actual GUI expectations
6. **Metrics dashboard**: Real-time performance monitoring

## Conclusion

The GUI Simulator is an essential tool for rapid, reliable testing of the Pangea-Bet backend-GUI integration. It eliminates the need for manual GUI interaction during development and testing, allowing for:

- ✅ Faster development cycles
- ✅ More thorough testing coverage
- ✅ Easier debugging of edge cases
- ✅ Automated regression testing
- ✅ Confident deployment

For any issues or questions, refer to the main documentation:
- `/root/bet/docs/GUI_BACKEND_MAPPING.md` - Message format specifications
- `/root/bet/docs/COMMUNITY_TESTING_GUIDE.md` - Manual testing procedures
- `/root/bet/docs/GUI_INTEGRATION_STATUS.md` - Current integration status

