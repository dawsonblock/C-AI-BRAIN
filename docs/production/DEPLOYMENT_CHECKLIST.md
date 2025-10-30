# Brain-AI v3.6 Production Deployment Checklist

## Pre-Deployment (Week -2 to 0)

### Testing
- [ ] All 85 tests passing
- [ ] Load test with 100K queries
- [ ] Memory leak check (Valgrind)
- [ ] Thread safety verification (ThreadSanitizer)
- [ ] Benchmark latency: <10ms p50, <50ms p99
- [ ] Stress test: 1M queries over 24 hours

### Configuration
- [ ] Production `system.yaml` reviewed
- [ ] Secrets management configured (JWT, TLS certs)
- [ ] Resource limits defined (CPU, memory)
- [ ] Backup strategy documented
- [ ] Monitoring dashboards created

### Security
- [ ] Security audit completed
- [ ] mTLS certificates generated
- [ ] Authentication enabled
- [ ] Rate limiting configured
- [ ] Firewall rules reviewed

## Deployment (Week 0)

### Phase 1: Staging (Day 1-2)
- [ ] Deploy to staging environment
- [ ] Verify health checks responding
- [ ] Run smoke tests
- [ ] Check metrics collection
- [ ] Verify logs centralized
- [ ] Test backup/restore procedure

### Phase 2: Canary (Day 3-5)
- [ ] Deploy to 1% production traffic
- [ ] Monitor error rates (<0.1%)
- [ ] Check latency p99 (<50ms)
- [ ] Verify resource usage (<80% limits)
- [ ] No memory leaks observed

### Phase 3: Gradual Rollout (Day 6-10)
- [ ] Increase to 10% traffic (Day 6)
- [ ] Increase to 50% traffic (Day 8)
- [ ] Increase to 100% traffic (Day 10)
- [ ] Monitor at each step (24h observation)

## Post-Deployment (Week 1-4)

### Monitoring (Daily)
- [ ] Check error rates
- [ ] Review latency metrics
- [ ] Verify uptime SLA (99.9%)
- [ ] Monitor memory usage
- [ ] Check log for warnings

### Performance (Weekly)
- [ ] Analyze slow queries
- [ ] Optimize hot paths
- [ ] Review cache hit rates
- [ ] Check graph auto-construction

### Incidents (As Needed)
- [ ] Document all incidents
- [ ] Root cause analysis
- [ ] Implement fixes
- [ ] Update runbooks

## Success Criteria

| Metric | Target | Measurement |
|--------|--------|-------------|
| Uptime SLA | 99.9% | 4-week average |
| Latency p50 | <10ms | Real traffic |
| Latency p99 | <50ms | Real traffic |
| Error rate | <0.1% | 24h window |
| Memory stable | No leaks | Week-long test |

## Rollback Plan

If any success criteria fails:
1. Immediate: Route traffic to previous version
2. Within 1h: Investigate root cause
3. Within 4h: Fix deployed or rollback confirmed
4. Within 24h: Post-mortem document

## Approval Sign-Off

- [ ] Engineering Lead: __________________
- [ ] DevOps Lead: __________________
- [ ] Security Team: __________________
- [ ] Product Manager: __________________

Date: __________________
