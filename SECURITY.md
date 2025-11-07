# Security Policy

## Supported Versions

We release patches for security vulnerabilities for the following versions:

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues.**

Instead, please report them via email to: [security contact email - update this]

You should receive a response within 48 hours. If for some reason you do not, please follow up via email to ensure we received your original message.

Please include the following information in your report:

- Type of vulnerability
- Full paths of source file(s) related to the vulnerability
- Location of the affected source code (tag/branch/commit or direct URL)
- Step-by-step instructions to reproduce the issue
- Proof-of-concept or exploit code (if possible)
- Impact of the issue, including how an attacker might exploit it

## Security Measures

### Application Security

1. **Authentication**
   - API key authentication required for all sensitive endpoints
   - Keys should be rotated quarterly
   - No default/hardcoded credentials

2. **Input Validation**
   - All inputs validated with Pydantic models
   - Request size limits enforced
   - MIME type validation for uploads
   - Safe evaluation for mathematical expressions (no code execution)

3. **Rate Limiting**
   - Per-IP rate limiting enabled by default
   - Configurable limits per endpoint
   - 429 responses for exceeded limits

4. **CORS**
   - Disabled by default in production
   - Must be explicitly configured with allowed origins
   - Never use wildcard (*) with credentials

5. **Security Headers**
   - X-Content-Type-Options: nosniff
   - X-Frame-Options: DENY
   - X-XSS-Protection: 1; mode=block
   - Content-Security-Policy enforced
   - HSTS available for HTTPS deployments

### Container Security

1. **Image Hardening**
   - Base images pinned by SHA256 digest
   - Non-root users (UID 1000, 1001)
   - Read-only root filesystem
   - Minimal attack surface (slim images)

2. **Runtime Security**
   - All capabilities dropped
   - Security options: no-new-privileges
   - Resource limits enforced
   - Health checks for availability

3. **Network Security**
   - Services isolated in Docker network
   - No direct external exposure
   - Deploy behind reverse proxy with TLS

### Data Security

1. **At Rest**
   - SQLite database on encrypted volumes
   - Temporary files automatically cleaned up
   - Sensitive data not logged

2. **In Transit**
   - TLS 1.2+ required for production
   - HTTPS only for external communication
   - Internal service communication over private network

3. **Secrets Management**
   - API keys via environment variables
   - Never commit secrets to Git
   - Use secret management tools in production

### Supply Chain Security

1. **Dependencies**
   - Automated vulnerability scanning (pip-audit, safety)
   - Dependabot for automated updates
   - Version pinning in requirements.txt

2. **CI/CD**
   - GitHub Actions with pinned commit SHAs
   - Bandit for Python security linting
   - Trivy for container and filesystem scanning
   - CodeQL for static analysis
   - SBOM generation for transparency

3. **Container Registry**
   - Images signed and verified
   - Scan on push and scheduled
   - Use official base images only

## Secure Configuration

### Production Checklist

- [ ] Strong API key generated and configured
- [ ] CORS origins explicitly set (or disabled)
- [ ] TLS certificates configured on proxy
- [ ] ENVIRONMENT=production
- [ ] LOG_LEVEL=INFO or WARNING
- [ ] SECURE_COOKIES=true (if using HTTPS)
- [ ] HSTS_ENABLED=true (if using HTTPS)
- [ ] Metrics endpoint protected or firewalled
- [ ] Database on encrypted persistent volume
- [ ] Regular backups configured
- [ ] Log aggregation configured
- [ ] Monitoring and alerting set up
- [ ] Secrets managed via vault/secrets manager
- [ ] Network policies/firewalls configured
- [ ] Rate limiting enabled
- [ ] No debug/docs endpoints exposed

### Environment Variables

**Required**:
- `API_KEY` - Strong random key (min 32 chars)

**Security-Related**:
- `CORS_ORIGINS` - Comma-separated allowed origins (empty = disabled)
- `ENVIRONMENT` - Set to "production" for production
- `SECURE_COOKIES` - Enable for HTTPS
- `HSTS_ENABLED` - Enable for HTTPS
- `RATE_LIMIT_ENABLED` - Default true
- `METRICS_ENABLED` - Default true (protect endpoint)

## Vulnerability Disclosure Timeline

1. **Day 0**: Vulnerability reported
2. **Day 2**: Acknowledgment sent to reporter
3. **Day 7**: Initial assessment and severity rating
4. **Day 14**: Fix developed and tested
5. **Day 21**: Fix released and public disclosure
6. **Day 28**: Follow-up and lessons learned

Timelines may be adjusted based on severity and complexity.

## Security Best Practices

### For Developers

1. **Never use `eval()` or `exec()`**
   - Use AST-based safe evaluation
   - Validate all dynamic code paths

2. **Validate all inputs**
   - Use Pydantic models
   - Set reasonable limits
   - Sanitize before logging

3. **Least privilege**
   - Minimize container capabilities
   - Use non-root users
   - Restrict file permissions

4. **Secure defaults**
   - Disable unnecessary features
   - Fail secure
   - Require explicit opt-in for risky operations

5. **Log security events**
   - Authentication failures
   - Rate limit violations
   - Input validation errors
   - Unexpected errors

### For Operators

1. **Regular updates**
   - Apply security patches promptly
   - Monitor Dependabot PRs
   - Subscribe to security advisories

2. **Monitor and alert**
   - Failed authentication attempts
   - Unusual traffic patterns
   - Error rate spikes
   - Resource exhaustion

3. **Incident response plan**
   - Document procedures
   - Contact information
   - Escalation paths
   - Post-incident review

4. **Regular audits**
   - Quarterly configuration reviews
   - Annual penetration testing
   - Log analysis
   - Access reviews

## Known Limitations

1. **SQLite Concurrency**
   - Single writer limitation
   - Consider PostgreSQL for high write loads

2. **API Key Authentication**
   - Shared secret model
   - No per-user access control
   - Consider OAuth2 for multi-tenant

3. **Rate Limiting**
   - Per-IP only (can be evaded)
   - No distributed rate limiting
   - Consider API gateway for advanced controls

4. **No WAF**
   - Application handles security
   - Consider Cloudflare or AWS WAF for additional protection

## Security Contacts

- **Security Issues**: [security email]
- **General Questions**: [general email]
- **Urgent Issues**: [emergency contact]

## Acknowledgments

We thank the security research community for responsible disclosure and helping keep this project secure.

---

**Last Updated**: 2025-11-07
**Version**: 1.0.0
