import cv2
import numpy as np
import time
import sys
import os
import yaml
from collections import deque
from fes_controller import FESController
from vision_core import GripVision

class SignalSmoother:
    def __init__(self, history_size=5, alpha=0.1):
        self.history = deque(maxlen=history_size)
        self.smoothed = 0.0
        self.alpha = alpha

    def update(self, raw_val):
        if raw_val is None: return self.smoothed
        self.history.append(raw_val)
        median_val = sorted(self.history)[len(self.history)//2]
        if self.smoothed == 0.0 and median_val > 0:
            self.smoothed = median_val
        else:
            self.smoothed = self.smoothed * (1 - self.alpha) + median_val * self.alpha
        return self.smoothed

def draw_ui(frame, kg_val, history, status_msg, status_color, 
            instruction="", sub_text="", target_kg=None, limits=None):
    
    # Bottom Instruction Bar
    cv2.rectangle(frame, (100, 780), (1820, 1020), (0, 0, 0), -1)
    cv2.rectangle(frame, (100, 780), (1820, 1020), (100, 100, 100), 2)
    
    if instruction:
        cv2.putText(frame, instruction, (150, 860), cv2.FONT_HERSHEY_DUPLEX, 1.8, (255, 255, 255), 2)
    if sub_text:
        cv2.putText(frame, sub_text, (150, 950), cv2.FONT_HERSHEY_PLAIN, 2.5, (0, 255, 255), 2)

    # Top Left Status
    cv2.putText(frame, f"{kg_val:.1f} KG", (100, 150), cv2.FONT_HERSHEY_DUPLEX, 3.0, status_color, 5)
    cv2.putText(frame, status_msg, (100, 220), cv2.FONT_HERSHEY_PLAIN, 2.0, status_color, 2)

    # Graph
    g_x, g_y, gw, gh = 1200, 800, 550, 180
    cv2.rectangle(frame, (g_x, g_y), (g_x + gw, g_y + gh), (30, 30, 30), -1)
    
    max_val = max(list(history) + ([target_kg] if target_kg else []) + ([limits[1]] if limits else []) + [1.0])
    scale = max_val * 1.3
    
    def get_y(v): return g_y + gh - int((v / scale) * gh)

    if limits:
        y_low, y_high = get_y(limits[0]), get_y(limits[1])
        y_low, y_high = max(g_y, min(g_y+gh, y_low)), max(g_y, min(g_y+gh, y_high))
        if y_high < y_low:
            roi = frame[y_high:y_low, g_x:g_x+gw]
            green = np.zeros_like(roi); green[:] = (0, 150, 0)
            cv2.addWeighted(green, 0.3, roi, 0.7, 0, roi)
            frame[y_high:y_low, g_x:g_x+gw] = roi

    pts = [[g_x + int(i * (gw/history.maxlen)), get_y(v)] for i, v in enumerate(history)]
    if len(pts) > 1:
        cv2.polylines(frame, [np.array(pts, dtype=np.int32)], False, (255, 255, 255), 2)
    if target_kg:
        cv2.putText(frame, f"TARGET: {target_kg:.1f} KG", (g_x, g_y - 15), cv2.FONT_HERSHEY_PLAIN, 1.5, (200, 200, 200), 2)

def calibrate(config, vision, fes):
    STATE_INIT, STATE_MAX_STRENGTH, STATE_RELAX, STATE_FES_MIN, STATE_FES_MAX, STATE_DONE = range(6)
    current_state = STATE_INIT
    state_start_time = time.time()
    max_strength_values = []
    max_strength_final = 0.0
    fes_current_ma, fes_min_ma, fes_max_ma, fes_last_update = 0.0, 0.0, 0.0, 0
    
    smoother = SignalSmoother()
    history = deque(maxlen=550)

    print("Starting Calibration Mode.")
    try:
        while True:
            frame, raw_kg, roi_status = vision.get_frame()
            if frame is None: break
            
            current_kg_val = smoother.update(raw_kg)
            history.append(current_kg_val)
            instruction, sub_text = "", ""
            status_msg, status_color = "CALIBRATION", (0, 255, 255)
            key = cv2.waitKey(1) & 0xFF

            if current_state == STATE_INIT:
                instruction, sub_text = "Step 1: Calibration", "Press SPACE to start Max Strength Test"
                if key == 32:
                    current_state = STATE_MAX_STRENGTH
                    state_start_time, max_strength_values = time.time(), []
            
            elif current_state == STATE_MAX_STRENGTH:
                elapsed = time.time() - state_start_time
                remaining = 5 - elapsed
                instruction, sub_text = "SQUEEZE MAX STRENGTH!", f"Hold for {remaining:.1f}s..."
                if roi_status == "STABLE": max_strength_values.append(current_kg_val)
                if remaining <= 0:
                    max_strength_final = np.percentile(max_strength_values, 90) if max_strength_values else current_kg_val
                    current_state, state_start_time = STATE_RELAX, time.time()

            elif current_state == STATE_RELAX:
                instruction, sub_text = f"Max Recorded: {max_strength_final:.1f} KG", "Relax... Press SPACE for FES Floor Test."
                if key == 32:
                    current_state, fes_current_ma, fes_last_update = STATE_FES_MIN, 0.0, time.time()

            elif current_state == STATE_FES_MIN:
                instruction, sub_text = "FES Calibration: FLOOR", f"Current: {fes_current_ma:.1f} mA. Press SPACE when FELT."
                if time.time() - fes_last_update > 0.5:
                    fes_current_ma += 0.5
                    fes.update(200, fes_current_ma, 0, 0, 0)
                    fes_last_update = time.time()
                if key == 32:
                    fes_min_ma, current_state = fes_current_ma, STATE_FES_MAX
                    time.sleep(0.5)

            elif current_state == STATE_FES_MAX:
                instruction, sub_text = "FES Calibration: CEILING", f"Current: {fes_current_ma:.1f} mA. Press SPACE when UNCOMFORTABLE."
                if time.time() - fes_last_update > 0.5:
                    fes_current_ma += 0.5
                    fes.update(200, fes_current_ma, 0, 0, 0)
                    fes_last_update = time.time()
                if key == 32:
                    fes_max_ma = fes_current_ma
                    fes.update(0, 0, 0, 0, 0)
                    current_state, state_start_time = STATE_DONE, time.time()
                    results = {"max_strength": float(max_strength_final), "fes_min_ma": float(fes_min_ma), 
                               "fes_max_ma": float(fes_max_ma), "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")}
                    with open("calibration_data.yaml", "w") as f: yaml.dump(results, f)
                    print(f"Results saved: {results}")

            elif current_state == STATE_DONE:
                elapsed_done = time.time() - state_start_time
                instruction, sub_text = "CALIBRATION COMPLETE", f"Closing in {max(0, 3 - int(elapsed_done))}s..."
                if elapsed_done > 3: break

            draw_ui(frame, current_kg_val, history, status_msg, status_color, instruction, sub_text)
            vision.show_panel(frame)
            if key == ord("q"): break
    except KeyboardInterrupt: pass

def run_session(config, vision, fes):
    calib_file = "calibration_data.yaml"
    if not os.path.exists(calib_file):
        print(f"ERROR: {calib_file} not found! Run 'calibrate' first.")
        return
    with open(calib_file, "r") as f: calib_data = yaml.safe_load(f)

    session_config = config["system"]["session"]
    TARGET_PCT, TOLERANCE_PCT = session_config["target_percentage"], session_config["tolerance_percentage"]
    PULSE_WIDTH, SESSION_DURATION = session_config["pulse_width_us"], session_config.get("session_duration_seconds", 60)
    FES_MIN_FACTOR = session_config.get("fes_min_factor", 1.0)
    RAMP_RATE = session_config.get("ramp_rate_ma_per_second", 1.0)
    GRACE_PERIOD = session_config.get("grace_period_seconds", 3)

    TARGET_KG = calib_data["max_strength"] * TARGET_PCT
    LOWER_LIMIT, UPPER_LIMIT = TARGET_KG * (1.0 - TOLERANCE_PCT), TARGET_KG * (1.0 + TOLERANCE_PCT)
    FES_FLOOR_START, FES_CEILING_SAFE = calib_data["fes_min_ma"] * FES_MIN_FACTOR, calib_data["fes_max_ma"] * 0.9 

    STATE_WAIT, STATE_RUNNING, STATE_FINISHED = range(3)
    current_state, session_start_time = STATE_WAIT, 0
    current_ma, is_fes_active, last_fes_time = 0.0, False, time.time()
    
    smoother = SignalSmoother()
    history = deque(maxlen=550)

    print("Session Ready. Press SPACE to start.")
    try:
        while True:
            frame, raw_kg, status = vision.get_frame()
            if frame is None: break
            
            current_time = time.time()
            current_kg_val = smoother.update(raw_kg)
            history.append(current_kg_val)
            key = cv2.waitKey(1) & 0xFF
            
            instruction, sub_text, status_msg = "", "", "WAITING"
            status_color = (0, 255, 255)

            if current_state == STATE_WAIT:
                instruction, sub_text = "Session Ready", "Press SPACE to start"
                if key == 32:
                    current_state, session_start_time = STATE_RUNNING, time.time()
            
            elif current_state == STATE_RUNNING:
                elapsed = time.time() - session_start_time
                remaining = max(0, SESSION_DURATION - elapsed)
                grace_msg = ""
                
                if elapsed < GRACE_PERIOD:
                    is_fes_active, current_ma = False, 0.0
                    grace_msg = f" (GRACE: {int(GRACE_PERIOD - elapsed)}s)"
                else:
                    if current_kg_val < LOWER_LIMIT:
                        if not is_fes_active:
                            is_fes_active, current_ma, last_fes_time = True, FES_FLOOR_START, current_time
                        else:
                            dt = current_time - last_fes_time
                            if dt > 0.1:
                                current_ma = min(current_ma + RAMP_RATE * dt, FES_CEILING_SAFE)
                                last_fes_time = current_time
                    else:
                        is_fes_active, current_ma = False, 0.0

                fes.update(PULSE_WIDTH, current_ma if is_fes_active else 0, current_kg_val, LOWER_LIMIT, UPPER_LIMIT, TARGET_KG)
                status_color = (0, 255, 0) if LOWER_LIMIT <= current_kg_val <= UPPER_LIMIT else (0, 0, 255)
                status_msg = f"RUNNING: {int(remaining)}s LEFT | {current_ma:.1f} mA{grace_msg}"
                if remaining <= 0:
                    current_state = STATE_FINISHED
                    fes.update(0, 0, current_kg_val, LOWER_LIMIT, UPPER_LIMIT, TARGET_KG)

            elif current_state == STATE_FINISHED:
                status_color, status_msg = (255, 255, 0), "COMPLETE. PRESS 'Q' TO EXIT."
                fes.update(0, 0, current_kg_val, LOWER_LIMIT, UPPER_LIMIT, TARGET_KG)

            draw_ui(frame, current_kg_val, history, status_msg, status_color, instruction, sub_text, 
                    target_kg=TARGET_KG, limits=(LOWER_LIMIT, UPPER_LIMIT))
            vision.show_panel(frame)
            if key == ord("q"): break
    except KeyboardInterrupt: pass

def main():
    import argparse
    parser = argparse.ArgumentParser(description="FES Grip System Control")
    parser.add_argument("mode", choices=["run", "calibrate"], help="Mode to execute: 'run' for session, 'calibrate' for calibration")
    parser.add_argument("--config", type=str, default="config.yaml", help="Path to configuration file")
    args = parser.parse_args()

    if not os.path.exists(args.config):
        print(f"ERROR: Configuration file '{args.config}' not found.")
        sys.exit(1)

    with open(args.config, "r") as f:
        config = yaml.safe_load(f)

    print("Initializing Vision Core...")
    vision = GripVision(config)

    print("Initializing FES...")
    fes = FESController(config_path=args.config)
    if not fes.connect():
        print("WARNING: FES connection failed. Continuing in simulation mode.")

    try:
        if args.mode == "calibrate":
            calibrate(config, vision, fes)
        elif args.mode == "run":
            run_session(config, vision, fes)
    finally:
        vision.release()
        fes.close()

if __name__ == "__main__":
    main()
