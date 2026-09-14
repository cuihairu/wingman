from __future__ import annotations

from typing import TypedDict


class ModelIOInfo(TypedDict):
    name: str
    shape: list[int]


class ModelInput(TypedDict, total=False):
    name: str
    data: list[float]
    shape: list[int]
    dtype: str


class ModelOutput(TypedDict):
    name: str
    shape: list[int]
    data: list[float]


class InferenceResult(TypedDict):
    success: bool
    error: str
    outputs: list[ModelOutput]
    timeMs: float


def providers() -> list[str]: ...

def loadModel(path: str, ep: str = ...) -> str | None: ...
def load_model(path: str, ep: str = ...) -> str | None: ...

def unload(modelId: str) -> bool: ...

def isLoaded(modelId: str) -> bool: ...
def is_loaded(modelId: str) -> bool: ...

def inputs(modelId: str) -> list[ModelIOInfo]: ...
def outputs(modelId: str) -> list[ModelIOInfo]: ...

def run(modelId: str, inputs: list[ModelInput]) -> InferenceResult: ...
