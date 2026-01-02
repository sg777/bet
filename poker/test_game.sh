#!/bin/bash
set -x

cd /root/bet/poker
pkill -f "bin/bet" 2>/dev/null
sleep 2
rm -f /tmp/dealer.log /tmp/player1.log /tmp/player2.log /tmp/cashier.log

echo "=== Starting dealer with reset ==="
./bin/bet dealer reset > /tmp/dealer.log 2>&1 &
DEALER_PID=$!
echo "Dealer PID: $DEALER_PID"
sleep 15

echo "=== Starting player 1 ==="
./bin/bet player config/verus_player_p1.ini > /tmp/player1.log 2>&1 &
P1_PID=$!
echo "Player1 PID: $P1_PID"
sleep 30

echo "=== Starting player 2 ==="
./bin/bet player config/verus_player_p2.ini > /tmp/player2.log 2>&1 &
P2_PID=$!
echo "Player2 PID: $P2_PID"
sleep 30

echo "=== Starting cashier ==="
./bin/bet cashier t1 > /tmp/cashier.log 2>&1 &
CASHIER_PID=$!
echo "Cashier PID: $CASHIER_PID"

echo "=== All nodes started, waiting 3 minutes ==="
sleep 180

echo "=== Results ==="
ls -la /tmp/*.log
echo ""
echo "=== Dealer key events ==="
grep -E "RESET|joined|full|shuffling|BETTING|REVEAL|Card|SHOWDOWN|Its time" /tmp/dealer.log | tail -30
echo ""
echo "=== Running processes ==="
ps aux | grep "bin/bet" | grep -v grep

