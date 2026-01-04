#!/usr/bin/env node
/**
 * GUI Simulator for Pangea-Bet Backend Testing
 * 
 * This tool simulates a GUI client connecting to the backend WebSocket server.
 * It automatically responds to betting prompts and monitors game flow.
 * 
 * Usage:
 *   node gui_simulator.js [player_port] [action_mode]
 * 
 * Examples:
 *   node gui_simulator.js 9001 check     # Connect to port 9001, always check
 *   node gui_simulator.js 9001 random    # Connect to port 9001, random actions
 *   node gui_simulator.js 9001 call      # Connect to port 9001, always call
 */

const WebSocket = require('ws');

// Configuration
const PORT = process.argv[2] || '9001';
const ACTION_MODE = process.argv[3] || 'check';  // check, call, random, fold
const BACKEND_URL = `ws://localhost:${PORT}`;

let messageCount = 0;
let gameStarted = false;
let connectedSince = null;

console.log('╔' + '═'.repeat(68) + '╗');
console.log('║' + ' '.repeat(20) + 'GUI SIMULATOR' + ' '.repeat(35) + '║');
console.log('╚' + '═'.repeat(68) + '╝');
console.log('');
console.log(`Backend URL:    ${BACKEND_URL}`);
console.log(`Action Mode:    ${ACTION_MODE}`);
console.log(`Node Type:      Player`);
console.log('');

const ws = new WebSocket(BACKEND_URL);

// Random action selector
function getRandomAction(possibilities, pot, toCall, minRaise) {
  const actions = ['fold', 'check', 'call', 'raise'];
  
  if (ACTION_MODE === 'random') {
    // Weighted random: prefer call/check over fold/raise
    const weights = [10, 40, 40, 10];  // fold, check, call, raise
    const total = weights.reduce((a, b) => a + b, 0);
    let rand = Math.random() * total;
    
    for (let i = 0; i < actions.length; i++) {
      rand -= weights[i];
      if (rand <= 0) {
        // Validate action is possible
        if (actions[i] === 'check' && toCall > 0) return 'call';
        if (actions[i] === 'raise' && toCall === 0) return 'bet';
        return actions[i];
      }
    }
    return 'call';
  }
  
  return ACTION_MODE;
}

ws.on('open', function open() {
  connectedSince = new Date();
  console.log('✅ Connected to backend!');
  console.log(`   Connection time: ${connectedSince.toLocaleTimeString()}`);
  console.log('');
  
  // Simulate GUI startup sequence
  console.log('📤 Sending initial requests...');
  
  ws.send(JSON.stringify({ method: 'backend_status' }));
  console.log('   → backend_status');
  
  setTimeout(() => {
    ws.send(JSON.stringify({ method: 'walletInfo' }));
    console.log('   → walletInfo');
  }, 500);
  
  setTimeout(() => {
    ws.send(JSON.stringify({ method: 'get_bal_info' }));
    console.log('   → get_bal_info');
  }, 1000);
  
  // Auto-join table after 2 seconds (for automated testing)
  setTimeout(() => {
    ws.send(JSON.stringify({ method: 'join_table' }));
    console.log('   → join_table (auto-join for testing)');
  }, 2000);
  
  console.log('');
  console.log('🔄 Monitoring for game messages...');
  console.log('   (Will auto-respond to betting prompts)');
  console.log('');
  console.log('─'.repeat(70));
  console.log('');
});

ws.on('message', function incoming(data) {
  messageCount++;
  const timestamp = new Date().toLocaleTimeString();
  
  try {
    const parsed = JSON.parse(data);
    
    // Compact logging for different message types
    if (parsed.method === 'backend_status') {
      console.log(`[${timestamp}] 📊 Backend Status: ${parsed.backend_status}`);
      
    } else if (parsed.method === 'warning') {
      console.log(`[${timestamp}] ⚠️  Warning: ${parsed.warning_num}`);
      
    } else if (parsed.method === 'bal_info') {
      console.log(`[${timestamp}] 💰 Balance: ${parsed.chips_bal} CHIPS`);
      
    } else if (parsed.method === 'walletInfo') {
      console.log(`[${timestamp}] 👛 Wallet: ${parsed.balance} CHIPS, Stake: ${parsed.table_min_stake}, Blinds: ${parsed.small_blind}/${parsed.big_blind}`);
      
    } else if (parsed.method === 'join_ack') {
      console.log(`[${timestamp}] ✅ Join Approved: ${parsed.message}`);
      
    } else if (parsed.method === 'deal') {
      if (parsed.deal && parsed.deal.holecards) {
        console.log(`[${timestamp}] 🃏 Hole Cards: [${parsed.deal.holecards.join(', ')}]`);
        console.log(`                Balance: ${parsed.deal.balance || 'N/A'}`);
      } else if (parsed.deal && parsed.deal.board) {
        console.log(`[${timestamp}] 🎴 Board: [${parsed.deal.board.join(', ')}]`);
      }
      gameStarted = true;
      
    } else if (parsed.method === 'betting') {
      if (parsed.action === 'round_betting') {
        // It's our turn to act!
        console.log('');
        console.log(`[${timestamp}] 🎯 YOUR TURN TO ACT`);
        console.log(`                Player: ${parsed.playerid}`);
        console.log(`                Pot: ${parsed.pot || 0}`);
        console.log(`                To Call: ${parsed.toCall || 0}`);
        console.log(`                Min Raise: ${parsed.minRaiseTo || 0}`);
        
        if (parsed.player_funds) {
          console.log(`                Player Funds: [${parsed.player_funds.join(', ')}]`);
        }
        
        // Auto-respond
        setTimeout(() => {
          const action = getRandomAction(
            parsed.possibilities,
            parsed.pot || 0,
            parsed.toCall || 0,
            parsed.minRaiseTo || 0
          );
          
          let betAmount = 0;
          if (action === 'call') {
            betAmount = parsed.toCall || 0;
          } else if (action === 'raise') {
            betAmount = parsed.minRaiseTo || (parsed.toCall * 2);
          } else if (action === 'bet') {
            betAmount = parsed.minRaiseTo || 0.02;
          }
          
          const response = {
            method: 'betting',
            playerid: parsed.playerid || 0,
            round: parsed.round || 0,
            min_amount: parsed.toCall || 0,
            possibilities: parsed.possibilities || [],
            action: action,
            bet_amount: betAmount
          };
          
          console.log(`                🤖 Auto-Response: ${action.toUpperCase()}`);
          if (betAmount > 0) {
            console.log(`                Amount: ${betAmount}`);
          }
          ws.send(JSON.stringify(response));
        }, 1500);  // 1.5 second delay to simulate thinking
        
      } else {
        // Another player's action
        console.log(`[${timestamp}] 🎲 Player ${parsed.playerid}: ${parsed.action.toUpperCase()}`);
        if (parsed.bet_amount) {
          console.log(`                Amount: ${parsed.bet_amount}`);
        }
      }
      
    } else if (parsed.method === 'finalInfo') {
      console.log('');
      console.log(`[${timestamp}] 🏆 SHOWDOWN`);
      console.log(`                Winners: [${parsed.winners?.join(', ') || 'N/A'}]`);
      console.log(`                Win Amount: ${parsed.win_amount || 0}`);
      if (parsed.showInfo) {
        console.log(`                All Hands: ${JSON.stringify(parsed.showInfo.allHoleCardsInfo || [])}`);
        console.log(`                Board: ${JSON.stringify(parsed.showInfo.boardCardInfo || [])}`);
      }
      
    } else {
      // Unknown message type - show full JSON
      console.log(`[${timestamp}] 📨 Message #${messageCount}:`);
      console.log(JSON.stringify(parsed, null, 2));
    }
    
  } catch (e) {
    console.log(`[${timestamp}] 📨 Raw: ${data.toString()}`);
  }
});

ws.on('error', function error(err) {
  console.log('');
  console.log('❌ WebSocket Error:', err.message);
  console.log('   Make sure backend is running with --gui flag');
  console.log('   Command: ./bin/bet player config/verus_player_p1.ini --gui');
});

ws.on('close', function close(code, reason) {
  const duration = connectedSince ? (Date.now() - connectedSince) / 1000 : 0;
  
  console.log('');
  console.log('─'.repeat(70));
  console.log('');
  console.log('🔌 Connection Closed');
  console.log(`   Code: ${code}`);
  if (reason) console.log(`   Reason: ${reason}`);
  console.log(`   Duration: ${duration.toFixed(1)}s`);
  console.log(`   Messages received: ${messageCount}`);
  console.log(`   Game started: ${gameStarted ? 'Yes' : 'No'}`);
  console.log('');
  process.exit(0);
});

ws.on('pong', function pong() {
  // Silent pong - connection is alive
});

// Keep connection alive with pings
const pingInterval = setInterval(() => {
  if (ws.readyState === WebSocket.OPEN) {
    ws.ping();
  }
}, 30000);

// Handle Ctrl+C gracefully
process.on('SIGINT', () => {
  console.log('');
  console.log('⚠️  Interrupted by user. Closing...');
  clearInterval(pingInterval);
  ws.close();
});

// Auto-close after 5 minutes
setTimeout(() => {
  console.log('');
  console.log('⏰ Test timeout (5 minutes). Closing...');
  clearInterval(pingInterval);
  ws.close();
}, 300000);

