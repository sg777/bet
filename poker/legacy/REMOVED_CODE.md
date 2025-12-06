# Removed Legacy Code

This directory contains code that was removed during cleanup:

## Lightning Network Code
- All LN-related functions and variables
- LN configuration flags
- LN error codes (kept for compatibility but unused)

## Nano Sockets (Nanomsg) Code - COMPLETELY REMOVED
- All nanomsg/nano sockets support removed
- NN_PUB/NN_SUB socket types removed
- NN_PUSH/NN_PULL socket types removed
- All nn_send/nn_recv calls removed
- All bet_nanosock() calls removed
- All socket initialization code removed
- All socket fields removed from structures
- pubsub.h, pipeline.h, and all nanomsg includes removed

Note: Communication now uses websockets (libwebsockets) exclusively.
