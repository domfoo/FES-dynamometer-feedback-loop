import cv2
import numpy as np
import easyocr
import time
from collections import deque

class GripVision:
    def __init__(self, config):
        self.config = config
        self.camera_index = config["system"]["camera"]["index"]
        self.focus_init = config["system"]["camera"]["focus"]
        
        # Init OCR
        print("Initializing EasyOCR...")
        self.reader = easyocr.Reader(["en"], gpu=True)
        
        # Init Camera
        print(f"Opening Camera Index {self.camera_index}...")
        self.cap = cv2.VideoCapture(self.camera_index, cv2.CAP_DSHOW)
        self.apply_camera_settings(self.focus_init)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
        
        # Warmup
        print("Warming up camera...")
        for _ in range(5):
            self.cap.read()
            time.sleep(0.1)
        self.apply_camera_settings(self.focus_init)

        # UI Init
        cv2.namedWindow("Control Panel")
        cv2.createTrackbar("Threshold", "Control Panel", config["system"]["camera"]["threshold"], 255, lambda x: None)
        cv2.createTrackbar("Focus", "Control Panel", self.focus_init, 255, lambda x: self.apply_camera_settings(x))
        
        # State
        self.last_kg = "0.0"
        self.frame_count = 0
        self.dst_pts = np.array([[0,0], [1280,0], [1280,720], [0,720]], dtype="float32")
        
        # ROI Defaults (can be overridden by UI)
        self.roi = config["system"]["camera"]["roi"]

    def apply_camera_settings(self, focus_val):
        self.cap.set(cv2.CAP_PROP_AUTOFOCUS, 0) 
        self.cap.set(cv2.CAP_PROP_FOCUS, focus_val)
        self.cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)
        self.cap.set(cv2.CAP_PROP_EXPOSURE, -6) 
        self.cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))

    def _order_points(self, pts):
        rect = np.zeros((4, 2), dtype="float32")
        s = pts.sum(axis=1)
        rect[0] = pts[np.argmin(s)]
        rect[2] = pts[np.argmax(s)]
        diff = np.diff(pts, axis=1)
        rect[1] = pts[np.argmin(diff)]
        rect[3] = pts[np.argmax(diff)]
        return rect

    def _clean_screen_noise(self, image):
        if image.size == 0: return image
        scale = 2
        h, w = image.shape
        big = cv2.resize(image, (w * scale, h * scale), interpolation=cv2.INTER_LINEAR)
        blurred = cv2.GaussianBlur(big, (1, 5), 0)
        _, binary = cv2.threshold(blurred, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
        return binary

    def get_frame(self):
        """
        Captures and processes a single frame.
        Returns:
            frame: The annotated video frame (or None if failed)
            kg_value: The parsed float value
            roi_status: String status ("STABLE", "NO SCREEN", etc.)
        """
        ret, frame = self.cap.read()
        if not ret: return None, 0.0, "ERROR"
        
        self.frame_count += 1
        
        # Get UI Params
        t_val = cv2.getTrackbarPos("Threshold", "Control Panel")
        
        rx, ry, rw, rh = 470, 416, 229, 71

        # Image Processing
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        gray_blurred = cv2.medianBlur(gray, 5) 
        _, mask = cv2.threshold(gray_blurred, t_val, 255, cv2.THRESH_BINARY)
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (15, 15))
        mask_closed = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
        contours, _ = cv2.findContours(mask_closed, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        warped = None
        if contours:
            cnt = max(contours, key=cv2.contourArea)
            if cv2.contourArea(cnt) > 30000:
                epsilon = 0.02 * cv2.arcLength(cnt, True)
                approx = cv2.approxPolyDP(cnt, epsilon, True)
                if len(approx) == 4:
                    cv2.drawContours(frame, [approx], -1, (0, 255, 0), 2)
                    rect_pts = self._order_points(approx.reshape(4, 2))
                    M = cv2.getPerspectiveTransform(rect_pts, self.dst_pts)
                    warped = cv2.warpPerspective(gray, M, (1280, 720))

        roi_status = "NO SCREEN"
        if warped is not None:
            roi = warped[ry:ry+rh, rx:rx+rw]
            if roi.size > 0:
                roi_status = "STABLE"
                roi_clean = self._clean_screen_noise(roi)
                if self.frame_count % 5 == 0:
                    results = self.reader.readtext(roi_clean, allowlist="0123456789.", detail=1)
                    if results:
                        best = max(results, key=lambda x: x[2]) 
                        text = best[1]
                        if "." not in text and len(text) > 1:
                            text = text[:-1] + "." + text[-1]
                        self.last_kg = text
                
                cv2.imshow("LOCKED ROI", roi_clean)

        # Parse
        try:
            clean_val = "".join(c for c in self.last_kg if c.isdigit() or c == ".")
            if clean_val.count(".") > 1: clean_val = clean_val.replace(".", "", clean_val.count(".") - 1)
            val = float(clean_val) if clean_val else 0.0
        except ValueError:
            val = 0.0
            
        return frame, val, roi_status

    def show_panel(self, frame):
        cv2.imshow("Control Panel", cv2.resize(frame, (1280, 720)))

    def release(self):
        self.cap.release()
        cv2.destroyAllWindows()
