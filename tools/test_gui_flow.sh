#!/bin/bash
##############################################################################
# GUI Flow Test Script
#
# This script automates the testing of backend-GUI integration by:
# 1. Starting backend nodes (dealer, cashier, players)
# 2. Launching GUI simulators for each player
# 3. Monitoring logs and game progression
# 4. Reporting test results
#
# Usage:
#   ./test_gui_flow.sh [mode] [action]
#
# Modes:
#   quick  - Single player, short test (default)
#   full   - Two players, complete game
#   stress - Multiple games in succession
#
# Actions (for simulator):
#   check  - Always check/call (default)
#   random - Random actions
#   call   - Always call/match
##############################################################################

set -e

MODE="${1:-quick}"
ACTION="${2:-check}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
POKER_DIR="$PROJECT_ROOT/poker"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_ID="$(date +%s)"
LOG_DIR="/tmp/gui_test_${TEST_ID}"
mkdir -p "$LOG_DIR"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║               GUI BACKEND INTEGRATION TEST                     ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Mode:           ${GREEN}${MODE}${NC}"
echo -e "Action:         ${GREEN}${ACTION}${NC}"
echo -e "Log Directory:  ${LOG_DIR}"
echo -e "Test ID:        ${TEST_ID}"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo -e "${YELLOW}🧹 Cleaning up...${NC}"
    
    # Kill all bet processes
    pkill -f "bin/bet" 2>/dev/null || true
    
    # Kill node processes (GUI simulators)
    pkill -f "gui_simulator.js" 2>/dev/null || true
    
    sleep 2
    
    echo -e "${GREEN}✅ Cleanup complete${NC}"
}

# Set trap for cleanup on exit
trap cleanup EXIT INT TERM

# Function to check if port is in use
check_port() {
    local port=$1
    netstat -tlnp 2>/dev/null | grep ":${port} " > /dev/null || \
    ss -tlnp 2>/dev/null | grep ":${port} " > /dev/null
}

# Function to wait for port to be ready
wait_for_port() {
    local port=$1
    local timeout=30
    local elapsed=0
    
    echo -n "   Waiting for port ${port}..."
    while [ $elapsed -lt $timeout ]; do
        if check_port $port; then
            echo -e " ${GREEN}✓${NC}"
            return 0
        fi
        sleep 1
        elapsed=$((elapsed + 1))
        echo -n "."
    done
    
    echo -e " ${RED}✗ (timeout)${NC}"
    return 1
}

# Step 1: Clean previous state
echo -e "${BLUE}Step 1: Cleaning previous state${NC}"
cd "$POKER_DIR"

if [ -f "logs/player1.log" ]; then
    echo "   Archiving old logs to ${LOG_DIR}/archive/"
    mkdir -p "${LOG_DIR}/archive"
    cp -r logs/*.log "${LOG_DIR}/archive/" 2>/dev/null || true
fi

# Stop any running processes
pkill -f "bin/bet" 2>/dev/null || true
sleep 2

echo -e "   ${GREEN}✓${NC} State cleaned"
echo ""

# Step 2: Start backend nodes
echo -e "${BLUE}Step 2: Starting backend nodes${NC}"

# Start dealer
echo -n "   Starting dealer..."
cd "$POKER_DIR"
nohup ./bin/bet dealer config/verus_dealer.ini --reset > "${LOG_DIR}/dealer.log" 2>&1 &
DEALER_PID=$!
sleep 3
if ps -p $DEALER_PID > /dev/null; then
    echo -e " ${GREEN}✓${NC} (PID: $DEALER_PID)"
else
    echo -e " ${RED}✗${NC}"
    echo "Check ${LOG_DIR}/dealer.log for errors"
    exit 1
fi

# Start cashier
echo -n "   Starting cashier..."
nohup ./bin/bet cashier config/verus_cashier.ini > "${LOG_DIR}/cashier.log" 2>&1 &
CASHIER_PID=$!
sleep 2
if ps -p $CASHIER_PID > /dev/null; then
    echo -e " ${GREEN}✓${NC} (PID: $CASHIER_PID)"
else
    echo -e " ${RED}✗${NC}"
    echo "Check ${LOG_DIR}/cashier.log for errors"
    exit 1
fi

# Start player 1 in GUI mode
echo -n "   Starting player 1 (GUI mode)..."
nohup ./bin/bet player config/verus_player_p1.ini --gui > "${LOG_DIR}/player1.log" 2>&1 &
PLAYER1_PID=$!
if ! wait_for_port 9001; then
    echo "Player 1 WebSocket failed to start"
    exit 1
fi
echo -e "   ${GREEN}✓${NC} Player 1 ready (PID: $PLAYER1_PID, WS: 9001)"

# Start player 2 in GUI mode (if full mode)
if [ "$MODE" = "full" ] || [ "$MODE" = "stress" ]; then
    echo -n "   Starting player 2 (GUI mode)..."
    nohup ./bin/bet player config/verus_player_p2.ini --gui > "${LOG_DIR}/player2.log" 2>&1 &
    PLAYER2_PID=$!
    if ! wait_for_port 9002; then
        echo "Player 2 WebSocket failed to start"
        exit 1
    fi
    echo -e "   ${GREEN}✓${NC} Player 2 ready (PID: $PLAYER2_PID, WS: 9002)"
fi

echo ""

# Step 3: Launch GUI simulators
echo -e "${BLUE}Step 3: Launching GUI simulators${NC}"

echo "   Waiting 10 seconds for game to initialize..."
sleep 10

# Start simulator for player 1
echo "   Starting simulator for Player 1 (port 9001)..."
cd "$SCRIPT_DIR"
node gui_simulator.js 9001 "$ACTION" > "${LOG_DIR}/simulator_p1.log" 2>&1 &
SIM1_PID=$!
echo -e "   ${GREEN}✓${NC} Simulator 1 started (PID: $SIM1_PID)"

# Start simulator for player 2 (if full mode)
if [ "$MODE" = "full" ] || [ "$MODE" = "stress" ]; then
    echo "   Starting simulator for Player 2 (port 9002)..."
    node gui_simulator.js 9002 "$ACTION" > "${LOG_DIR}/simulator_p2.log" 2>&1 &
    SIM2_PID=$!
    echo -e "   ${GREEN}✓${NC} Simulator 2 started (PID: $SIM2_PID)"
fi

echo ""

# Step 4: Monitor game progression
echo -e "${BLUE}Step 4: Monitoring game progression${NC}"
echo "   Logs are being written to: ${LOG_DIR}/"
echo "   Press Ctrl+C to stop monitoring"
echo ""

# Monitor based on mode
if [ "$MODE" = "quick" ]; then
    DURATION=60
    echo "   Quick test: monitoring for ${DURATION} seconds..."
elif [ "$MODE" = "full" ]; then
    DURATION=180
    echo "   Full test: monitoring for ${DURATION} seconds..."
else
    DURATION=300
    echo "   Stress test: monitoring for ${DURATION} seconds..."
fi

START_TIME=$(date +%s)
END_TIME=$((START_TIME + DURATION))

while [ $(date +%s) -lt $END_TIME ]; do
    ELAPSED=$(($(date +%s) - START_TIME))
    echo -ne "\r   Elapsed: ${ELAPSED}s / ${DURATION}s"
    
    # Check if processes are still running
    if ! ps -p $PLAYER1_PID > /dev/null; then
        echo -e "\n   ${RED}✗${NC} Player 1 died!"
        break
    fi
    
    if [ "$MODE" != "quick" ]; then
        if ! ps -p $PLAYER2_PID > /dev/null 2>&1; then
            echo -e "\n   ${RED}✗${NC} Player 2 died!"
            break
        fi
    fi
    
    sleep 1
done

echo ""
echo ""

# Step 5: Collect and analyze results
echo -e "${BLUE}Step 5: Test Results${NC}"
echo ""

# Check simulator logs for key events
echo "📊 Player 1 Simulator Results:"
if [ -f "${LOG_DIR}/simulator_p1.log" ]; then
    CONN=$(grep "Connected to backend" "${LOG_DIR}/simulator_p1.log" | wc -l)
    HOLE=$(grep "Hole Cards" "${LOG_DIR}/simulator_p1.log" | wc -l)
    BOARD=$(grep "Board:" "${LOG_DIR}/simulator_p1.log" | wc -l)
    BETTING=$(grep "YOUR TURN" "${LOG_DIR}/simulator_p1.log" | wc -l)
    SHOWDOWN=$(grep "SHOWDOWN" "${LOG_DIR}/simulator_p1.log" | wc -l)
    
    echo "   Connected:        ${CONN}"
    echo "   Hole cards dealt: ${HOLE}"
    echo "   Board cards:      ${BOARD}"
    echo "   Betting actions:  ${BETTING}"
    echo "   Showdowns:        ${SHOWDOWN}"
else
    echo -e "   ${RED}✗ Log file not found${NC}"
fi

echo ""

# Check backend logs for errors
echo "🔍 Backend Health Check:"
ERROR_COUNT=$(grep -i "error\|fail\|crash" "${LOG_DIR}/player1.log" 2>/dev/null | wc -l)
if [ $ERROR_COUNT -eq 0 ]; then
    echo -e "   ${GREEN}✓${NC} No errors in player 1 log"
else
    echo -e "   ${YELLOW}⚠${NC}  Found ${ERROR_COUNT} potential errors in player 1 log"
fi

echo ""

# Summary
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}Test Complete!${NC}"
echo ""
echo "Full logs available in: ${LOG_DIR}/"
echo "  - dealer.log         - Dealer backend"
echo "  - cashier.log        - Cashier backend"
echo "  - player1.log        - Player 1 backend"
echo "  - simulator_p1.log   - Player 1 GUI simulator"
if [ "$MODE" != "quick" ]; then
    echo "  - player2.log        - Player 2 backend"
    echo "  - simulator_p2.log   - Player 2 GUI simulator"
fi
echo ""
echo "To view simulator output:"
echo "  cat ${LOG_DIR}/simulator_p1.log"
echo ""
echo "To view real-time player 1 log:"
echo "  tail -f ${LOG_DIR}/player1.log"
echo ""

