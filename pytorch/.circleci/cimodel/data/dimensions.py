PHASES = ["build", "test"]

CUDA_VERSIONS = [
    "102",
    "111",
    "113",
    "115",
]

ROCM_VERSIONS = [
    "4.1",
    "4.2",
    "4.3.1",
]

ROCM_VERSION_LABELS = ["rocm" + v for v in ROCM_VERSIONS]

GPU_VERSIONS = [None] + ["cuda" + v for v in CUDA_VERSIONS] + ROCM_VERSION_LABELS

STANDARD_PYTHON_VERSIONS = [
    "3.6",
    "3.7",
    "3.8",
    "3.9"
]
