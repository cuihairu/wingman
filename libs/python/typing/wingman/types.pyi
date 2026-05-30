from __future__ import annotations

from typing import Dict, List, NotRequired, TypedDict, Union

JSONValue = Union[None, bool, int, float, str, List["JSONValue"], Dict[str, "JSONValue"]]

class Point(TypedDict):
    x: int
    y: int

class Rect(TypedDict):
    x: int
    y: int
    width: int
    height: int

class Color(TypedDict):
    r: int
    g: int
    b: int
    a: NotRequired[int]

class HttpOptions(TypedDict, total=False):
    timeout: int
    followRedirects: bool
    maxRedirects: int
    headers: Dict[str, str]

class HttpResponse(TypedDict):
    status: int
    body: str
    elapsed: float
    success: bool
    headers: Dict[str, str]
    error: NotRequired[str]

class QRLoginResult(TypedDict, total=False):
    state: str
    message: str
    token: str
    sessionId: str
    data: JSONValue

