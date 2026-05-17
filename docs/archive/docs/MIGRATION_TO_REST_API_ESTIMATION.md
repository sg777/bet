# Migration from CHIPS CLI to REST API - Effort Estimation

## Current Implementation

### Architecture
- **Method**: CLI commands executed via `popen()` system calls
- **Command Format**: `chips-cli <method> <params>` or `verus -chain=chips <method> <params>`
- **Execution**: `make_command()` function in `poker/src/commands.c` (lines 1608-1770)
- **Functions Affected**: ~47 CHIPS-related functions (see `poker/include/commands.h`)

### CHIPS RPC APIs Currently Used (22 APIs)
From `docs/protocol/upgrade.md`:
1. `addmultisigaddress`
2. `createrawtransaction`
3. `decoderawtransaction`
4. `getbalance`
5. `getblock`
6. `getblockcount`
7. `getnewaddress`
8. `getrawtransaction`
9. `gettransaction`
10. `importaddress`
11. `listaddressgroupings`
12. `listunspent`
13. `sendrawtransaction`
14. `signrawtransactionwithwallet`
15. `validateaddress`
16. `getidentity` (Verus-specific)
17. `updateidentity` (Verus-specific)
18. `getidentitycontent` (Verus-specific)
19. Additional Verus VDXF APIs (if any)

### Key Files
- `poker/src/commands.c` (~1936 lines) - Main implementation
- `poker/include/commands.h` - Function declarations
- `poker/src/config.c` - Configuration (blockchain_cli setting)
- All files that call CHIPS functions

## Migration Scope

### Phase 1: Infrastructure Setup (Medium Effort)

**Tasks:**
1. **HTTP Client Library Integration**
   - Add HTTP client library (libcurl recommended, ~100KB dependency)
   - Create REST API client wrapper module
   - Handle JSON-RPC request/response format
   - Implement authentication (HTTP Basic Auth with RPC user/password)
   - Error handling for HTTP status codes and network issues

**Estimation:** 2-3 days
**Complexity:** Medium
**Dependencies:** libcurl library (may need to add to build system)

### Phase 2: Core REST API Client (Medium-High Effort)

**Tasks:**
1. **Create REST API Module** (`poker/src/chips_rpc.c`, `poker/include/chips_rpc.h`)
   - Generic JSON-RPC request builder
   - HTTP request/response handler
   - Response parsing and error handling
   - Connection pooling/management
   - Timeout handling

2. **Configuration Updates**
   - Add REST API endpoint configuration (URL, port, auth)
   - Backward compatibility: support both CLI and REST modes
   - Environment detection (local vs remote CHIPS node)

**Estimation:** 3-5 days
**Complexity:** Medium-High
**Risk:** HTTP client reliability, error handling complexity

### Phase 3: Function Migration (High Effort)

**Tasks:**
1. **Migrate Core Functions** (~47 functions)
   - Replace `make_command()` calls with REST API calls
   - Update each CHIPS function to use REST client
   - Maintain same function signatures for compatibility
   - Error handling migration (CLI errors → HTTP/JSON-RPC errors)

2. **Function Categories to Migrate:**
   - **Address Management** (6 functions): `chips_get_new_address`, `chips_validate_address`, `chips_import_address`, `chips_ismine`, `chips_iswatchonly`, `chips_list_address_groupings`
   - **Transaction Operations** (15+ functions): `chips_transfer_funds`, `chips_create_raw_tx`, `chips_sign_raw_tx`, `chips_send_raw_tx`, `chips_get_raw_tx`, `chips_decode_raw_tx`, etc.
   - **Block Operations** (5 functions): `chips_get_block`, `chips_get_block_hash`, `chips_get_block_count`, etc.
   - **Multisig Operations** (5+ functions): `chips_add_multisig_address`, `chips_sign_msig_tx`, etc.
   - **Balance/UTXO Operations** (4 functions): `chips_get_balance`, `chips_spendable_tx`, `chips_listunspent`, etc.
   - **Verus ID Operations** (10+ functions): All `vdxf.c` functions that use `getidentity`, `updateidentity`, `getidentitycontent`

**Estimation:** 10-15 days
**Complexity:** High
**Risk:** 
- Each function needs individual testing
- Error handling differences (CLI vs REST)
- Response format variations
- Edge cases and error scenarios

### Phase 4: Testing & Validation (High Effort)

**Tasks:**
1. **Unit Testing**
   - Test each migrated function individually
   - Compare REST responses with CLI responses
   - Error scenario testing

2. **Integration Testing**
   - Full game flow testing (dealer, player, cashier)
   - Transaction flow testing
   - Multisig transaction testing
   - Verus ID operations testing

3. **Performance Testing**
   - Latency comparison (CLI vs REST)
   - Concurrent request handling
   - Error recovery testing

**Estimation:** 5-8 days
**Complexity:** High
**Risk:** Uncovered edge cases, performance regressions

### Phase 5: Documentation & Cleanup (Low Effort)

**Tasks:**
1. Update documentation
2. Remove CLI-specific code (optional, can keep for fallback)
3. Update configuration documentation

**Estimation:** 1-2 days
**Complexity:** Low

## Total Effort Estimation

### Conservative Estimate
- **Phase 1**: 3 days
- **Phase 2**: 5 days
- **Phase 3**: 15 days
- **Phase 4**: 8 days
- **Phase 5**: 2 days
- **Buffer (20%)**: 7 days
- **Total: 40 days (~8 weeks)**

### Realistic Estimate (Accounting for Complexity)
- **Phase 1**: 2-3 days
- **Phase 2**: 3-5 days
- **Phase 3**: 10-15 days (some functions are simpler)
- **Phase 4**: 5-8 days
- **Phase 5**: 1-2 days
- **Buffer (25%)**: 6-8 days
- **Total: 27-41 days (~5-8 weeks)**

### Optimistic Estimate (With Prior Experience)
- **Phase 1**: 2 days
- **Phase 2**: 3 days
- **Phase 3**: 8-10 days
- **Phase 4**: 4-5 days
- **Phase 5**: 1 day
- **Buffer (15%)**: 3 days
- **Total: 21-24 days (~4-5 weeks)**

## Complexity Factors

### High Complexity Areas
1. **Transaction Building & Signing**: Complex logic with multisig, data embedding
2. **Verus ID Operations**: Custom VDXF protocol, requires careful handling
3. **Error Handling**: CLI errors vs HTTP/RPC errors need mapping
4. **Concurrency**: Multiple simultaneous requests
5. **State Management**: Connection pooling, retries, timeouts

### Medium Complexity Areas
1. **Address Operations**: Straightforward mapping
2. **Block Queries**: Simple read operations
3. **Balance Queries**: Simple read operations

### Low Complexity Areas
1. **Configuration Changes**: Straightforward
2. **Documentation Updates**: Straightforward

## Risks & Considerations

### Technical Risks
1. **HTTP Client Reliability**: Network timeouts, connection issues
2. **Error Handling**: Different error formats between CLI and REST
3. **Performance**: HTTP overhead vs direct CLI (likely negligible)
4. **Authentication**: Secure credential handling
5. **Backward Compatibility**: Support both modes during transition?

### Dependencies
1. **libcurl** or similar HTTP client library
2. **JSON parsing** (already have cJSON)
3. **SSL/TLS** support for HTTPS (if needed)

### Testing Challenges
1. **Test Environment**: Need CHIPS REST API endpoint for testing
2. **Coverage**: 47 functions × multiple test cases each
3. **Edge Cases**: Network failures, malformed responses, timeouts

## Recommendations

### Approach 1: Complete Migration (Recommended for Long-term)
- Migrate all functions to REST API
- Remove CLI dependency entirely
- **Pros**: Clean architecture, better scalability, remote node support
- **Cons**: Higher initial effort, need HTTP client library

### Approach 2: Hybrid Approach (Recommended for Gradual Migration)
- Keep CLI as fallback, add REST API support
- Migrate functions incrementally
- Configuration flag to choose CLI vs REST
- **Pros**: Lower risk, gradual migration, backward compatible
- **Cons**: More code to maintain, complexity

### Approach 3: Wrapper Layer (Quick Win)
- Create REST API wrapper that mimics CLI interface
- Minimal changes to existing code
- **Pros**: Fastest implementation, minimal code changes
- **Cons**: Less optimal, still has CLI-like patterns

## Implementation Strategy (Recommended)

1. **Week 1-2**: Infrastructure + Core REST Client
   - Add libcurl dependency
   - Create `chips_rpc.c` module
   - Implement basic JSON-RPC client
   - Test with simple commands (getblockcount, getbalance)

2. **Week 3-5**: Function Migration (incrementally)
   - Start with simple read operations (getblock, getrawtransaction)
   - Migrate address operations
   - Migrate transaction operations
   - Migrate Verus ID operations (most complex)

3. **Week 6-7**: Testing & Refinement
   - Comprehensive testing
   - Performance optimization
   - Error handling refinement
   - Documentation

4. **Week 8**: Cleanup & Deployment
   - Final testing
   - Documentation updates
   - Optional: Remove CLI code or keep as fallback

## Success Criteria

1. ✅ All 22+ CHIPS APIs work via REST
2. ✅ All 47 functions migrated and tested
3. ✅ Performance comparable to CLI (within 10-20ms overhead acceptable)
4. ✅ Error handling comprehensive
5. ✅ Documentation updated
6. ✅ Backward compatibility (if hybrid approach)

## Conclusion

**Recommended Estimate: 5-8 weeks (25-40 working days)** for a complete, production-ready migration.

This assumes:
- Experienced C developer familiar with HTTP clients
- Good test coverage
- Access to CHIPS REST API endpoint for testing
- Reasonable code review and iteration cycles

The migration is **feasible** but requires **significant effort** due to:
- Large number of functions (47)
- Complex transaction logic
- Verus ID protocol complexity
- Need for comprehensive testing

**Risk Level**: Medium-High (mitigated by incremental approach and testing)

