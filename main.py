import sys
import math
import random
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QHBoxLayout, 
                             QVBoxLayout, QPushButton, QTextEdit, QComboBox, QLabel)
from PyQt5.QtGui import QPainter, QColor, QPen, QBrush, QFont, QPolygonF
from PyQt5.QtCore import Qt, QTimer, QDateTime, QPointF

class RadarDisplay(QWidget):
    """Modern, Hareketli ve Taktiksel Radar (PPI) Ekranı"""
    def __init__(self, main_window):
        super().__init__()
        self.main_window = main_window 
        self.setMinimumSize(500, 500)
        
        self.sweep_angle = 0.0
        self.sweep_speed = 0.25  # Radarın dönüş hızı daha da yavaşlatıldı (Analiz için)
        self.sweep_width_deg = 40.0 
        
        self.targets = []
        self.missiles = [] 
        
        while len(self.targets) < 5:
            self.spawn_new_target()
            
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update_simulation)
        self.timer.start(16)

    def spawn_new_target(self):
        """Yok edilenin yerine yepyeni bir uçak oluşturur (Gizli olarak başlar)"""
        iff_states = ["DOST", "DÜŞMAN", "DÜŞMAN", "BİLİNMEYEN"]
        
        aircraft_db = [
            ("F-22 Raptor", 0.3, 0.40),      
            ("F-35B Lightning II", 0.4, 0.35),
            ("B-2 Spirit", 0.2, 0.20),
            ("F-15SG Eagle", 1.0, 0.25),     
            ("Mig-35 Fulcrum", 0.9, 0.30),
            ("Su-57 Felon", 0.5, 0.35),
            ("TU-160 Blackjack", 1.0, 0.10), 
            ("JF-17 Thunder", 0.9, 0.20)
        ]
        
        r = random.uniform(0.3, 0.9)
        theta_rad = random.uniform(0, 2 * math.pi)
        nx = r * math.cos(theta_rad)
        ny = r * math.sin(theta_rad)
        
        model_data = random.choice(aircraft_db)
        
        target = {
            'id': f"TRG-{random.randint(100, 999)}",
            'nx': nx,
            'ny': ny,
            'heading': random.uniform(0, 2 * math.pi), 
            'speed': random.uniform(0.0002, 0.0005),   
            'model': model_data[0],
            'rcs_visibility': model_data[1], 
            'evade_chance': model_data[2],
            'iff': random.choice(iff_states),
            'last_seen': QDateTime.currentDateTime().addSecs(-10),
            'is_currently_visible': False,
            'discovered': False # HEDEF RADARDA İLK KEZ GÖRÜLDÜ MÜ?
        }
        self.targets.append(target)

    def fire_missiles(self, target_id):
        target = next((t for t in self.targets if t['id'] == target_id), None)
        if not target: return

        offsets = [0, 0.015, -0.015]
        for offset in offsets:
            self.missiles.append({
                'nx': offset,
                'ny': offset,
                'target_id': target_id,
                'speed': 0.012 
            })

    def update_simulation(self):
        self.sweep_angle = (self.sweep_angle - self.sweep_speed) % 360
        now = QDateTime.currentDateTime()
        
        list_needs_update = False

        for t in self.targets:
            t['nx'] += math.cos(t['heading']) * t['speed']
            t['ny'] += math.sin(t['heading']) * t['speed']
            
            dist = math.hypot(t['nx'], t['ny'])
            if dist > 0.95:
                t['heading'] += math.pi 

            tgt_angle_deg = (math.degrees(math.atan2(t['ny'], t['nx']))) % 360
            sweep_visual_angle = (360 - self.sweep_angle) % 360
            angle_diff = (tgt_angle_deg - sweep_visual_angle) % 360
            
            if angle_diff < self.sweep_width_deg or angle_diff > (360 - self.sweep_width_deg):
                if random.random() <= t['rcs_visibility'] + 0.05: 
                    t['is_currently_visible'] = True
                    t['last_seen'] = now
                    
                    # Eğer hedef ilk kez tespit edildiyse listeyi güncelle
                    if not t['discovered']:
                        t['discovered'] = True
                        list_needs_update = True
                        self.main_window.log_action(f"[TESPİT] Yeni temas! {t['id']} radarda belirdi.")
            else:
                t['is_currently_visible'] = False

        if list_needs_update:
            self.main_window.refresh_target_list()

        for m in self.missiles[:]:
            target = next((t for t in self.targets if t['id'] == m['target_id']), None)
            
            if not target:
                self.missiles.remove(m) 
                continue
                
            dx = target['nx'] - m['nx']
            dy = target['ny'] - m['ny']
            dist = math.hypot(dx, dy)
            
            if dist < 0.02: 
                if random.random() < target['evade_chance']:
                    self.main_window.log_action(f"[DİKKAT] {target['id']} füzeden KAÇTI (Chaff/Flare)!")
                    self.missiles.remove(m) 
                else:
                    self.targets.remove(target)
                    self.missiles = [msl for msl in self.missiles if msl['target_id'] != target['id']] 
                    self.spawn_new_target() 
                    self.main_window.refresh_target_list() 
                    self.main_window.log_action(f"[KILL] {target['id']} İMHA EDİLDİ.")
                break 
            else:
                m['nx'] += (dx / dist) * m['speed']
                m['ny'] += (dy / dist) * m['speed']

        self.main_window.update_live_telemetry()
        self.update() 

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        self.setStyleSheet("background-color: #121215;")
        
        size = min(self.width(), self.height()) - 20
        cx = self.width() / 2
        cy = self.height() / 2
        radius = size / 2

        painter.setBrush(QColor(15, 25, 20)) 
        painter.setPen(QPen(QColor(0, 200, 150), 2)) 
        painter.drawEllipse(int(cx - radius), int(cy - radius), int(size), int(size))

        painter.setBrush(Qt.NoBrush)
        painter.setPen(QPen(QColor(0, 150, 100, 80), 1, Qt.DashLine))
        for i in range(1, 5):
            r_ring = radius * (i / 5.0)
            painter.drawEllipse(int(cx - r_ring), int(cy - r_ring), int(r_ring * 2), int(r_ring * 2))

        painter.setPen(QPen(QColor(0, 150, 100, 100), 1))
        painter.drawLine(int(cx), int(cy - radius), int(cx), int(cy + radius))
        painter.drawLine(int(cx - radius), int(cy), int(cx + radius), int(cy))

        current_angle_rad = math.radians(-self.sweep_angle) 
        painter.setPen(QPen(QColor(0, 255, 200, 255), 2))
        end_x = cx + radius * math.cos(current_angle_rad)
        end_y = cy + radius * math.sin(current_angle_rad)
        painter.drawLine(int(cx), int(cy), int(end_x), int(end_y))
        
        painter.setBrush(QBrush(QColor(0, 255, 200, 30)))
        painter.setPen(Qt.NoPen)
        painter.drawPie(int(cx - radius), int(cy - radius), int(size), int(size), 
                        int(self.sweep_angle * 16), int(self.sweep_width_deg * 16))

        now = QDateTime.currentDateTime()
        for t in self.targets:
            if not t['discovered']: continue # Henüz keşfedilmediyse ekranda GÖSTERME
            
            tx = cx + t['nx'] * radius
            ty = cy + t['ny'] * radius

            if t['iff'] == "DOST":
                base_color = QColor(50, 150, 255) 
            else:
                base_color = QColor(255, 50, 50)  

            ms_since = t['last_seen'].msecsTo(now)
            
            if ms_since < 200: 
                alpha = 255
            elif ms_since < 5000: 
                alpha = int(255 * (1.0 - (ms_since / 5000)))
            else:
                alpha = 20 

            if alpha <= 20: continue 

            base_color.setAlpha(alpha)
            painter.setBrush(base_color)
            painter.setPen(QPen(base_color, 1))
            
            poly = QPolygonF()
            jet_size = 8
            p_nose = QPointF(tx + math.cos(t['heading']) * jet_size, 
                             ty + math.sin(t['heading']) * jet_size)
            p_left = QPointF(tx + math.cos(t['heading'] + 2.5) * (jet_size * 0.8), 
                             ty + math.sin(t['heading'] + 2.5) * (jet_size * 0.8))
            p_right = QPointF(tx + math.cos(t['heading'] - 2.5) * (jet_size * 0.8), 
                              ty + math.sin(t['heading'] - 2.5) * (jet_size * 0.8))
            
            poly.append(p_nose)
            poly.append(p_left)
            poly.append(p_right)
            painter.drawPolygon(poly)

            if alpha > 150: 
                painter.setFont(QFont("Consolas", 8, QFont.Bold))
                painter.drawText(int(tx + 10), int(ty + 5), t['id'])

        painter.setBrush(QColor(255, 255, 255)) 
        for m in self.missiles:
            mx = cx + m['nx'] * radius
            my = cy + m['ny'] * radius
            painter.drawEllipse(int(mx - 2), int(my - 2), 4, 4)

class Seeker120AM_C2(QMainWindow):
    def __init__(self):
        super().__init__()
        self.init_ui()
        self.refresh_target_list()

    def init_ui(self):
        self.setWindowTitle("Seeker-120AM - Advanced Tactical C2")
        self.setGeometry(100, 100, 1100, 650)
        
        self.setStyleSheet("""
            QMainWindow { background-color: #1a1a24; }
            QLabel { color: #a0a0b5; font-family: 'Segoe UI', Arial; font-weight: bold; }
        """)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QHBoxLayout(central_widget)

        self.radar = RadarDisplay(self)
        main_layout.addWidget(self.radar, stretch=5)

        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)

        right_layout.addWidget(QLabel("AKTİF HEDEFLER:"))
        self.target_combo = QComboBox()
        self.target_combo.setStyleSheet("""
            QComboBox {
                background-color: #252535; color: white;
                border: 1px solid #444; border-radius: 4px;
                padding: 5px; font-family: Consolas; font-size: 14px;
            }
        """)
        self.target_combo.currentIndexChanged.connect(self.update_live_telemetry)
        right_layout.addWidget(self.target_combo)

        right_layout.addWidget(QLabel("CANLI TELEMETRİ:"))
        self.telemetry_box = QTextEdit()
        self.telemetry_box.setReadOnly(True)
        self.telemetry_box.setStyleSheet("""
            QTextEdit {
                background-color: #0f0f13; color: #00ffcc; 
                font-family: Consolas; font-size: 15px; 
                border: 1px solid #333; border-radius: 4px; padding: 10px;
            }
        """)
        right_layout.addWidget(self.telemetry_box, stretch=3)

        right_layout.addWidget(QLabel("SİSTEM LOGLARI:"))
        self.log_box = QTextEdit()
        self.log_box.setReadOnly(True)
        self.log_box.setStyleSheet("""
            QTextEdit {
                background-color: #0f0f13; color: #aaaaaa; 
                font-family: Consolas; font-size: 13px; 
                border: 1px solid #333; border-radius: 4px; padding: 5px;
            }
        """)
        right_layout.addWidget(self.log_box, stretch=2)

        button_layout = QHBoxLayout()

        self.btn_sal = QPushButton("SAL (IGNORE)")
        self.btn_sal.setCursor(Qt.PointingHandCursor)
        self.btn_sal.setStyleSheet("""
            QPushButton {
                background-color: #fbc02d; color: black; font-weight: bold; font-size: 15px;
                border-radius: 5px; min-height: 45px;
            }
            QPushButton:hover { background-color: #ffeb3b; }
        """)
        self.btn_sal.clicked.connect(self.action_sal)
        button_layout.addWidget(self.btn_sal)

        self.btn_imha = QPushButton("İMHA ET (FOX-3)")
        self.btn_imha.setCursor(Qt.PointingHandCursor)
        self.btn_imha.setStyleSheet("""
            QPushButton {
                background-color: #d32f2f; color: white; font-weight: bold; font-size: 15px;
                border-radius: 5px; min-height: 45px;
            }
            QPushButton:hover { background-color: #f44336; }
        """)
        self.btn_imha.clicked.connect(self.action_imha)
        button_layout.addWidget(self.btn_imha)

        right_layout.addLayout(button_layout)
        main_layout.addWidget(right_panel, stretch=2)

        self.log_action("Seeker-120AM C2 Sistemi Başlatıldı. Radar taraması aktif.")

    def refresh_target_list(self):
        """Sadece KEŞFEDİLMİŞ uçakları listeye ekler"""
        current_selection = self.target_combo.currentData()
        
        self.target_combo.blockSignals(True)
        self.target_combo.clear()
        for t in self.radar.targets:
            if t['discovered']: # Sadece radarın gördüğü hedefler
                self.target_combo.addItem(f"{t['id']} [{t['iff']}]", t['id'])
                
        # Eski seçimi korumaya çalış
        idx = self.target_combo.findData(current_selection)
        if idx >= 0:
            self.target_combo.setCurrentIndex(idx)
            
        self.target_combo.blockSignals(False)
        self.update_live_telemetry()

    def update_live_telemetry(self):
        target_id = self.target_combo.currentData()
        if not target_id: 
            self.telemetry_box.setText("[SİSTEM] Radarda tanımlanmış hedef yok veya bekleniyor...")
            return

        target = next((t for t in self.radar.targets if t['id'] == target_id), None)
        if target:
            dist = math.hypot(target['nx'], target['ny']) * 120 
            speed_mach = target['speed'] * 3000
            heading_deg = (math.degrees(target['heading'])) % 360
            
            data = (
                f"Hedef ID   : {target['id']}\n"
                f"Model      : {target['model']}\n"
                f"Mesafe     : {dist:.1f} NM\n"
                f"Hız        : Mach {speed_mach:.2f}\n"
                f"İstikamet  : {heading_deg:.0f}°\n"
                f"Stealth    : %{int((1.0 - target['rcs_visibility'])*100)}\n"
                f"IFF Durumu : {target['iff']}\n\n"
                f"== CANLI İZLEME AKTİF =="
            )
            self.telemetry_box.setText(data)

    def log_action(self, text):
        self.log_box.append(f"> {text}")
        self.log_box.verticalScrollBar().setValue(self.log_box.verticalScrollBar().maximum())

    def action_sal(self):
        """Seçilen uçağı hava sahasından çıkarır ve yeni uçak getirir"""
        target_id = self.target_combo.currentData()
        if target_id:
            target = next((t for t in self.radar.targets if t['id'] == target_id), None)
            if target:
                self.log_action(f"[SAL] {target_id} hedefine geçiş izni verildi. Hava sahasından ayrılıyor.")
                self.radar.targets.remove(target)
                self.radar.spawn_new_target() # Yerine yenisini yolla
                self.refresh_target_list()

    def action_imha(self):
        target_id = self.target_combo.currentData()
        if target_id:
            target = next((t for t in self.radar.targets if t['id'] == target_id), None)
            if target and target['iff'] == "DOST":
                self.log_action(f"[UYARI] {target_id} DOST UNSUR! Dost ateşi engellendi.")
            else:
                self.log_action(f"[FOX-3] {target_id} hedefine füzeler ateşlendi!")
                self.radar.fire_missiles(target_id)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = Seeker120AM_C2()
    window.show()
    sys.exit(app.exec_())