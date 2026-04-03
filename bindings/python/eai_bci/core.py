"""Core ctypes FFI wrapper for libeai_bci C API."""

import ctypes
import os
import platform
from pathlib import Path


def _find_library():
    """Locate the shared library."""
    if platform.system() == "Windows":
        name = "eai_bci_shared.dll"
    elif platform.system() == "Darwin":
        name = "libeai_bci_shared.dylib"
    else:
        name = "libeai_bci_shared.so"

    search_paths = [
        Path(__file__).parent.parent.parent.parent / "build" / "bci",
        Path(__file__).parent.parent.parent.parent / "build",
        Path(os.environ.get("EAI_BCI_LIB_PATH", "")),
    ]

    for p in search_paths:
        lib_path = p / name
        if lib_path.exists():
            return str(lib_path)

    return name


class BciLib:
    """Wrapper around the eai_bci C shared library."""

    def __init__(self, lib_path=None):
        path = lib_path or _find_library()
        self._lib = ctypes.CDLL(path)
        self._setup_signatures()

    def _setup_signatures(self):
        lib = self._lib

        lib.eai_bci_create.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        lib.eai_bci_create.restype = ctypes.c_void_p

        lib.eai_bci_start.argtypes = [ctypes.c_void_p]
        lib.eai_bci_start.restype = ctypes.c_int

        lib.eai_bci_stop.argtypes = [ctypes.c_void_p]
        lib.eai_bci_stop.restype = ctypes.c_int

        lib.eai_bci_poll.argtypes = [ctypes.c_void_p]
        lib.eai_bci_poll.restype = ctypes.c_int

        lib.eai_bci_get_intent.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int,
            ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_uint32)
        ]
        lib.eai_bci_get_intent.restype = ctypes.c_int

        lib.eai_bci_get_signal.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
            ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int)
        ]
        lib.eai_bci_get_signal.restype = ctypes.c_int

        lib.eai_bci_get_channel_count.argtypes = [ctypes.c_void_p]
        lib.eai_bci_get_channel_count.restype = ctypes.c_int

        lib.eai_bci_get_sample_rate.argtypes = [ctypes.c_void_p]
        lib.eai_bci_get_sample_rate.restype = ctypes.c_int

        lib.eai_bci_get_samples_processed.argtypes = [ctypes.c_void_p]
        lib.eai_bci_get_samples_processed.restype = ctypes.c_uint64

        lib.eai_bci_destroy.argtypes = [ctypes.c_void_p]
        lib.eai_bci_destroy.restype = None

        lib.eai_bci_version.argtypes = []
        lib.eai_bci_version.restype = ctypes.c_char_p

    def version(self):
        return self._lib.eai_bci_version().decode()


class BciHandle:
    """Opaque handle wrapping a BCI pipeline instance."""

    def __init__(self, lib: BciLib, device="simulator", decoder="threshold", output="log"):
        self._lib = lib._lib
        self._handle = self._lib.eai_bci_create(
            device.encode(), decoder.encode(), output.encode()
        )
        if not self._handle:
            raise RuntimeError("Failed to create BCI handle")

    def start(self):
        if self._lib.eai_bci_start(self._handle) != 0:
            raise RuntimeError("Failed to start BCI pipeline")

    def stop(self):
        self._lib.eai_bci_stop(self._handle)

    def poll(self):
        return self._lib.eai_bci_poll(self._handle) == 0

    def get_intent(self):
        label = ctypes.create_string_buffer(64)
        conf = ctypes.c_float()
        cls = ctypes.c_uint32()
        if self._lib.eai_bci_get_intent(self._handle, label, 64,
                                         ctypes.byref(conf), ctypes.byref(cls)) != 0:
            return None
        return {"label": label.value.decode(), "confidence": conf.value, "class_id": cls.value}

    def get_signal(self, max_samples=256):
        channels = self._lib.eai_bci_get_channel_count(self._handle)
        buf = (ctypes.c_float * (max_samples * channels))()
        got = ctypes.c_int()
        self._lib.eai_bci_get_signal(self._handle, buf, max_samples, channels, ctypes.byref(got))
        return list(buf[:got.value * channels]), got.value, channels

    @property
    def channel_count(self):
        return self._lib.eai_bci_get_channel_count(self._handle)

    @property
    def sample_rate(self):
        return self._lib.eai_bci_get_sample_rate(self._handle)

    @property
    def samples_processed(self):
        return self._lib.eai_bci_get_samples_processed(self._handle)

    def destroy(self):
        if self._handle:
            self._lib.eai_bci_destroy(self._handle)
            self._handle = None

    def __del__(self):
        self.destroy()
