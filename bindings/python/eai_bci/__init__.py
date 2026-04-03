"""eAI BCI Python Bindings — ctypes wrapper for libeai_bci"""

from .core import BciLib, BciHandle
from .pipeline import BciPipeline

__version__ = "0.2.0"
__all__ = ["BciLib", "BciHandle", "BciPipeline"]
