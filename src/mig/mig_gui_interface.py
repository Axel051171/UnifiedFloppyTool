"""
MIG-Flash GUI Interface
=======================

High-level interface designed for GUI integration.
Provides async-friendly operations with callbacks for progress updates.

This module wraps mig_flash.py with GUI-specific functionality:
- Thread-safe operations
- Progress callbacks
- Event notifications
- State management

Author: UFI Project
Version: 2.0.0
Date: 2026-01-20
"""

import threading
import queue
import time
from dataclasses import dataclass, field
from typing import Optional, Callable, List, Any
from enum import Enum, auto

from mig_flash import (
    MIGFlash, MIGDeviceInfo, CartridgeInfo, XCIHeader,
    find_mig_devices, MemoryMap, SECTOR_SIZE
)

#=============================================================================
# EVENT TYPES
#=============================================================================

class MIGEvent(Enum):
    """Events sent to GUI"""
    DEVICE_CONNECTED = auto()
    DEVICE_DISCONNECTED = auto()
    CART_INSERTED = auto()
    CART_REMOVED = auto()
    AUTH_STARTED = auto()
    AUTH_SUCCESS = auto()
    AUTH_FAILED = auto()
    DUMP_STARTED = auto()
    DUMP_PROGRESS = auto()
    DUMP_COMPLETE = auto()
    DUMP_ERROR = auto()
    ERROR = auto()


class MIGState(Enum):
    """Device state machine"""
    DISCONNECTED = auto()
    CONNECTING = auto()
    CONNECTED = auto()
    NO_CART = auto()
    CART_DETECTED = auto()
    AUTHENTICATING = auto()
    AUTHENTICATED = auto()
    DUMPING = auto()
    ERROR = auto()


#=============================================================================
# EVENT DATA
#=============================================================================

@dataclass
class MIGEventData:
    """Event data passed to callbacks"""
    event: MIGEvent
    state: MIGState
    message: str = ""
    progress: float = 0.0          # 0-100%
    bytes_done: int = 0
    bytes_total: int = 0
    speed_kbps: int = 0
    eta_seconds: int = 0
    error: Optional[Exception] = None
    data: Any = None               # Event-specific data


#=============================================================================
# GUI CONTROLLER
#=============================================================================

class MIGController:
    """
    MIG-Flash GUI Controller
    
    Thread-safe controller for GUI applications.
    All operations run in background threads with callbacks.
    
    Usage:
        controller = MIGController()
        controller.set_event_callback(on_event)
        
        # Connect (async)
        controller.connect_async()
        
        # In callback, when CART_DETECTED:
        controller.authenticate_async()
        
        # In callback, when AUTHENTICATED:
        controller.dump_xci_async("game.xci")
    """
    
    def __init__(self):
        self._mig: Optional[MIGFlash] = None
        self._state = MIGState.DISCONNECTED
        self._event_callback: Optional[Callable[[MIGEventData], None]] = None
        
        # Threading
        self._worker_thread: Optional[threading.Thread] = None
        self._abort_flag = threading.Event()
        self._lock = threading.Lock()
        
        # Device monitoring
        self._monitor_thread: Optional[threading.Thread] = None
        self._monitoring = False
        
        # Cached info
        self._device_info: Optional[MIGDeviceInfo] = None
        self._cart_info: Optional[CartridgeInfo] = None
        self._xci_header: Optional[XCIHeader] = None
    
    #-------------------------------------------------------------------------
    # PROPERTIES
    #-------------------------------------------------------------------------
    
    @property
    def state(self) -> MIGState:
        """Current device state"""
        return self._state
    
    @property
    def is_connected(self) -> bool:
        """Check if connected to device"""
        return self._mig is not None and self._mig.is_connected
    
    @property
    def device_info(self) -> Optional[MIGDeviceInfo]:
        """Connected device info"""
        return self._device_info
    
    @property
    def firmware_version(self) -> str:
        """Firmware version string"""
        if self._mig:
            return self._mig.firmware_version
        return ""
    
    @property
    def cart_info(self) -> Optional[CartridgeInfo]:
        """Cartridge information"""
        return self._cart_info
    
    @property
    def xci_header(self) -> Optional[XCIHeader]:
        """XCI header data"""
        return self._xci_header
    
    #-------------------------------------------------------------------------
    # CALLBACKS
    #-------------------------------------------------------------------------
    
    def set_event_callback(self, callback: Callable[[MIGEventData], None]):
        """
        Set callback for events.
        
        The callback is called from worker threads, so GUI frameworks
        may need to use invoke/dispatch to update UI.
        """
        self._event_callback = callback
    
    def _emit_event(self, event: MIGEvent, message: str = "", **kwargs):
        """Send event to callback"""
        if self._event_callback:
            data = MIGEventData(
                event=event,
                state=self._state,
                message=message,
                **kwargs
            )
            self._event_callback(data)
    
    def _set_state(self, state: MIGState):
        """Update state"""
        self._state = state
    
    #-------------------------------------------------------------------------
    # DEVICE DISCOVERY
    #-------------------------------------------------------------------------
    
    def find_devices(self) -> List[MIGDeviceInfo]:
        """Find all connected MIG-Flash devices (synchronous)"""
        return find_mig_devices()
    
    def find_devices_async(self):
        """Find devices asynchronously"""
        def worker():
            try:
                devices = find_mig_devices()
                self._emit_event(
                    MIGEvent.DEVICE_CONNECTED,
                    f"Found {len(devices)} device(s)",
                    data=devices
                )
            except Exception as e:
                self._emit_event(MIGEvent.ERROR, str(e), error=e)
        
        threading.Thread(target=worker, daemon=True).start()
    
    #-------------------------------------------------------------------------
    # CONNECTION
    #-------------------------------------------------------------------------
    
    def connect(self, device_path: Optional[str] = None) -> bool:
        """Connect to device (synchronous)"""
        with self._lock:
            if self._mig:
                self._mig.disconnect()
            
            self._mig = MIGFlash()
            
            try:
                self._mig.connect(device_path)
                self._device_info = self._mig.device_info
                self._set_state(MIGState.CONNECTED)
                
                # Check cart status
                if self._mig.cart_inserted:
                    self._set_state(MIGState.CART_DETECTED)
                else:
                    self._set_state(MIGState.NO_CART)
                
                return True
                
            except Exception as e:
                self._set_state(MIGState.ERROR)
                raise
    
    def connect_async(self, device_path: Optional[str] = None):
        """Connect to device asynchronously"""
        def worker():
            self._set_state(MIGState.CONNECTING)
            self._emit_event(MIGEvent.DEVICE_CONNECTED, "Connecting...")
            
            try:
                self.connect(device_path)
                self._emit_event(
                    MIGEvent.DEVICE_CONNECTED,
                    f"Connected: {self.firmware_version}",
                    data=self._device_info
                )
                
                # Notify cart status
                if self._state == MIGState.CART_DETECTED:
                    self._emit_event(MIGEvent.CART_INSERTED, "Cartridge detected")
                else:
                    self._emit_event(MIGEvent.CART_REMOVED, "No cartridge")
                    
            except Exception as e:
                self._set_state(MIGState.ERROR)
                self._emit_event(MIGEvent.ERROR, str(e), error=e)
        
        self._worker_thread = threading.Thread(target=worker, daemon=True)
        self._worker_thread.start()
    
    def disconnect(self):
        """Disconnect from device"""
        with self._lock:
            if self._mig:
                self._mig.disconnect()
                self._mig = None
            
            self._device_info = None
            self._cart_info = None
            self._xci_header = None
            self._set_state(MIGState.DISCONNECTED)
        
        self._emit_event(MIGEvent.DEVICE_DISCONNECTED, "Disconnected")
    
    #-------------------------------------------------------------------------
    # AUTHENTICATION
    #-------------------------------------------------------------------------
    
    def authenticate(self) -> bool:
        """Authenticate cartridge (synchronous)"""
        if not self._mig:
            raise RuntimeError("Not connected")
        
        with self._lock:
            success = self._mig.authenticate()
            
            if success:
                self._xci_header = self._mig._xci_header
                self._cart_info = self._mig._cart_info
                self._set_state(MIGState.AUTHENTICATED)
            else:
                self._set_state(MIGState.ERROR)
            
            return success
    
    def authenticate_async(self):
        """Authenticate cartridge asynchronously"""
        def worker():
            self._set_state(MIGState.AUTHENTICATING)
            self._emit_event(MIGEvent.AUTH_STARTED, "Authenticating...")
            
            try:
                if self.authenticate():
                    self._emit_event(
                        MIGEvent.AUTH_SUCCESS,
                        "Authentication successful",
                        data=self._cart_info
                    )
                else:
                    self._emit_event(MIGEvent.AUTH_FAILED, "Authentication failed")
                    
            except Exception as e:
                self._set_state(MIGState.ERROR)
                self._emit_event(MIGEvent.AUTH_FAILED, str(e), error=e)
        
        self._worker_thread = threading.Thread(target=worker, daemon=True)
        self._worker_thread.start()
    
    #-------------------------------------------------------------------------
    # XCI DUMPING
    #-------------------------------------------------------------------------
    
    def dump_xci(self, filename: str, trimmed: bool = True,
                 progress_callback: Optional[Callable[[int, int], None]] = None) -> bool:
        """Dump XCI to file (synchronous)"""
        if not self._mig:
            raise RuntimeError("Not connected")
        
        return self._mig.dump_xci(filename, trimmed, progress_callback)
    
    def dump_xci_async(self, filename: str, trimmed: bool = True):
        """Dump XCI to file asynchronously"""
        def worker():
            self._abort_flag.clear()
            self._set_state(MIGState.DUMPING)
            
            total_size, trimmed_size = self._mig.get_xci_size()
            dump_size = trimmed_size if trimmed else total_size
            
            self._emit_event(
                MIGEvent.DUMP_STARTED,
                f"Dumping {dump_size / (1024*1024*1024):.2f} GB",
                bytes_total=dump_size
            )
            
            start_time = time.time()
            last_update = start_time
            
            def on_progress(done: int, total: int):
                nonlocal last_update
                
                # Check abort
                if self._abort_flag.is_set():
                    raise InterruptedError("Dump aborted")
                
                # Throttle updates
                now = time.time()
                if now - last_update < 0.1:
                    return
                last_update = now
                
                # Calculate speed and ETA
                elapsed = now - start_time
                speed = done / elapsed if elapsed > 0 else 0
                remaining = total - done
                eta = int(remaining / speed) if speed > 0 else 0
                
                self._emit_event(
                    MIGEvent.DUMP_PROGRESS,
                    f"{done / total * 100:.1f}%",
                    progress=done / total * 100,
                    bytes_done=done,
                    bytes_total=total,
                    speed_kbps=int(speed / 1024),
                    eta_seconds=eta
                )
            
            try:
                success = self.dump_xci(filename, trimmed, on_progress)
                
                if success:
                    self._set_state(MIGState.AUTHENTICATED)
                    self._emit_event(
                        MIGEvent.DUMP_COMPLETE,
                        f"Dump complete: {filename}",
                        bytes_done=dump_size,
                        bytes_total=dump_size,
                        progress=100.0
                    )
                else:
                    self._emit_event(MIGEvent.DUMP_ERROR, "Dump failed")
                    
            except InterruptedError:
                self._set_state(MIGState.AUTHENTICATED)
                self._emit_event(MIGEvent.DUMP_ERROR, "Dump aborted by user")
                
            except Exception as e:
                self._set_state(MIGState.ERROR)
                self._emit_event(MIGEvent.DUMP_ERROR, str(e), error=e)
        
        self._worker_thread = threading.Thread(target=worker, daemon=True)
        self._worker_thread.start()
    
    def abort_dump(self):
        """Abort ongoing dump operation"""
        self._abort_flag.set()
    
    #-------------------------------------------------------------------------
    # DEVICE MONITORING
    #-------------------------------------------------------------------------
    
    def start_monitoring(self, interval: float = 1.0):
        """
        Start monitoring for device/cartridge changes.
        
        Periodically checks for:
        - Device connection/disconnection
        - Cartridge insertion/removal
        """
        if self._monitoring:
            return
        
        self._monitoring = True
        
        def monitor():
            was_connected = self.is_connected
            had_cart = False
            
            if was_connected and self._mig:
                had_cart = self._mig.cart_inserted
            
            while self._monitoring:
                time.sleep(interval)
                
                try:
                    # Check connection
                    is_connected = self.is_connected
                    
                    if was_connected and not is_connected:
                        # Lost connection
                        self._set_state(MIGState.DISCONNECTED)
                        self._emit_event(MIGEvent.DEVICE_DISCONNECTED, "Device disconnected")
                        was_connected = False
                        had_cart = False
                        continue
                    
                    if not is_connected:
                        continue
                    
                    # Check cartridge
                    has_cart = self._mig.cart_inserted
                    
                    if not had_cart and has_cart:
                        # Cart inserted
                        self._set_state(MIGState.CART_DETECTED)
                        self._emit_event(MIGEvent.CART_INSERTED, "Cartridge inserted")
                    
                    elif had_cart and not has_cart:
                        # Cart removed
                        self._set_state(MIGState.NO_CART)
                        self._cart_info = None
                        self._xci_header = None
                        self._emit_event(MIGEvent.CART_REMOVED, "Cartridge removed")
                    
                    was_connected = is_connected
                    had_cart = has_cart
                    
                except Exception:
                    pass  # Ignore monitoring errors
        
        self._monitor_thread = threading.Thread(target=monitor, daemon=True)
        self._monitor_thread.start()
    
    def stop_monitoring(self):
        """Stop device monitoring"""
        self._monitoring = False
        if self._monitor_thread:
            self._monitor_thread.join(timeout=2.0)
            self._monitor_thread = None
    
    #-------------------------------------------------------------------------
    # CLEANUP
    #-------------------------------------------------------------------------
    
    def close(self):
        """Clean up resources"""
        self.stop_monitoring()
        self.disconnect()


#=============================================================================
# EXAMPLE GUI INTEGRATION (Tkinter)
#=============================================================================

def example_tkinter_gui():
    """Example Tkinter GUI using MIGController"""
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    
    class MIGDumperGUI:
        def __init__(self, root):
            self.root = root
            self.root.title("MIG-Flash Dumper")
            
            # Controller
            self.controller = MIGController()
            self.controller.set_event_callback(self.on_event)
            
            # GUI Elements
            self.create_widgets()
            
            # Start monitoring
            self.controller.start_monitoring()
        
        def create_widgets(self):
            # Device frame
            frame = ttk.LabelFrame(self.root, text="Device")
            frame.pack(fill='x', padx=10, pady=5)
            
            self.status_label = ttk.Label(frame, text="Disconnected")
            self.status_label.pack(side='left', padx=5)
            
            self.connect_btn = ttk.Button(frame, text="Connect", command=self.on_connect)
            self.connect_btn.pack(side='right', padx=5)
            
            # Cart frame
            frame = ttk.LabelFrame(self.root, text="Cartridge")
            frame.pack(fill='x', padx=10, pady=5)
            
            self.cart_label = ttk.Label(frame, text="No cartridge")
            self.cart_label.pack(side='left', padx=5)
            
            self.auth_btn = ttk.Button(frame, text="Authenticate", command=self.on_auth)
            self.auth_btn.pack(side='right', padx=5)
            self.auth_btn['state'] = 'disabled'
            
            # Dump frame
            frame = ttk.LabelFrame(self.root, text="Dump")
            frame.pack(fill='x', padx=10, pady=5)
            
            self.progress = ttk.Progressbar(frame, mode='determinate')
            self.progress.pack(fill='x', padx=5, pady=5)
            
            self.progress_label = ttk.Label(frame, text="")
            self.progress_label.pack(side='left', padx=5)
            
            self.dump_btn = ttk.Button(frame, text="Dump XCI", command=self.on_dump)
            self.dump_btn.pack(side='right', padx=5)
            self.dump_btn['state'] = 'disabled'
        
        def on_event(self, event: MIGEventData):
            """Handle events from controller (called from worker thread)"""
            # Schedule GUI update on main thread
            self.root.after(0, lambda: self.handle_event(event))
        
        def handle_event(self, event: MIGEventData):
            """Process event on main thread"""
            
            if event.event == MIGEvent.DEVICE_CONNECTED:
                self.status_label['text'] = f"Connected: {self.controller.firmware_version}"
                self.connect_btn['text'] = "Disconnect"
                
            elif event.event == MIGEvent.DEVICE_DISCONNECTED:
                self.status_label['text'] = "Disconnected"
                self.connect_btn['text'] = "Connect"
                self.auth_btn['state'] = 'disabled'
                self.dump_btn['state'] = 'disabled'
                
            elif event.event == MIGEvent.CART_INSERTED:
                self.cart_label['text'] = "Cartridge detected"
                self.auth_btn['state'] = 'normal'
                
            elif event.event == MIGEvent.CART_REMOVED:
                self.cart_label['text'] = "No cartridge"
                self.auth_btn['state'] = 'disabled'
                self.dump_btn['state'] = 'disabled'
                
            elif event.event == MIGEvent.AUTH_SUCCESS:
                self.cart_label['text'] = f"Authenticated - {event.data.size / (1024**3):.1f} GB"
                self.dump_btn['state'] = 'normal'
                
            elif event.event == MIGEvent.AUTH_FAILED:
                messagebox.showerror("Error", f"Authentication failed: {event.message}")
                
            elif event.event == MIGEvent.DUMP_PROGRESS:
                self.progress['value'] = event.progress
                self.progress_label['text'] = f"{event.progress:.1f}% - {event.speed_kbps} KB/s"
                
            elif event.event == MIGEvent.DUMP_COMPLETE:
                self.progress['value'] = 100
                self.progress_label['text'] = "Complete!"
                messagebox.showinfo("Success", event.message)
                
            elif event.event == MIGEvent.DUMP_ERROR:
                messagebox.showerror("Error", event.message)
                
            elif event.event == MIGEvent.ERROR:
                messagebox.showerror("Error", event.message)
        
        def on_connect(self):
            if self.controller.is_connected:
                self.controller.disconnect()
            else:
                self.controller.connect_async()
        
        def on_auth(self):
            self.controller.authenticate_async()
        
        def on_dump(self):
            filename = filedialog.asksaveasfilename(
                defaultextension=".xci",
                filetypes=[("XCI files", "*.xci")]
            )
            if filename:
                self.controller.dump_xci_async(filename)
        
        def on_closing(self):
            self.controller.close()
            self.root.destroy()
    
    # Run
    root = tk.Tk()
    app = MIGDumperGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()


if __name__ == "__main__":
    example_tkinter_gui()
