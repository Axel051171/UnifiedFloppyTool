"""
MIG-Flash Block I/O Library
============================

Correct implementation for MIG-Flash cartridge dumper.
MIG-Flash is a USB Mass Storage device, NOT a serial device!

The device presents itself as a standard USB storage device and
communication happens via raw block I/O (sector reads/writes).

Hardware:
- Genesys Logic GL3227 USB Storage Controller
- USB VID: 0x05E3, PID: 0x0751
- ESP32-S2 for cartridge interface
- Switch gamecard slot

Architecture:
    ┌──────────────┐    ┌──────────────┐    ┌──────────────────┐
    │  Switch      │───▶│   ESP32-S2   │───▶│  GL3227 USB      │
    │  Gamecard    │    │   (MCU)      │    │  Mass Storage    │
    └──────────────┘    └──────────────┘    └────────┬─────────┘
                                                     │
                                              USB Mass Storage
                                                     │
                                                     ▼
                                            ┌─────────────────┐
                                            │   HOST PC       │
                                            │   (Block I/O)   │
                                            └─────────────────┘

Author: UFI Project
Version: 2.0.0
Date: 2026-01-20
"""

import os
import sys
import struct
import platform
import logging
import time
import json
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, List, Callable, BinaryIO, Tuple
from enum import IntEnum

#=============================================================================
# LOGGING
#=============================================================================

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

#=============================================================================
# CONSTANTS - MEMORY MAP
#=============================================================================

# USB Device IDs
MIG_USB_VID = 0x05E3       # Genesys Logic
MIG_USB_PID = 0x0751       # USB Storage

# Sector size (standard USB Mass Storage)
SECTOR_SIZE = 512

# GPT Partition GUID for Microsoft Basic Data
GPT_MSDATA_GUID = b'\xA2\xA0\xD0\xEB\xE5\xB9\x33\x44\x87\xC0\x68\xB6\xB7\x26\x99\xC7'

# Memory Map (from migupdater.py analysis)
class MemoryMap:
    """MIG-Flash Memory Layout"""
    
    # Standard disk structures
    MBR_OFFSET              = 0x00000000    # Master Boot Record
    GPT_HEADER_OFFSET       = 0x00000200    # GPT Header
    GPT_PARTITION_OFFSET    = 0x00000400    # GPT Partition Entries
    
    # Firmware area (confirmed from migupdater)
    FIRMWARE_OFFSET         = 0x209A4000    # ~549 MB
    FIRMWARE_SIZE           = 0x00044000    # 278,528 bytes (544 sectors)
    FIRMWARE_VERSION_LEN    = 16
    
    # XCI Storage area (estimated - needs verification)
    # These offsets are educated guesses based on typical layouts
    XCI_HEADER_OFFSET       = 0x00100000    # 1 MB - XCI Header area
    XCI_HEADER_SIZE         = 0x00000200    # 512 bytes
    
    XCI_CERT_OFFSET         = 0x00100200    # Certificate area
    XCI_CERT_SIZE           = 0x00000200    # 512 bytes
    
    XCI_DATA_OFFSET         = 0x00200000    # 2 MB - XCI Data start
    
    # Control registers (estimated - for command interface)
    CONTROL_OFFSET          = 0x20000000    # 512 MB - Control area
    STATUS_OFFSET           = 0x20000200    # Status register
    COMMAND_OFFSET          = 0x20000400    # Command register
    
    # Cart info area
    CART_INFO_OFFSET        = 0x20001000    # Cartridge info


#=============================================================================
# CONTROL COMMANDS (written to COMMAND_OFFSET)
#=============================================================================

class MIGCommand(IntEnum):
    """Commands written to control area"""
    NOP             = 0x00
    GET_STATUS      = 0x01
    GET_CART_INFO   = 0x02
    AUTH_START      = 0x10
    AUTH_STATUS     = 0x11
    READ_XCI        = 0x20
    READ_CERT       = 0x21
    READ_UID        = 0x22
    ABORT           = 0xFF


class MIGStatus(IntEnum):
    """Status codes read from status area"""
    IDLE            = 0x00
    BUSY            = 0x01
    READY           = 0x02
    AUTH_REQUIRED   = 0x10
    AUTH_IN_PROGRESS = 0x11
    AUTH_SUCCESS    = 0x12
    AUTH_FAILED     = 0x13
    NO_CART         = 0x20
    CART_INSERTED   = 0x21
    ERROR           = 0xFF


#=============================================================================
# DATA STRUCTURES
#=============================================================================

@dataclass
class MIGDeviceInfo:
    """MIG-Flash device information"""
    path: str
    label: str
    is_removable: bool
    firmware_version: str = ""
    raw_device: str = ""


@dataclass
class CartridgeInfo:
    """Switch cartridge information"""
    inserted: bool = False
    authenticated: bool = False
    title_id: str = ""
    title_name: str = ""
    version: str = ""
    size: int = 0
    trimmed_size: int = 0


@dataclass
class XCIHeader:
    """XCI file header structure (0x200 bytes)"""
    signature: bytes            # RSA-2048 signature
    magic: bytes                # "HEAD"
    secure_area_start: int
    backup_area_start: int
    title_key_dec_index: int
    game_card_size: int
    game_card_header_version: int
    game_card_flags: int
    package_id: int
    valid_data_end: int
    iv: bytes
    hfs0_partition_offset: int
    hfs0_header_size: int
    sha256_hash: bytes
    init_data_hash: bytes
    secure_mode_flag: int
    title_key_flag: int
    key_flag: int
    normal_area_end: int
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'XCIHeader':
        """Parse XCI header from raw bytes"""
        if len(data) < 0x200:
            raise ValueError("XCI header too short")
        
        return cls(
            signature=data[0x000:0x100],
            magic=data[0x100:0x104],
            secure_area_start=struct.unpack('<I', data[0x104:0x108])[0],
            backup_area_start=struct.unpack('<I', data[0x108:0x10C])[0],
            title_key_dec_index=data[0x10C],
            game_card_size=data[0x10D],
            game_card_header_version=data[0x10E],
            game_card_flags=data[0x10F],
            package_id=struct.unpack('<Q', data[0x110:0x118])[0],
            valid_data_end=struct.unpack('<Q', data[0x118:0x120])[0],
            iv=data[0x120:0x130],
            hfs0_partition_offset=struct.unpack('<Q', data[0x130:0x138])[0],
            hfs0_header_size=struct.unpack('<Q', data[0x138:0x140])[0],
            sha256_hash=data[0x140:0x160],
            init_data_hash=data[0x160:0x180],
            secure_mode_flag=data[0x180],
            title_key_flag=data[0x181],
            key_flag=data[0x182],
            normal_area_end=struct.unpack('<I', data[0x183:0x187])[0]
        )
    
    @property
    def is_valid(self) -> bool:
        return self.magic == b'HEAD'
    
    @property
    def cart_size_bytes(self) -> int:
        """Get cartridge size in bytes"""
        sizes = {
            0xFA: 1 * 1024 * 1024 * 1024,      # 1 GB
            0xF8: 2 * 1024 * 1024 * 1024,      # 2 GB
            0xF0: 4 * 1024 * 1024 * 1024,      # 4 GB
            0xE0: 8 * 1024 * 1024 * 1024,      # 8 GB
            0xE1: 16 * 1024 * 1024 * 1024,     # 16 GB
            0xE2: 32 * 1024 * 1024 * 1024,     # 32 GB
        }
        return sizes.get(self.game_card_size, 0)


#=============================================================================
# PLATFORM-SPECIFIC BLOCK I/O
#=============================================================================

def is_windows() -> bool:
    return platform.system() == 'Windows'

def is_mac() -> bool:
    return platform.system() == 'Darwin'

def is_linux() -> bool:
    return platform.system() == 'Linux'


class BlockDevice:
    """
    Platform-independent block device access.
    Provides raw sector read/write to USB storage devices.
    """
    
    def __init__(self, device_path: str):
        self.device_path = device_path
        self.handle = None
        self._opened = False
        
        # Platform-specific imports
        if is_windows():
            import win32file
            import win32con
            import winioctlcon
            self._win32file = win32file
            self._win32con = win32con
            self._winioctlcon = winioctlcon
    
    def open(self):
        """Open device for raw access"""
        if self._opened:
            return
        
        if is_windows():
            self._open_windows()
        else:
            self._open_unix()
        
        self._opened = True
        logger.debug(f"Opened device: {self.device_path}")
    
    def _open_windows(self):
        """Open device on Windows"""
        access = self._win32con.GENERIC_READ | self._win32con.GENERIC_WRITE
        share = self._win32con.FILE_SHARE_READ | self._win32con.FILE_SHARE_WRITE
        
        self.handle = self._win32file.CreateFile(
            self.device_path,
            access,
            share,
            None,
            self._win32con.OPEN_EXISTING,
            self._win32con.FILE_ATTRIBUTE_NORMAL,
            None
        )
        
        # Lock volume for exclusive access
        try:
            self._win32file.DeviceIoControl(
                self.handle,
                self._winioctlcon.FSCTL_LOCK_VOLUME,
                None, 0, None
            )
        except:
            pass  # May fail if already locked
    
    def _open_unix(self):
        """Open device on Unix/Mac"""
        if is_mac():
            # Unmount on Mac first
            import subprocess
            try:
                subprocess.run(
                    ["diskutil", "unmountDisk", "force", self.device_path],
                    capture_output=True,
                    check=False
                )
            except:
                pass
        
        self.handle = open(self.device_path, 'r+b', buffering=0)
    
    def close(self):
        """Close device"""
        if not self._opened:
            return
        
        if is_windows():
            try:
                self._win32file.DeviceIoControl(
                    self.handle,
                    self._winioctlcon.FSCTL_UNLOCK_VOLUME,
                    None, 0, None
                )
            except:
                pass
            self._win32file.CloseHandle(self.handle)
        else:
            self.handle.close()
        
        self.handle = None
        self._opened = False
        logger.debug(f"Closed device: {self.device_path}")
    
    def seek(self, offset: int):
        """Seek to absolute offset"""
        if is_windows():
            self._win32file.SetFilePointer(
                self.handle, offset, self._win32con.FILE_BEGIN
            )
        else:
            self.handle.seek(offset)
    
    def read(self, size: int) -> bytes:
        """Read bytes from current position"""
        if is_windows():
            _, data = self._win32file.ReadFile(self.handle, size, None)
            return bytes(data)
        else:
            return self.handle.read(size)
    
    def write(self, data: bytes):
        """Write bytes to current position"""
        if is_windows():
            self._win32file.WriteFile(self.handle, data, None)
        else:
            self.handle.write(data)
    
    def read_sector(self, sector: int) -> bytes:
        """Read a single 512-byte sector"""
        self.seek(sector * SECTOR_SIZE)
        return self.read(SECTOR_SIZE)
    
    def write_sector(self, sector: int, data: bytes):
        """Write a single 512-byte sector"""
        if len(data) != SECTOR_SIZE:
            raise ValueError(f"Sector data must be {SECTOR_SIZE} bytes")
        self.seek(sector * SECTOR_SIZE)
        self.write(data)
    
    def read_at(self, offset: int, size: int) -> bytes:
        """Read bytes from specific offset"""
        self.seek(offset)
        return self.read(size)
    
    def write_at(self, offset: int, data: bytes):
        """Write bytes to specific offset"""
        self.seek(offset)
        self.write(data)
    
    def __enter__(self):
        self.open()
        return self
    
    def __exit__(self, *args):
        self.close()


#=============================================================================
# DEVICE DISCOVERY
#=============================================================================

def find_mig_devices() -> List[MIGDeviceInfo]:
    """
    Find all connected MIG-Flash devices.
    Returns list of device info objects.
    """
    devices = []
    
    if is_windows():
        devices = _find_windows_devices()
    elif is_mac():
        devices = _find_mac_devices()
    elif is_linux():
        devices = _find_linux_devices()
    
    return devices


def _find_windows_devices() -> List[MIGDeviceInfo]:
    """Find MIG devices on Windows"""
    import win32file
    import win32con
    import win32api
    import winioctlcon
    import ctypes
    from ctypes import wintypes
    
    kernel32 = ctypes.WinDLL('kernel32')
    kernel32.FindFirstVolumeW.restype = wintypes.HANDLE
    kernel32.FindNextVolumeW.argtypes = (wintypes.HANDLE, wintypes.LPWSTR, wintypes.DWORD)
    kernel32.FindVolumeClose.argtypes = (wintypes.HANDLE,)
    
    devices = []
    
    # Find all volumes
    volume_name = ctypes.create_unicode_buffer(" " * 255)
    h = kernel32.FindFirstVolumeW(volume_name, 255)
    
    if h == win32file.INVALID_HANDLE_VALUE:
        return devices
    
    while True:
        guid = volume_name.value
        
        try:
            drive_type = win32file.GetDriveType(guid)
            
            if drive_type == win32con.DRIVE_REMOVABLE:
                try:
                    vol_info = win32api.GetVolumeInformation(guid)
                    label = vol_info[0]
                except:
                    label = ""
                
                # Get physical drive path
                try:
                    h_vol = win32file.CreateFile(
                        guid[:-1],
                        win32con.GENERIC_READ,
                        win32con.FILE_SHARE_READ | win32con.FILE_SHARE_WRITE,
                        None,
                        win32con.OPEN_EXISTING,
                        win32con.FILE_ATTRIBUTE_NORMAL,
                        None
                    )
                    
                    r = win32file.DeviceIoControl(
                        h_vol,
                        winioctlcon.IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                        None, 512, None
                    )
                    
                    if len(r) >= 12:
                        disk_num = struct.unpack('L', r[8:12])[0]
                        phys_path = fr"\\.\PhysicalDrive{disk_num}"
                        
                        devices.append(MIGDeviceInfo(
                            path=phys_path,
                            label=label,
                            is_removable=True
                        ))
                    
                    win32file.CloseHandle(h_vol)
                except:
                    pass
        except:
            pass
        
        # Next volume
        if kernel32.FindNextVolumeW(h, volume_name, 255) == 0:
            break
    
    kernel32.FindVolumeClose(h)
    return devices


def _find_mac_devices() -> List[MIGDeviceInfo]:
    """Find MIG devices on macOS"""
    import subprocess
    import plistlib
    
    devices = []
    
    try:
        result = subprocess.run(
            ["diskutil", "list", "-plist", "external"],
            capture_output=True
        )
        
        if result.returncode == 0:
            plist = plistlib.loads(result.stdout)
            
            for disk in plist.get('AllDisksAndPartitions', []):
                disk_id = disk.get('DeviceIdentifier', '')
                partitions = disk.get('Partitions', [])
                
                if partitions:
                    labels = [p.get('VolumeName', '') for p in partitions]
                    label = ' '.join(filter(None, labels))
                    
                    devices.append(MIGDeviceInfo(
                        path=f'/dev/{disk_id}',
                        label=label,
                        is_removable=True,
                        raw_device=f'/dev/r{disk_id}'  # Raw device for macOS
                    ))
    except Exception as e:
        logger.error(f"Error finding Mac devices: {e}")
    
    return devices


def _find_linux_devices() -> List[MIGDeviceInfo]:
    """Find MIG devices on Linux"""
    import subprocess
    
    devices = []
    
    try:
        result = subprocess.run(
            ["lsblk", "-o", "path,type,fstype,tran,label", "-J"],
            capture_output=True,
            text=True
        )
        
        if result.returncode == 0:
            data = json.loads(result.stdout)
            
            for block in data.get('blockdevices', []):
                if block.get('type') == 'disk' and block.get('tran') == 'usb':
                    path = block.get('path', '')
                    children = block.get('children', [])
                    
                    labels = [c.get('label', '') for c in children]
                    label = ' '.join(filter(None, labels))
                    
                    devices.append(MIGDeviceInfo(
                        path=path,
                        label=label,
                        is_removable=True
                    ))
    except Exception as e:
        logger.error(f"Error finding Linux devices: {e}")
    
    return devices


#=============================================================================
# MIG-FLASH MAIN CLASS
#=============================================================================

class MIGFlash:
    """
    MIG-Flash Cartridge Dumper Interface
    
    This class provides high-level access to MIG-Flash hardware
    for reading Switch game cartridges.
    
    Example:
        mig = MIGFlash()
        mig.connect()
        
        print(f"Firmware: {mig.firmware_version}")
        
        if mig.cart_inserted:
            mig.authenticate()
            mig.dump_xci("game.xci")
        
        mig.disconnect()
    """
    
    def __init__(self):
        self.device: Optional[BlockDevice] = None
        self.device_info: Optional[MIGDeviceInfo] = None
        self._connected = False
        
        # Cached info
        self._firmware_version: Optional[str] = None
        self._cart_info: Optional[CartridgeInfo] = None
        self._xci_header: Optional[XCIHeader] = None
    
    #-------------------------------------------------------------------------
    # CONNECTION
    #-------------------------------------------------------------------------
    
    @staticmethod
    def find_devices() -> List[MIGDeviceInfo]:
        """Find all connected MIG-Flash devices"""
        return find_mig_devices()
    
    def connect(self, device_path: Optional[str] = None) -> bool:
        """
        Connect to MIG-Flash device.
        
        Args:
            device_path: Optional specific device path.
                        If None, auto-detects first device.
        
        Returns:
            True if connected successfully.
        """
        if self._connected:
            self.disconnect()
        
        # Find device
        if device_path:
            self.device_info = MIGDeviceInfo(
                path=device_path,
                label="",
                is_removable=True
            )
        else:
            devices = self.find_devices()
            if not devices:
                raise ConnectionError("No MIG-Flash device found")
            self.device_info = devices[0]
        
        # Open device
        try:
            self.device = BlockDevice(self.device_info.path)
            self.device.open()
            
            # Verify it's a MIG device by checking GPT
            if not self._verify_device():
                self.device.close()
                raise ConnectionError("Device is not a valid MIG-Flash")
            
            self._connected = True
            logger.info(f"Connected to MIG-Flash: {self.device_info.path}")
            
            # Read firmware version
            self._read_firmware_version()
            
            return True
            
        except Exception as e:
            if self.device:
                try:
                    self.device.close()
                except:
                    pass
            raise ConnectionError(f"Failed to connect: {e}")
    
    def disconnect(self):
        """Disconnect from MIG-Flash device"""
        if self.device:
            try:
                self.device.close()
            except:
                pass
        
        self.device = None
        self._connected = False
        self._firmware_version = None
        self._cart_info = None
        self._xci_header = None
        
        logger.info("Disconnected from MIG-Flash")
    
    @property
    def is_connected(self) -> bool:
        """Check if connected"""
        return self._connected
    
    def _verify_device(self) -> bool:
        """Verify device is a MIG-Flash by checking GPT"""
        try:
            data = self.device.read_at(MemoryMap.GPT_PARTITION_OFFSET, 512)
            return data[0:16] == GPT_MSDATA_GUID
        except:
            return False
    
    #-------------------------------------------------------------------------
    # FIRMWARE
    #-------------------------------------------------------------------------
    
    def _read_firmware_version(self):
        """Read firmware version from device"""
        try:
            data = self.device.read_at(
                MemoryMap.FIRMWARE_OFFSET,
                MemoryMap.FIRMWARE_VERSION_LEN
            )
            
            # Version is null-terminated ASCII string
            self._firmware_version = data.split(b'\x00')[0].decode('ascii')
            logger.info(f"Firmware version: {self._firmware_version}")
            
        except Exception as e:
            logger.error(f"Failed to read firmware version: {e}")
            self._firmware_version = "Unknown"
    
    @property
    def firmware_version(self) -> str:
        """Get firmware version string"""
        return self._firmware_version or "Unknown"
    
    #-------------------------------------------------------------------------
    # CARTRIDGE STATUS
    #-------------------------------------------------------------------------
    
    @property
    def cart_inserted(self) -> bool:
        """Check if cartridge is inserted"""
        if not self._connected:
            return False
        
        try:
            # Try to read XCI header area
            data = self.device.read_at(MemoryMap.XCI_HEADER_OFFSET, 4)
            # Check for "HEAD" magic
            return data == b'HEAD'
        except:
            return False
    
    @property
    def cart_authenticated(self) -> bool:
        """Check if cartridge is authenticated"""
        return self._cart_info is not None and self._cart_info.authenticated
    
    #-------------------------------------------------------------------------
    # AUTHENTICATION
    #-------------------------------------------------------------------------
    
    def authenticate(self, timeout: float = 30.0) -> bool:
        """
        Authenticate the inserted cartridge.
        
        This is typically handled automatically by the MIG-Flash
        hardware, but this method allows checking/waiting for
        authentication completion.
        
        Returns:
            True if authentication successful.
        """
        if not self._connected:
            raise ConnectionError("Not connected")
        
        if not self.cart_inserted:
            raise RuntimeError("No cartridge inserted")
        
        # Read XCI header to verify authentication
        try:
            header_data = self.device.read_at(
                MemoryMap.XCI_HEADER_OFFSET,
                MemoryMap.XCI_HEADER_SIZE
            )
            
            self._xci_header = XCIHeader.from_bytes(header_data)
            
            if self._xci_header.is_valid:
                self._cart_info = CartridgeInfo(
                    inserted=True,
                    authenticated=True,
                    size=self._xci_header.cart_size_bytes,
                    trimmed_size=self._xci_header.valid_data_end * 0x200
                )
                logger.info("Cartridge authenticated successfully")
                return True
            
        except Exception as e:
            logger.error(f"Authentication error: {e}")
        
        return False
    
    #-------------------------------------------------------------------------
    # XCI READING
    #-------------------------------------------------------------------------
    
    def get_xci_header(self) -> Optional[XCIHeader]:
        """Get XCI header (requires authentication)"""
        if not self.cart_authenticated:
            if not self.authenticate():
                raise RuntimeError("Authentication required")
        
        return self._xci_header
    
    def get_xci_size(self) -> Tuple[int, int]:
        """
        Get XCI size.
        
        Returns:
            Tuple of (total_size, trimmed_size) in bytes.
        """
        header = self.get_xci_header()
        if header:
            return (header.cart_size_bytes, header.valid_data_end * 0x200)
        return (0, 0)
    
    def read_xci(self, offset: int, length: int) -> bytes:
        """
        Read XCI data.
        
        Args:
            offset: Offset from start of XCI data.
            length: Number of bytes to read.
        
        Returns:
            Read data.
        """
        if not self.cart_authenticated:
            raise RuntimeError("Authentication required")
        
        return self.device.read_at(
            MemoryMap.XCI_DATA_OFFSET + offset,
            length
        )
    
    def dump_xci(self, filename: str,
                 trimmed: bool = True,
                 progress_callback: Optional[Callable[[int, int], None]] = None) -> bool:
        """
        Dump entire XCI to file.
        
        Args:
            filename: Output filename.
            trimmed: If True, only dump used data (smaller file).
            progress_callback: Called with (bytes_done, total_bytes).
        
        Returns:
            True if successful.
        """
        if not self.cart_authenticated:
            if not self.authenticate():
                raise RuntimeError("Authentication required")
        
        total_size, trimmed_size = self.get_xci_size()
        dump_size = trimmed_size if trimmed else total_size
        
        logger.info(f"Dumping XCI: {dump_size / (1024*1024*1024):.2f} GB")
        
        chunk_size = 1024 * 1024  # 1 MB chunks
        offset = 0
        
        with open(filename, 'wb') as f:
            while offset < dump_size:
                # Read chunk
                remaining = dump_size - offset
                read_size = min(chunk_size, remaining)
                
                data = self.read_xci(offset, read_size)
                f.write(data)
                
                offset += len(data)
                
                # Progress callback
                if progress_callback:
                    progress_callback(offset, dump_size)
        
        logger.info(f"Dump complete: {filename}")
        return True
    
    #-------------------------------------------------------------------------
    # CERTIFICATE/UID
    #-------------------------------------------------------------------------
    
    def read_uid(self) -> bytes:
        """Read cartridge UID"""
        if not self._connected:
            raise ConnectionError("Not connected")
        
        # UID is typically in the XCI header area
        return self.device.read_at(
            MemoryMap.XCI_HEADER_OFFSET + 0x100,
            16
        )
    
    def read_certificate(self) -> bytes:
        """Read cartridge certificate"""
        if not self.cart_authenticated:
            raise RuntimeError("Authentication required")
        
        return self.device.read_at(
            MemoryMap.XCI_CERT_OFFSET,
            MemoryMap.XCI_CERT_SIZE
        )
    
    #-------------------------------------------------------------------------
    # CONTEXT MANAGER
    #-------------------------------------------------------------------------
    
    def __enter__(self):
        return self
    
    def __exit__(self, *args):
        self.disconnect()


#=============================================================================
# CONVENIENCE FUNCTION
#=============================================================================

def connect_mig(device_path: Optional[str] = None) -> MIGFlash:
    """
    Connect to MIG-Flash device (context manager).
    
    Usage:
        with connect_mig() as mig:
            print(mig.firmware_version)
    """
    mig = MIGFlash()
    mig.connect(device_path)
    return mig


#=============================================================================
# CLI
#=============================================================================

def main():
    """Command-line interface"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='MIG-Flash Cartridge Dumper'
    )
    
    subparsers = parser.add_subparsers(dest='command')
    
    # List devices
    subparsers.add_parser('list', help='List connected devices')
    
    # Check firmware
    check_parser = subparsers.add_parser('check', help='Check firmware version')
    check_parser.add_argument('--device', '-d', help='Device path')
    
    # Dump XCI
    dump_parser = subparsers.add_parser('dump', help='Dump XCI')
    dump_parser.add_argument('output', help='Output filename')
    dump_parser.add_argument('--device', '-d', help='Device path')
    dump_parser.add_argument('--full', action='store_true', help='Dump full size (not trimmed)')
    
    args = parser.parse_args()
    
    if args.command == 'list':
        print("Looking for MIG-Flash devices...")
        devices = find_mig_devices()
        
        if not devices:
            print("No devices found!")
        else:
            for i, dev in enumerate(devices):
                print(f"  [{i}] {dev.path} - {dev.label}")
    
    elif args.command == 'check':
        try:
            with connect_mig(args.device) as mig:
                print(f"Firmware version: {mig.firmware_version}")
                print(f"Cart inserted: {mig.cart_inserted}")
        except Exception as e:
            print(f"Error: {e}")
    
    elif args.command == 'dump':
        try:
            with connect_mig(args.device) as mig:
                print(f"Firmware: {mig.firmware_version}")
                
                if not mig.cart_inserted:
                    print("No cartridge inserted!")
                    return
                
                print("Authenticating...")
                if not mig.authenticate():
                    print("Authentication failed!")
                    return
                
                def on_progress(done, total):
                    pct = done / total * 100
                    print(f"\rProgress: {pct:.1f}% ({done}/{total})", end='')
                
                print(f"Dumping to {args.output}...")
                mig.dump_xci(
                    args.output,
                    trimmed=not args.full,
                    progress_callback=on_progress
                )
                print("\nDone!")
                
        except Exception as e:
            print(f"Error: {e}")
    
    else:
        parser.print_help()


if __name__ == "__main__":
    # Check for admin privileges
    if is_windows():
        import ctypes
        if not ctypes.windll.shell32.IsUserAnAdmin():
            print("Please run as Administrator!")
            sys.exit(1)
    elif os.geteuid() != 0:
        print("Please run with sudo!")
        sys.exit(1)
    
    main()
