from setuptools import setup, find_packages

setup(
    name="eai-bci",
    version="0.2.0",
    description="Python bindings for eAI Brain-Computer Interface framework",
    author="EoS Project",
    license="MIT",
    packages=find_packages(),
    python_requires=">=3.8",
    install_requires=[],
    extras_require={
        "numpy": ["numpy>=1.20"],
        "plot": ["matplotlib>=3.5", "pyqtgraph>=0.13"],
    },
)
