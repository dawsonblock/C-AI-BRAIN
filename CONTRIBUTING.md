# Contributing to C-AI-BRAIN

Thank you for your interest in contributing to C-AI-BRAIN! This document provides guidelines and instructions for contributing.

## Code of Conduct

By participating in this project, you agree to maintain a respectful, inclusive, and harassment-free environment for everyone.

## How Can I Contribute?

### Reporting Bugs

Before creating bug reports, please check existing issues to avoid duplicates.

**When reporting a bug, include:**
- Clear, descriptive title
- Steps to reproduce
- Expected vs actual behavior
- Environment details (OS, Docker version, etc.)
- Relevant logs or error messages
- Screenshots if applicable

### Suggesting Enhancements

Enhancement suggestions are welcome! Please include:
- Clear use case description
- Why this enhancement would be useful
- Possible implementation approach
- Any potential drawbacks or concerns

### Pull Requests

1. **Fork and Clone**
   ```bash
   git clone https://github.com/your-username/C-AI-BRAIN.git
   cd C-AI-BRAIN
   ```

2. **Create Branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **Make Changes**
   - Follow existing code style
   - Add tests for new features
   - Update documentation as needed
   - Ensure all tests pass

4. **Commit Changes**
   ```bash
   git commit -m "feat: add amazing feature"
   ```
   
   Use conventional commit messages:
   - `feat:` New feature
   - `fix:` Bug fix
   - `docs:` Documentation changes
   - `test:` Test additions/changes
   - `refactor:` Code refactoring
   - `perf:` Performance improvements
   - `ci:` CI/CD changes
   - `chore:` Maintenance tasks

5. **Push and Create PR**
   ```bash
   git push origin feature/your-feature-name
   ```

## Development Setup

### Prerequisites
- Python 3.11+
- Docker 20.10+
- Docker Compose 2.0+

### Local Environment

```bash
# Create virtual environment
python -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate

# Install dependencies
cd brain-ai-rest-service
pip install -r requirements.txt
pip install pytest pytest-asyncio pytest-cov black isort flake8 mypy bandit

# Install pre-commit hooks (recommended)
pip install pre-commit
pre-commit install
```

### Running Tests

```bash
# Unit tests
pytest tests/ -v --cov

# Integration tests (requires running services)
docker compose up -d
pytest tests/integration/ -v
docker compose down -v

# Security scans
bandit -r brain-ai-rest-service/app deepseek-ocr-service
pip-audit -r brain-ai-rest-service/requirements.txt
```

### Code Quality

```bash
# Format code
black brain-ai-rest-service/app deepseek-ocr-service

# Sort imports
isort brain-ai-rest-service/app deepseek-ocr-service

# Lint
flake8 brain-ai-rest-service/app deepseek-ocr-service --max-line-length=100

# Type check
mypy brain-ai-rest-service/app deepseek-ocr-service --ignore-missing-imports
```

## Coding Standards

### Python

- **Style**: Follow PEP 8, enforced by Black
- **Type Hints**: Use type annotations for all functions
- **Docstrings**: Google style for all public APIs
- **Imports**: Sorted with isort
- **Line Length**: 100 characters max
- **Testing**: 80%+ code coverage required

### Security

- **No Dynamic Evaluation**: Never use `eval()`, `exec()`, or `compile()` directly
- **Input Validation**: Validate all user inputs with Pydantic
- **SQL Safety**: Use parameterized queries, never string concatenation
- **Secrets**: Never commit secrets, use environment variables
- **Dependencies**: Keep dependencies up to date
- **Logging**: Never log sensitive data

### Documentation

- Update README.md for user-facing changes
- Update ARCHITECTURE.md for architectural changes
- Add docstrings for all public APIs
- Include examples in docstrings
- Update CHANGELOG.md

## Testing Requirements

### Required Tests

- **Unit Tests**: Test individual functions/methods
- **Integration Tests**: Test service interactions
- **Security Tests**: Test security controls
- **Edge Cases**: Test boundary conditions

### Test Structure

```python
def test_feature_description():
    """Test that feature works correctly."""
    # Arrange
    setup_data = ...
    
    # Act
    result = function_under_test(setup_data)
    
    # Assert
    assert result == expected_value
```

### Coverage

- Aim for 80%+ code coverage
- Critical security code must have 100% coverage
- Include both positive and negative test cases

## Pull Request Process

1. **Before Submitting**
   - [ ] All tests pass locally
   - [ ] Code formatted with Black and isort
   - [ ] No linting errors from flake8
   - [ ] Security scan passes (bandit)
   - [ ] Documentation updated
   - [ ] CHANGELOG.md updated

2. **PR Description**
   - Clear description of changes
   - Link to related issues
   - Breaking changes highlighted
   - Security implications noted
   - Screenshots/examples if applicable

3. **Review Process**
   - CI/CD checks must pass
   - At least one maintainer approval required
   - Address review comments
   - Keep PR scope focused

4. **After Merge**
   - PR branch will be deleted
   - Thank you for your contribution! 🎉

## Security Contributions

- **Critical**: Report security vulnerabilities privately via email
- **See**: [SECURITY.md](SECURITY.md) for reporting process
- **Recognition**: Security researchers acknowledged in releases

## Questions?

- Open an issue with `question` label
- Start a discussion in GitHub Discussions
- Contact maintainers directly

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.

---

Thank you for contributing to C-AI-BRAIN! 🚀
