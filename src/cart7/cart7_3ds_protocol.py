"""
Nintendo 3DS Cartridge Protocol for Cart8 (8-in-1 Reader)
=========================================================

Extends Cart7 with Nintendo 3DS support.

Usage:
    from cart7_3ds_protocol import Ctr3DSCartridge
    
    ctr = Ctr3DSCartridge(device)
    ctr.select()
    info = ctr.get_info()
    ctr.dump("game.3ds")
"""

import struct
from dataclasses import dataclass
from typing import Optional, List, Callable
from enum import IntEnum

# Constants
SLOT_3DS = 0x09
CTR_MEDIA_UNIT_SIZE = 0x200
CTR_NCSD_HEADER_SIZE = 0x200
CTR_NCCH_HEADER_SIZE = 0x200
CTR_SMDH_SIZE = 0x36C0

class Ctr3DSCommand(IntEnum):
    GET_HEADER = 0x70
    GET_NCCH = 0x71
    READ_ROM = 0x72
    READ_SAVE = 0x79
    WRITE_SAVE = 0x7A
    GET_SMDH = 0x7C

class CtrSaveType(IntEnum):
    NONE = 0
    EEPROM_4K = 1
    EEPROM_64K = 2
    EEPROM_512K = 3
    FLASH_512K = 5
    FLASH_1M = 6
    FLASH_2M = 7
    FLASH_4M = 8
    FLASH_8M = 9

@dataclass
class CtrPartitionInfo:
    index: int
    offset: int
    size: int
    fs_type: int
    encrypted: bool

@dataclass
class CtrNCSDHeader:
    magic: bytes
    size: int
    media_id: bytes
    partitions: List[CtrPartitionInfo]
    
    @property
    def size_bytes(self) -> int:
        return self.size * CTR_MEDIA_UNIT_SIZE
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'CtrNCSDHeader':
        magic = data[0x100:0x104]
        size = struct.unpack('<I', data[0x104:0x108])[0]
        media_id = data[0x108:0x110]
        
        partitions = []
        for i in range(8):
            offset = 0x120 + i * 8
            part_offset = struct.unpack('<I', data[offset:offset+4])[0]
            part_size = struct.unpack('<I', data[offset+4:offset+8])[0]
            if part_size > 0:
                partitions.append(CtrPartitionInfo(
                    index=i,
                    offset=part_offset * CTR_MEDIA_UNIT_SIZE,
                    size=part_size * CTR_MEDIA_UNIT_SIZE,
                    fs_type=data[0x110 + i],
                    encrypted=(data[0x118 + i] != 0)
                ))
        
        return cls(magic=magic, size=size, media_id=media_id, partitions=partitions)

@dataclass
class CtrNCCHHeader:
    magic: bytes
    content_size: int
    product_code: str
    maker_code: bytes
    exefs_offset: int
    exefs_size: int
    romfs_offset: int
    romfs_size: int
    flags: bytes
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'CtrNCCHHeader':
        return cls(
            magic=data[0x100:0x104],
            content_size=struct.unpack('<I', data[0x104:0x108])[0],
            product_code=data[0x150:0x160].decode('ascii').rstrip('\x00'),
            maker_code=data[0x110:0x112],
            exefs_offset=struct.unpack('<I', data[0x1A0:0x1A4])[0],
            exefs_size=struct.unpack('<I', data[0x1A4:0x1A8])[0],
            romfs_offset=struct.unpack('<I', data[0x1B0:0x1B4])[0],
            romfs_size=struct.unpack('<I', data[0x1B4:0x1B8])[0],
            flags=data[0x188:0x190]
        )

@dataclass
class Ctr3DSInfo:
    product_code: str
    card_size: int
    title_short: str
    title_long: str
    publisher: str
    partitions: List[CtrPartitionInfo]
    is_new3ds_exclusive: bool

class Ctr3DSCartridge:
    """High-level 3DS cartridge interface"""
    
    def __init__(self, device):
        self.device = device
        self._ncsd: Optional[CtrNCSDHeader] = None
    
    def select(self):
        """Select 3DS slot (1.8V)"""
        self.device.select_slot(SLOT_3DS, voltage=18)
    
    def read_ncsd(self) -> CtrNCSDHeader:
        data = self.device.read_rom(0, CTR_NCSD_HEADER_SIZE)
        self._ncsd = CtrNCSDHeader.from_bytes(data)
        return self._ncsd
    
    def read_ncch(self, partition: int = 0) -> CtrNCCHHeader:
        if self._ncsd is None:
            self.read_ncsd()
        for part in self._ncsd.partitions:
            if part.index == partition:
                data = self.device.read_rom(part.offset, CTR_NCCH_HEADER_SIZE)
                return CtrNCCHHeader.from_bytes(data)
        raise ValueError(f"Partition {partition} not found")
    
    def get_info(self) -> Ctr3DSInfo:
        ncsd = self.read_ncsd()
        ncch = self.read_ncch(0)
        return Ctr3DSInfo(
            product_code=ncch.product_code,
            card_size=ncsd.size_bytes,
            title_short="",
            title_long="",
            publisher="",
            partitions=ncsd.partitions,
            is_new3ds_exclusive=False
        )
    
    def dump(self, filename: str, trimmed: bool = True,
             progress_callback: Optional[Callable[[int, int], bool]] = None):
        ncsd = self.read_ncsd()
        
        if trimmed:
            dump_size = max((p.offset + p.size) for p in ncsd.partitions) if ncsd.partitions else 0
        else:
            dump_size = ncsd.size_bytes
        
        chunk_size = 1024 * 1024
        with open(filename, 'wb') as f:
            offset = 0
            while offset < dump_size:
                read_size = min(chunk_size, dump_size - offset)
                data = self.device.read_rom(offset, read_size)
                f.write(data)
                offset += len(data)
                if progress_callback and not progress_callback(offset, dump_size):
                    raise InterruptedError("Dump aborted")

def format_size(size_bytes: int) -> str:
    if size_bytes >= 1024**3:
        return f"{size_bytes / 1024**3:.2f} GB"
    elif size_bytes >= 1024**2:
        return f"{size_bytes / 1024**2:.2f} MB"
    return f"{size_bytes / 1024:.2f} KB"

if __name__ == "__main__":
    print("3DS Cartridge Protocol Library for Cart8")
    print("Version 1.0.0")
