# Brain-AI Python Wheel

This directory provides the packaging glue that builds the `brain_ai_py`
pybind11 module into a manylinux-compatible wheel using
[`scikit-build-core`](https://scikit-build-core.readthedocs.io/).

The build delegates to the top-level CMake project with
`BUILD_PYTHON_BINDINGS=ON` and installs the resulting extension module into the
wheel payload. To produce a wheel locally:

```bash
cd brain-ai/python
python -m build
```

The output wheel will appear under `dist/` and can be copied into
`brain-ai-rest-service/wheels/` or published to an artifact repository.
