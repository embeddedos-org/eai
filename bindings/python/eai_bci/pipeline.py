"""Context-manager pipeline for convenient BCI usage."""

from .core import BciLib, BciHandle


class BciPipeline:
    """High-level BCI pipeline with context manager support.

    Usage:
        with BciPipeline(simulator=True) as bci:
            for _ in range(100):
                bci.poll()
                intent = bci.get_intent()
                print(f"{intent['label']} ({intent['confidence']:.2f})")
    """

    def __init__(self, device="simulator", decoder="threshold", output="log",
                 lib_path=None, simulator=True):
        if simulator:
            device = "simulator"
        self._lib = BciLib(lib_path)
        self._handle = BciHandle(self._lib, device, decoder, output)

    def __enter__(self):
        self._handle.start()
        return self

    def __exit__(self, *args):
        self._handle.stop()
        self._handle.destroy()

    def poll(self):
        return self._handle.poll()

    def get_intent(self):
        return self._handle.get_intent()

    def get_signal(self, max_samples=256):
        return self._handle.get_signal(max_samples)

    @property
    def channel_count(self):
        return self._handle.channel_count

    @property
    def sample_rate(self):
        return self._handle.sample_rate

    @property
    def samples_processed(self):
        return self._handle.samples_processed
