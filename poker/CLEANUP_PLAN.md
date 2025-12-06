# Code Cleanup Plan

## 1. Lightning Network Code Removal
- Remove 18 LN functions from commands.c
- Remove bet_ln_config conditionals from all files
- Remove LN-related includes and error handling
- Update functions that call LN code

## 2. Nano Sockets Pub/Sub Removal  
- Remove NN_PUB/NN_SUB socket types
- Remove pubsub.h include
- Replace pub/sub with PUSH/PULL where needed
- Remove pubsock/subsock variables and operations

## 3. Dead Code Removal
- Find unused functions (need static analysis)
- Find unused structures
- Find unused variables
- Remove commented-out code blocks

Estimated impact: ~2000+ lines to remove/modify
