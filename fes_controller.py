import win32file
import time
import yaml
import sys
import subprocess
import os

class FESController:
    def __init__(self, config_path="config.yaml"):
        with open(config_path, "r") as f:
            self.config = yaml.safe_load(f)
        
        self.pipe_name = self.config["system"].get("pipe_name", r"\\.\pipe\UnityToCPipe")
        self.exe_path = self.config["system"].get("exe_path")
        self.com_port = self.config["system"].get("com_port", "COM4")
        self.stimulation_interval_ms = self.config["system"].get("stimulation_interval_ms", 50)
        self.handle = None
        self.process = None
        
        # State tracking to minimize pipe traffic
        self.last_sent_ma = -1.0
        self.last_sent_width = -1

    def connect(self):
        """Starts the FES executable and connects to its named pipe."""
        if self.exe_path and os.path.exists(self.exe_path):
            print(f"Starting FES executable: {self.exe_path}")
            # Start the process in a new console window
            cmd = [self.exe_path, self.com_port, str(self.stimulation_interval_ms)]
            self.process = subprocess.Popen(cmd, creationflags=subprocess.CREATE_NEW_CONSOLE)
            time.sleep(2)
        else:
            print(f"Warning: FES executable not found at {self.exe_path}. Expecting it to be running manually.")

        print(f"Searching for pipe: {self.pipe_name}...")
        
        timeout = 10 
        start_time = time.time()
        
        while (time.time() - start_time) < timeout:
            try:
                self.handle = win32file.CreateFile(
                    self.pipe_name,
                    win32file.GENERIC_WRITE,
                    0,
                    None,
                    win32file.OPEN_EXISTING,
                    0,
                    None
                )
                print("Successfully connected to the C Stimulator.")
                return True
            except win32file.error as e:
                if e.winerror == 2:
                    time.sleep(0.5)
                else:
                    raise e
        
        print("Error: Connection timeout. Could not find pipe.")
        return False

    def update(self, width_us, amplitude_ma, current_kg=0.0, target_min=0.0, target_max=0.0, target_kg=0.0):
        """
        Sends command if values have changed.
        Format: "width,ma,kg,min,max,target"
        """
        width_us = int(width_us)
        amplitude_ma = float(amplitude_ma)
        
        if (width_us == self.last_sent_width and 
            abs(amplitude_ma - self.last_sent_ma) < 0.1 and
            abs(current_kg - getattr(self, "last_kg", -999)) < 0.1):
            return

        if self.handle is None:
            return

        message = f"{width_us},{amplitude_ma},{current_kg},{target_min},{target_max},{target_kg}"
        try:
            data = message.encode("ascii")
            win32file.WriteFile(self.handle, data)
            # Update cache
            self.last_sent_width = width_us
            self.last_sent_ma = amplitude_ma
            self.last_kg = current_kg
        except win32file.error:
            print("Error: Pipe broken.")
            self.handle = None

    def close(self):
        if self.handle:
            try:
                win32file.CloseHandle(self.handle)
            except:
                pass
            self.handle = None

        if self.process:
            self.process.terminate()
            try:
                self.process.wait(timeout=3)
            except:
                pass
            self.process = None

