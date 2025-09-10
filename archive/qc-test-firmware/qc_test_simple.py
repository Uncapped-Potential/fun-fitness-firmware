#!/usr/bin/env python3
"""
Simple QC Test classes for GUI - extracted from Jupyter notebook
"""

import asyncio
import bleak
import struct
import logging
from dataclasses import dataclass
from typing import Dict, List, Optional

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# BLE Service UUIDs (matching firmware)
QC_SERVICE_UUID = "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2000"
QC_COMMAND_UUID = "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2001"
QC_STATUS_UUID = "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2002"
QC_LOG_UUID = "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2003"

# QC Commands (Safe Mode)
class QCCommand:
    GET_STATUS = 0x01
    START_LOGGING = 0x05
    STOP_LOGGING = 0x06
    CHECKPOINT = 0x07
    SELF_TEST = 0x09

# Test Phases
class TestPhase:
    IDLE = 0
    INITIAL = 1
    CHARGE_2H = 2
    DISCHARGE_2H = 3
    CHARGE_15MIN = 4
    FINAL_DISCHARGE = 5

PHASE_NAMES = {
    0: "IDLE", 1: "INITIAL", 2: "CHARGE_2H", 
    3: "DISCHARGE_2H", 4: "CHARGE_15MIN", 5: "FINAL_DISCHARGE"
}

@dataclass
class QCStatus:
    usb_connected: int
    charging_state: int     
    safe_mode: int
    battery_mv: int
    vcc_mv: int
    timestamp_ms: int
    serial: int

class QCTestDevice:
    """Individual device test controller - Safe Mode"""
    
    def __init__(self, address: str, name: str):
        self.address = address
        self.name = name
        self.serial = None
        self.client = None
        self.connected = False
        self.test_results = {
            'connection': 'PENDING',
            'initial_state': 'PENDING',
            'voltage_monitoring': 'PENDING',
            'thermal_check': 'PENDING',
            'final_result': 'PENDING'
        }
        
    async def connect(self):
        """Connect to device"""
        try:
            logger.info(f"[{self.name}] Connecting...")
            self.client = bleak.BleakClient(self.address)
            await self.client.connect()
            self.connected = True
            self.test_results['connection'] = 'PASS'
            logger.info(f"[{self.name}] Connected successfully")
            
        except Exception as e:
            logger.error(f"[{self.name}] Connection failed: {e}")
            self.test_results['connection'] = 'FAIL'
            raise
    
    async def send_command(self, command: int, data: bytes = b''):
        """Send command to device"""
        if not self.connected:
            raise Exception("Device not connected")
        
        cmd_data = bytes([command]) + data
        await self.client.write_gatt_char(QC_COMMAND_UUID, cmd_data)
        logger.debug(f"[{self.name}] Sent command: 0x{command:02X}")
    
    async def get_status(self) -> QCStatus:
        """Get current device status (Safe Mode)"""
        await self.send_command(QCCommand.GET_STATUS)
        await asyncio.sleep(0.1)  # Wait for response
        
        status_data = await self.client.read_gatt_char(QC_STATUS_UUID)
        if len(status_data) >= 16:  # QCStatus size
            usb_connected, charging_state, safe_mode, _, battery_mv, vcc_mv, timestamp_ms, serial = struct.unpack('<BBBBHHLL', status_data)
            self.serial = serial
            return QCStatus(usb_connected, charging_state, safe_mode, battery_mv, vcc_mv, timestamp_ms, serial)
        else:
            raise Exception("Invalid status response")
    
    async def start_phase(self, phase: int):
        """Start a test phase with logging"""
        await self.send_command(QCCommand.START_LOGGING, bytes([phase]))
        logger.info(f"[{self.name}] Started phase: {PHASE_NAMES[phase]}")
    
    async def run_self_test(self):
        """Run safe mode self-test"""
        await self.send_command(QCCommand.SELF_TEST)
        logger.info(f"[{self.name}] Self-test completed")
    
    async def disconnect(self):
        """Disconnect from device"""
        if self.client and self.connected:
            await self.client.disconnect()
            self.connected = False
            logger.info(f"[{self.name}] Disconnected")

class QCTestRunner:
    """Simple QC test orchestrator"""
    
    def __init__(self):
        self.devices = []
    
    async def discover_devices(self, timeout: int = 10) -> List[Dict[str, str]]:
        """Discover QC test devices"""
        logger.info("Scanning for QC test devices...")
        
        # Scan with service UUID filter (like web version)
        devices = await bleak.BleakScanner.discover(
            timeout=timeout,
            service_uuids=[QC_SERVICE_UUID]  # Filter for QC service
        )
        qc_devices = []
        
        for device in devices:
            # Also scan all devices and filter by name (fallback)
            if device.name and device.name.startswith("QC_"):
                serial = device.name.split("_")[1] if "_" in device.name else "000"
                qc_devices.append({
                    'name': device.name,
                    'address': device.address,
                    'serial': serial
                })
                logger.info(f"Found device: {device.name} ({device.address})")
        
        # If service UUID filter didn't work, try scanning all devices
        if not qc_devices:
            logger.info("No devices found with service UUID, scanning all devices...")
            all_devices = await bleak.BleakScanner.discover(timeout=timeout)
            for device in all_devices:
                if device.name and device.name.startswith("QC_"):
                    serial = device.name.split("_")[1] if "_" in device.name else "000"
                    qc_devices.append({
                        'name': device.name,
                        'address': device.address,
                        'serial': serial
                    })
                    logger.info(f"Found device: {device.name} ({device.address})")
        
        logger.info(f"Found {len(qc_devices)} QC test devices")
        return qc_devices
    
    async def connect_all_devices(self, device_info: List[Dict[str, str]]):
        """Connect to all discovered devices"""
        logger.info(f"Connecting to {len(device_info)} devices...")
        
        self.devices = [QCTestDevice(d['address'], d['name']) for d in device_info]
        
        # Connect in parallel
        connection_tasks = [device.connect() for device in self.devices]
        results = await asyncio.gather(*connection_tasks, return_exceptions=True)
        
        # Check results
        connected_devices = []
        for i, result in enumerate(results):
            if isinstance(result, Exception):
                logger.error(f"Failed to connect to {self.devices[i].name}: {result}")
            else:
                connected_devices.append(self.devices[i])
        
        self.devices = connected_devices
        logger.info(f"Successfully connected to {len(self.devices)} devices")
        
        return len(self.devices)
    
    async def run_initial_checks(self):
        """Run initial hardware checks - Safe Mode"""
        logger.info("=== Running Initial Checks (Safe Mode) ===")
        
        for device in self.devices:
            await device.start_phase(TestPhase.INITIAL)
            await device.run_self_test()
        
        # Wait for initial readings
        await asyncio.sleep(3)
        
        # Check each device
        for device in self.devices:
            try:
                status = await device.get_status()
                logger.info(f"[{device.name}] Initial: USB={status.usb_connected}, VBAT={status.battery_mv}mV, SAFE={status.safe_mode}")
                
                # Safe mode checks
                checks = {
                    'safe_mode': status.safe_mode == 1,
                    'battery': 3000 <= status.battery_mv <= 4200,
                    'connection': True
                }
                
                if all(checks.values()):
                    device.test_results['initial_state'] = 'PASS'
                    logger.info(f"[{device.name}] Initial checks: PASS")
                else:
                    device.test_results['initial_state'] = 'FAIL'
                    logger.error(f"[{device.name}] Initial checks: FAIL")
                
            except Exception as e:
                logger.error(f"[{device.name}] Initial check failed: {e}")
                device.test_results['initial_state'] = 'FAIL'
    
    async def cleanup(self):
        """Disconnect all devices"""
        logger.info("Disconnecting all devices...")
        for device in self.devices:
            try:
                await device.disconnect()
            except Exception as e:
                logger.error(f"Error disconnecting {device.name}: {e}")