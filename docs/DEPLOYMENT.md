# Production Deployment Runbook

## Pre-Deployment Checklist

### Environment Setup
- [ ] Generate strong API key: `openssl rand -hex 32`
- [ ] Create `.env` file from `.env.example`
- [ ] Configure all required environment variables
- [ ] Verify `ENVIRONMENT=production`
- [ ] Leave `CORS_ORIGINS` empty (or explicitly set allowed origins)

### Infrastructure
- [ ] Provision compute resources (min: 2 vCPU, 4GB RAM per service)
- [ ] Set up persistent volumes for database
- [ ] Configure backup storage
- [ ] Set up TLS certificates
- [ ] Configure reverse proxy/load balancer
- [ ] Set up log aggregation (ELK/Splunk/CloudWatch)
- [ ] Configure monitoring (Prometheus + Grafana)
- [ ] Set up alerting (PagerDuty/Opsgenie)

### Security
- [ ] Review and apply security hardening
- [ ] Configure firewall rules
- [ ] Enable security headers on proxy
- [ ] Test API key authentication
- [ ] Verify rate limiting works
- [ ] Test CORS configuration
- [ ] Run security scans (bandit, trivy)

## Deployment Steps

### 1. Build Images

```bash
# Build multi-platform images
docker buildx build --platform linux/amd64,linux/arm64 \
  -t ghcr.io/dawsonblock/brain-ai-rest-service:latest \
  --push brain-ai-rest-service/

docker buildx build --platform linux/amd64,linux/arm64 \
  -t ghcr.io/dawsonblock/deepseek-ocr-service:latest \
  --push deepseek-ocr-service/
```

### 2. Deploy Services

```bash
# On production server
cd /opt/c-ai-brain

# Pull latest images
docker compose pull

# Start services
docker compose up -d

# Wait for services to be ready
sleep 10
```

### 3. Verify Deployment

```bash
# Check containers are running
docker compose ps

# Check health endpoints
curl -f http://localhost:8000/health
curl -f http://localhost:8001/health

# Check readiness
curl -f http://localhost:8000/ready

# Verify metrics
curl http://localhost:8000/metrics | head -n 20

# Check logs
docker compose logs --tail=50
```

### 4. Smoke Tests

```bash
# Test authentication
curl -X POST http://localhost:8000/calculate \
  -H "X-API-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"expression": "2+2"}'

# Test document indexing
curl -X POST http://localhost:8000/index \
  -H "X-API-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"document_id": "test", "text": "smoke test"}'

# Test query
curl -X POST http://localhost:8000/query \
  -H "X-API-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"query": "test", "top_k": 5}'
```

## Post-Deployment

### Monitoring Setup

1. **Configure Prometheus scraping**:
```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'brain-ai-rest'
    static_configs:
      - targets: ['localhost:8000']
    metrics_path: '/metrics'
    scrape_interval: 15s
```

2. **Set up alerts**:
```yaml
# alerts.yml
groups:
  - name: brain-ai
    rules:
      - alert: HighErrorRate
        expr: rate(brain_ai_requests_total{status=~"5.."}[5m]) > 0.05
        for: 5m
        annotations:
          summary: "High error rate detected"
```

3. **Configure Grafana dashboard** - Import dashboard ID or create custom

### Backup Configuration

```bash
# Add to crontab
0 2 * * * /opt/c-ai-brain/scripts/backup-db.sh

# backup-db.sh
#!/bin/bash
DATE=$(date +%Y%m%d_%H%M%S)
docker exec brain-ai-rest sqlite3 /data/brain_ai.db ".backup /data/backup_$DATE.db"
aws s3 cp /var/lib/docker/volumes/brain-ai-data/_data/backup_$DATE.db \
  s3://backups/brain-ai/$DATE.db
```

## Rollback Procedure

### Quick Rollback

```bash
# Stop current version
docker compose down

# Revert to previous image
docker compose pull --ignore-pull-failures
docker tag ghcr.io/dawsonblock/brain-ai-rest-service:previous \
  ghcr.io/dawsonblock/brain-ai-rest-service:latest

# Restart with previous version
docker compose up -d

# Verify
curl http://localhost:8000/health
```

### Full Rollback with Database Restore

```bash
# Stop services
docker compose down

# Restore database backup
cp /backup/brain_ai_YYYYMMDD.db /var/lib/docker/volumes/brain-ai-data/_data/brain_ai.db

# Start previous version
docker compose up -d

# Verify data integrity
curl -X POST http://localhost:8000/query \
  -H "X-API-Key: $API_KEY" \
  -d '{"query": "test"}'
```

## Troubleshooting

### Service Won't Start

```bash
# Check logs
docker compose logs brain-ai-rest
docker compose logs deepseek-ocr

# Check container status
docker compose ps

# Verify environment variables
docker compose config

# Check disk space
df -h

# Check memory
free -h
```

### Health Check Failing

```bash
# Check database
docker exec brain-ai-rest sqlite3 /data/brain_ai.db "PRAGMA integrity_check;"

# Check file permissions
docker exec brain-ai-rest ls -la /data

# Check network connectivity
docker exec brain-ai-rest ping -c 3 deepseek-ocr
```

### High Memory Usage

```bash
# Check memory usage
docker stats

# Restart service with new limits
docker compose up -d --force-recreate --scale brain-ai-rest=1
```

### Database Locked Errors

```bash
# Check WAL mode
docker exec brain-ai-rest sqlite3 /data/brain_ai.db "PRAGMA journal_mode;"

# Checkpoint WAL file
docker exec brain-ai-rest sqlite3 /data/brain_ai.db "PRAGMA wal_checkpoint(TRUNCATE);"
```

## Scaling

### Horizontal Scaling (Multiple Instances)

```yaml
# docker-compose.yml
services:
  brain-ai-rest:
    deploy:
      replicas: 3
```

### Load Balancer Configuration (nginx)

```nginx
upstream brain_ai_backend {
    least_conn;
    server localhost:8000;
    server localhost:8001;
    server localhost:8002;
}
```

## Maintenance Windows

### Planned Maintenance

1. **Announce maintenance window** (24-48 hours notice)
2. **Stop accepting new requests** (set load balancer to drain)
3. **Wait for active requests to complete**
4. **Perform maintenance**
5. **Verify service health**
6. **Resume normal operations**
7. **Monitor for issues**

### Database Maintenance

```bash
# Vacuum database (during low traffic)
docker exec brain-ai-rest sqlite3 /data/brain_ai.db "VACUUM;"

# Optimize database
docker exec brain-ai-rest sqlite3 /data/brain_ai.db "ANALYZE;"

# Check integrity
docker exec brain-ai-rest sqlite3 /data/brain_ai.db "PRAGMA integrity_check;"
```

## Emergency Contacts

- **On-call Engineer**: [phone/pager]
- **DevOps Lead**: [contact]
- **Security Team**: [contact]
- **Infrastructure**: [contact]

## Related Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Security Policy](SECURITY.md)
- [Contributing Guidelines](CONTRIBUTING.md)

---

**Last Updated**: 2025-11-07  
**Version**: 1.0.0
