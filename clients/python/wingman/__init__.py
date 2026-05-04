"""
Wingman Python Client
用于远程控制 Wingman 自动化引擎
"""

from .client import (
    WingmanClient,
    WingmanError,
    ConnectionError,
    RequestTimeout,
)

__version__ = '0.1.0'
__all__ = [
    'WingmanClient',
    'WingmanError',
    'ConnectionError',
    'RequestTimeout',
]
