#!/usr/bin/env python3
"""
CodeCell QC Test GUI
Simple timeline-based interface showing progress for multiple devices
"""

import tkinter as tk
from tkinter import ttk, messagebox
import asyncio
import threading
import time
from datetime import datetime, timedelta
from dataclasses import dataclass
from typing import Dict, List, Optional
import bleak
import struct

# Import our QC test classes
from qc_test_simple import QCTestRunner, QCTestDevice, TestPhase, PHASE_NAMES

@dataclass
class DeviceTimeline:
    name: str
    serial: str
    stages: Dict[str, str]  # stage_name -> status (pending/active/complete/fail)
    current_stage: str
    start_time: Optional[datetime]
    battery_level: int
    usb_connected: bool

class QCTestGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("CodeCell QC Test - Timeline View")
        self.root.geometry("1200x800")
        
        # Test stages in order
        self.test_stages = [
            "Connection",
            "Initial Check", 
            "Charge (2h)",
            "Discharge (2h)",
            "Charge Cycles (3x 15min)",
            "Final Discharge (1h)",
            "Thermal Check",
            "Complete"
        ]
        
        # GUI state
        self.devices: List[DeviceTimeline] = []
        self.current_user_action = None
        self.test_runner = None
        self.test_active = False
        
        self.setup_gui()
        self.start_async_loop()
    
    def setup_gui(self):
        """Create the GUI layout"""
        # Main container
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Title
        title_label = ttk.Label(main_frame, text="CodeCell QC Test Progress", 
                               style="Title.TLabel")
        title_label.grid(row=0, column=0, columnspan=2, pady=(0, 20))
        
        # Control buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        self.start_btn = ttk.Button(button_frame, text="Start QC Test", command=self.start_test)
        self.start_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        self.stop_btn = ttk.Button(button_frame, text="Stop Test", command=self.stop_test, state=tk.DISABLED)
        self.stop_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        # User action frame (initially hidden)
        self.action_frame = ttk.LabelFrame(main_frame, text="User Action Required", padding="10")
        self.action_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        self.action_frame.grid_remove()  # Hide initially
        
        self.action_text = ttk.Label(self.action_frame, text="", font=('Arial', 12, 'bold'))
        self.action_text.pack(pady=(0, 10))
        
        self.action_btn = ttk.Button(self.action_frame, text="Done", command=self.user_action_complete)
        self.action_btn.pack()
        
        # Device timelines container
        self.timeline_frame = ttk.Frame(main_frame)
        self.timeline_frame.grid(row=3, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(3, weight=1)
        
        # Status bar
        self.status_var = tk.StringVar()
        self.status_var.set("Ready to start QC test...")
        status_bar = ttk.Label(main_frame, textvariable=self.status_var, relief=tk.SUNKEN)
        status_bar.grid(row=4, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(10, 0))
    
    def create_device_timeline(self, device: DeviceTimeline):
        """Create timeline widget for a device"""
        device_frame = ttk.LabelFrame(self.timeline_frame, text=f"{device.name} (Serial: {device.serial})", padding="10")
        device_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Device info row
        info_frame = ttk.Frame(device_frame)
        info_frame.pack(fill=tk.X, pady=(0, 10))
        
        battery_label = ttk.Label(info_frame, text=f"Battery: {device.battery_level}%")
        battery_label.pack(side=tk.LEFT, padx=(0, 20))
        
        usb_status = "USB Connected" if device.usb_connected else "USB Disconnected"
        usb_label = ttk.Label(info_frame, text=usb_status)
        usb_label.pack(side=tk.LEFT, padx=(0, 20))
        
        current_stage_label = ttk.Label(info_frame, text=f"Stage: {device.current_stage}", 
                                      font=('Arial', 10, 'bold'))
        current_stage_label.pack(side=tk.RIGHT)
        
        # Timeline row
        timeline_frame = ttk.Frame(device_frame)
        timeline_frame.pack(fill=tk.X)
        
        # Create stage indicators
        for i, stage in enumerate(self.test_stages):
            stage_status = device.stages.get(stage, "pending")
            
            # Choose color based on status
            if stage_status == "complete":
                color = "#4CAF50"  # Green
            elif stage_status == "active":
                color = "#FF9800"  # Orange
            elif stage_status == "fail":
                color = "#F44336"  # Red
            else:
                color = "#E0E0E0"  # Light gray
            
            # Create stage box
            stage_frame = tk.Frame(timeline_frame, bg=color, relief=tk.RAISED, bd=1)
            stage_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=1)
            
            stage_label = tk.Label(stage_frame, text=stage, bg=color, 
                                 font=('Arial', 8), wraplength=80)
            stage_label.pack(pady=2)
            
            # Add connector line (except for last stage)
            if i < len(self.test_stages) - 1:
                connector = tk.Frame(timeline_frame, bg="#CCCCCC", width=2)
                connector.pack(side=tk.LEFT, fill=tk.Y, padx=2)
        
        # Store references for updates
        device_frame.timeline_widgets = {
            'battery_label': battery_label,
            'usb_label': usb_label,
            'current_stage_label': current_stage_label,
            'timeline_frame': timeline_frame
        }
        
        return device_frame
    
    def update_device_timeline(self, device: DeviceTimeline, widget_frame):
        """Update existing device timeline"""
        widgets = widget_frame.timeline_widgets
        
        # Update info
        widgets['battery_label'].config(text=f"Battery: {device.battery_level}%")
        
        usb_status = "USB Connected" if device.usb_connected else "USB Disconnected"
        widgets['usb_label'].config(text=usb_status)
        
        widgets['current_stage_label'].config(text=f"Stage: {device.current_stage}")
        
        # Recreate timeline (simple approach - could be optimized)
        for child in widgets['timeline_frame'].winfo_children():
            child.destroy()
        
        # Recreate stage indicators
        for i, stage in enumerate(self.test_stages):
            stage_status = device.stages.get(stage, "pending")
            
            # Choose color based on status
            if stage_status == "complete":
                color = "#4CAF50"  # Green
            elif stage_status == "active":
                color = "#FF9800"  # Orange
            elif stage_status == "fail":
                color = "#F44336"  # Red
            else:
                color = "#E0E0E0"  # Light gray
            
            # Create stage box
            stage_frame = tk.Frame(widgets['timeline_frame'], bg=color, relief=tk.RAISED, bd=1)
            stage_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=1)
            
            stage_label = tk.Label(stage_frame, text=stage, bg=color, 
                                 font=('Arial', 8), wraplength=80)
            stage_label.pack(pady=2)
            
            # Add connector line (except for last stage)
            if i < len(self.test_stages) - 1:
                connector = tk.Frame(widgets['timeline_frame'], bg="#CCCCCC", width=2)
                connector.pack(side=tk.LEFT, fill=tk.Y, padx=2)
    
    def show_user_action(self, message: str):
        """Show user action prompt"""
        self.current_user_action = message
        self.action_text.config(text=message)
        self.action_frame.grid()  # Show the action frame
        self.action_btn.config(state=tk.NORMAL)
    
    def hide_user_action(self):
        """Hide user action prompt"""
        self.current_user_action = None
        self.action_frame.grid_remove()  # Hide the action frame
    
    def user_action_complete(self):
        """User clicked done on action"""
        self.action_btn.config(state=tk.DISABLED)
        self.hide_user_action()
        
        # Continue with test (this would be handled by the test runner)
        self.status_var.set("User action completed. Continuing test...")
    
    def start_test(self):
        """Start the QC test"""
        self.test_active = True
        self.start_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.NORMAL)
        self.status_var.set("Starting QC test...")
        
        # Start the test in the background
        asyncio.run_coroutine_threadsafe(self.run_qc_test(), self.loop)
    
    def stop_test(self):
        """Stop the QC test"""
        self.test_active = False
        self.start_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)
        self.hide_user_action()
        self.status_var.set("Test stopped by user")
    
    async def run_qc_test(self):
        """Main test execution (runs in async loop)"""
        try:
            self.test_runner = QCTestRunner()
            
            # Phase 1: Device discovery
            self.root.after(0, lambda: self.status_var.set("Discovering QC devices..."))
            device_info = await self.test_runner.discover_devices(timeout=15)
            
            if not device_info:
                self.root.after(0, lambda: messagebox.showerror("Error", "No QC devices found!"))
                return
            
            # Create device timelines
            for info in device_info:
                device = DeviceTimeline(
                    name=info['name'],
                    serial=info['serial'],
                    stages={stage: "pending" for stage in self.test_stages},
                    current_stage="Connection",
                    start_time=datetime.now(),
                    battery_level=0,
                    usb_connected=False
                )
                device.stages["Connection"] = "active"
                self.devices.append(device)
                
                # Create GUI timeline on main thread
                self.root.after(0, lambda d=device: self.create_device_timeline(d))
            
            # Phase 2: Connect to devices
            self.root.after(0, lambda: self.status_var.set(f"Connecting to {len(device_info)} devices..."))
            connected_count = await self.test_runner.connect_all_devices(device_info)
            
            # Update connection status
            for i, device in enumerate(self.devices):
                if i < connected_count:
                    device.stages["Connection"] = "complete"
                    device.stages["Initial Check"] = "active"
                    device.current_stage = "Initial Check"
                else:
                    device.stages["Connection"] = "fail"
                    device.current_stage = "Failed"
            
            self.root.after(0, self.update_all_timelines)
            
            if connected_count == 0:
                self.root.after(0, lambda: messagebox.showerror("Error", "Failed to connect to any devices!"))
                return
            
            # Phase 3: Run test stages with user prompts
            await self.run_test_stages()
            
        except Exception as e:
            self.root.after(0, lambda: messagebox.showerror("Test Error", f"Test failed: {e}"))
        finally:
            self.root.after(0, lambda: setattr(self, 'test_active', False))
            self.root.after(0, lambda: self.start_btn.config(state=tk.NORMAL))
            self.root.after(0, lambda: self.stop_btn.config(state=tk.DISABLED))
    
    async def run_test_stages(self):
        """Run the main test stages with user prompts"""
        if not self.test_active:
            return
        
        # Stage 1: Initial checks
        self.root.after(0, lambda: self.status_var.set("Running initial checks..."))
        await self.test_runner.run_initial_checks()
        
        # Update device status
        for i, device in enumerate(self.devices[:len(self.test_runner.devices)]):
            test_device = self.test_runner.devices[i]
            if test_device.test_results.get('initial_state') == 'PASS':
                device.stages["Initial Check"] = "complete"
                device.stages["Charge (2h)"] = "active"
                device.current_stage = "Charge (2h)"
            else:
                device.stages["Initial Check"] = "fail"
                device.current_stage = "Failed"
        
        self.root.after(0, self.update_all_timelines)
        await asyncio.sleep(1)
        
        # Stage 2: 2-hour charge (with USB connected)
        self.root.after(0, lambda: self.show_user_action("🔌 ENSURE USB hub is PLUGGED IN for 2-hour charge test"))
        
        # Wait for user to confirm
        while self.current_user_action and self.test_active:
            await asyncio.sleep(0.5)
        
        if not self.test_active:
            return
        
        # Run 2-hour charge monitoring
        await self.run_monitoring_phase("Charge (2h)", 2.0, "charging")
        
        # Stage 3: 2-hour discharge (USB unplugged)
        self.root.after(0, lambda: self.show_user_action("🔌 UNPLUG USB hub for 2-hour discharge test"))
        
        # Wait for user to confirm
        while self.current_user_action and self.test_active:
            await asyncio.sleep(0.5)
        
        if not self.test_active:
            return
            
        await self.run_monitoring_phase("Discharge (2h)", 2.0, "discharging")
        
        # Stage 4: Charge cycles (3x 15 minutes)
        for cycle in range(3):
            if not self.test_active:
                return
                
            self.root.after(0, lambda: self.show_user_action(f"🔌 PLUG IN USB hub for charge cycle {cycle + 1}/3 (15 min)"))
            
            # Wait for user to confirm
            while self.current_user_action and self.test_active:
                await asyncio.sleep(0.5)
            
            if not self.test_active:
                return
            
            await self.run_monitoring_phase("Charge Cycles (3x 15min)", 0.25, "charging")
        
        # Stage 5: Final discharge
        self.root.after(0, lambda: self.show_user_action("🔌 UNPLUG USB hub for final 1-hour discharge"))
        
        # Wait for user to confirm
        while self.current_user_action and self.test_active:
            await asyncio.sleep(0.5)
        
        if not self.test_active:
            return
            
        await self.run_monitoring_phase("Final Discharge (1h)", 1.0, "discharging")
        
        # Stage 6: Thermal check
        self.root.after(0, lambda: self.show_user_action("🌡️ CHECK thermal camera - any devices >50°C? Click Done when checked."))
        
        # Wait for user to confirm
        while self.current_user_action and self.test_active:
            await asyncio.sleep(0.5)
        
        # Stage 7: Complete
        for device in self.devices:
            device.stages["Thermal Check"] = "complete"
            device.stages["Complete"] = "complete"
            device.current_stage = "Complete"
        
        self.root.after(0, self.update_all_timelines)
        self.root.after(0, lambda: self.status_var.set("QC Test completed successfully!"))
        self.root.after(0, lambda: messagebox.showinfo("Success", "QC Test completed for all devices!"))
    
    async def run_monitoring_phase(self, stage_name: str, duration_hours: float, phase_type: str):
        """Run a monitoring phase with progress updates"""
        start_time = time.time()
        end_time = start_time + (duration_hours * 3600)
        
        # Update stage to active
        for device in self.devices:
            if device.stages.get(stage_name) != "fail":
                device.stages[stage_name] = "active"
                device.current_stage = stage_name
        
        self.root.after(0, self.update_all_timelines)
        
        # Monitor progress
        while time.time() < end_time and self.test_active:
            remaining_hours = (end_time - time.time()) / 3600
            remaining_minutes = remaining_hours * 60
            
            if remaining_minutes >= 60:
                time_str = f"{remaining_hours:.1f} hours"
            else:
                time_str = f"{remaining_minutes:.0f} minutes"
            
            self.root.after(0, lambda: self.status_var.set(f"{stage_name}: {time_str} remaining ({phase_type})"))
            
            # Update device battery levels (simulate or get real data)
            await self.update_device_status()
            
            await asyncio.sleep(30)  # Check every 30 seconds
        
        # Mark stage complete
        if self.test_active:
            for device in self.devices:
                if device.stages.get(stage_name) == "active":
                    device.stages[stage_name] = "complete"
                    # Set next stage active
                    next_stage = self.get_next_stage(stage_name)
                    if next_stage:
                        device.stages[next_stage] = "active"
                        device.current_stage = next_stage
            
            self.root.after(0, self.update_all_timelines)
    
    def get_next_stage(self, current_stage: str) -> Optional[str]:
        """Get the next stage in the timeline"""
        try:
            current_index = self.test_stages.index(current_stage)
            if current_index < len(self.test_stages) - 1:
                return self.test_stages[current_index + 1]
        except ValueError:
            pass
        return None
    
    async def update_device_status(self):
        """Update device battery levels and USB status"""
        if not self.test_runner or not self.test_runner.devices:
            return
        
        try:
            for i, test_device in enumerate(self.test_runner.devices):
                if i < len(self.devices):
                    status = await test_device.get_status()
                    device = self.devices[i]
                    
                    # Update from actual device status
                    device.battery_level = status.battery_mv // 40  # Rough conversion to percentage
                    device.usb_connected = bool(status.usb_connected)
            
            self.root.after(0, self.update_all_timelines)
        except Exception as e:
            print(f"Error updating device status: {e}")
    
    def update_all_timelines(self):
        """Update all device timeline widgets"""
        # Find existing timeline widgets and update them
        for child in self.timeline_frame.winfo_children():
            if hasattr(child, 'timeline_widgets'):
                # Find corresponding device
                device_name = child.cget('text').split(' (Serial:')[0]
                device = next((d for d in self.devices if d.name == device_name), None)
                if device:
                    self.update_device_timeline(device, child)
    
    def start_async_loop(self):
        """Start the asyncio event loop in a separate thread"""
        def run_loop():
            self.loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self.loop)
            self.loop.run_forever()
        
        self.loop_thread = threading.Thread(target=run_loop, daemon=True)
        self.loop_thread.start()

def main():
    root = tk.Tk()
    app = QCTestGUI(root)
    
    # Configure ttk styles
    style = ttk.Style()
    style.configure('Title.TLabel', font=('Arial', 16, 'bold'))
    
    try:
        root.mainloop()
    finally:
        if hasattr(app, 'loop'):
            app.loop.call_soon_threadsafe(app.loop.stop)

if __name__ == "__main__":
    main()