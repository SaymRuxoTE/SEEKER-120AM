#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <random>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- DATA STRUCTURES ---

struct Target {
    std::string id;
    float x, y;          
    float vx, vy;        
    float rcs;           
    float altitude;      
    float speed_mach;    
    std::string iff;     // FRIENDLY, HOSTILE, UNKNOWN
    
    float distance;      
    float bearing;       
    float snr;           
    
    bool is_discovered;  
    float alpha;         
    bool is_destroyed;   // Füze vurduğunda veya IGNORE atıldığında anında silinmesi için
};

struct Missile {
    float x, y;          
    std::string target_id;   // DEĞİŞTİ: Artık RAM adresi (pointer) değil, uçağın benzersiz ID'sini tutuyoruz
    bool active;
};

struct Explosion {
    float x, y;
    float radius;
    float alpha;
    bool active;
};

// --- RANDOM NUMBER GENERATORS ---
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dist_pos(-250.0f, 250.0f);
std::uniform_real_distribution<float> dist_vel(-0.8f, 0.8f);
std::uniform_real_distribution<float> dist_rcs(0.01f, 50.0f);
std::uniform_real_distribution<float> dist_alt(10000.0f, 40000.0f);
std::uniform_real_distribution<float> dist_mach(0.6f, 2.5f);
std::uniform_int_distribution<int> dist_iff(0, 2);
std::uniform_int_distribution<int> dist_spawn(1, 100);

std::string generateID() {
    return "TRG-" + std::to_string(100 + rand() % 899);
}

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "[GLFW ERROR] " << error << ": " << description << "\n";
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); 

    GLFWwindow* window = glfwCreateWindow(1280, 720, "SEEKER-120AM GEN-4.5 - Tactical C2 Interface", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.FontGlobalScale = 1.2f; 
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- SYSTEM VARIABLES ---
    std::vector<Target> active_targets;
    std::vector<Missile> active_missiles;
    std::vector<Explosion> explosions;
    std::vector<std::string> system_logs;
    
    system_logs.push_back("> [SYSTEM] Core engine active. Radar sweep initialized...");
    float radar_sweep_angle = 0.0f;
    int selected_target_idx = -1;

    // MAIN SIMULATION LOOP (60 FPS)
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); 

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

        // --- 1. SYSTEM LOGIC UPDATES ---
        
        // A) Radar Sweep
        radar_sweep_angle -= 0.03f;
        if (radar_sweep_angle < -M_PI) radar_sweep_angle += 2.0f * M_PI;

        // B) Random Spawning Mechanics (Normal Operation)
        if (active_targets.size() < 6 && dist_spawn(gen) <= 2) { 
            Target t;
            t.id = generateID();
            t.x = dist_pos(gen); t.y = dist_pos(gen);
            t.vx = dist_vel(gen); t.vy = dist_vel(gen);
            t.rcs = dist_rcs(gen);
            t.altitude = dist_alt(gen);
            t.speed_mach = dist_mach(gen);
            
            int iff_val = dist_iff(gen);
            if (iff_val == 0) t.iff = "FRIENDLY";
            else if (iff_val == 1) t.iff = "HOSTILE";
            else t.iff = "UNKNOWN";

            t.is_discovered = false;
            t.alpha = 0.0f;
            t.is_destroyed = false;
            active_targets.push_back(t);
        }

        // C) Target Updates & Radar Clamping
        for (auto it = active_targets.begin(); it != active_targets.end(); ) {
            it->x += it->vx;
            it->y += it->vy;
            it->distance = std::sqrt(it->x * it->x + it->y * it->y);

            // RADAR CLAMPING (Radius 290)
            if (it->distance > 290.0f) {
                float overlap = it->distance - 290.0f;
                it->x -= (it->x / it->distance) * overlap;
                it->y -= (it->y / it->distance) * overlap;
                it->vx = -it->vx + dist_vel(gen) * 0.2f; 
                it->vy = -it->vy + dist_vel(gen) * 0.2f;
                it->distance = 290.0f;
            }

            it->bearing = std::atan2(it->y, it->x);
            
            float r4 = std::pow(std::max(it->distance, 10.0f), 4.0f);
            it->snr = 10.0f * std::log10((it->rcs * 100000000.0f) / r4);

            float angle_diff = std::abs(radar_sweep_angle - it->bearing);
            if (angle_diff > M_PI) angle_diff = 2.0f * M_PI - angle_diff; 

            // Sweep & Fade
            if (angle_diff < 0.1f) {
                if (!it->is_discovered) {
                    it->is_discovered = true;
                    // Log spami önlemek için 100 hedeften azsa yazdırıyoruz, binlerce hedefte konsolu boğmasın
                    if (active_targets.size() < 100) {
                        system_logs.push_back("> [DETECTION] New contact! " + it->id + " on radar.");
                    }
                }
                it->alpha = 1.0f; 
            } else {
                it->alpha -= 0.005f; 
                if (it->alpha < 0.0f) it->alpha = 0.0f;
            }

            // Cleanup: Removed if destroyed (shot or ignored) or completely faded
            if (it->is_destroyed || (it->is_discovered && it->alpha <= 0.0f)) {
                // DEĞİŞTİ: Uçak silindiğinde sağdaki menü seçiminin kaymasını engelle
                int current_idx = std::distance(active_targets.begin(), it);
                if (selected_target_idx == current_idx) selected_target_idx = -1;
                else if (selected_target_idx > current_idx) selected_target_idx--; 

                it = active_targets.erase(it);
            } else {
                ++it;
            }
        }

        // D) FOX-3 Missile Guidance & Collision
        for (auto m_it = active_missiles.begin(); m_it != active_missiles.end(); ) {
            // DEĞİŞTİ: Hedefi RAM adresinden değil, ID'sinden bul
            auto t_it = std::find_if(active_targets.begin(), active_targets.end(), [&](const Target& t) { return t.id == m_it->target_id; });

            if (t_it != active_targets.end() && !t_it->is_destroyed) {
                float dx = t_it->x - m_it->x;
                float dy = t_it->y - m_it->y;
                float dist = std::sqrt(dx*dx + dy*dy);
                
                if (dist < 5.0f) { 
                    Explosion exp = {m_it->x, m_it->y, 5.0f, 1.0f, true};
                    explosions.push_back(exp);
                    
                    if (!t_it->is_destroyed) {
                        system_logs.push_back("> [SPLASH] " + t_it->id + " successfully intercepted.");
                        t_it->is_destroyed = true; 
                    }
                    m_it = active_missiles.erase(m_it); 
                    continue;
                } else {
                    m_it->x += (dx / dist) * 4.0f;
                    m_it->y += (dy / dist) * 4.0f;
                }
            } else {
                m_it = active_missiles.erase(m_it); 
                continue;
            }
            ++m_it;
        }

        // E) Explosion FX Update
        for (auto e_it = explosions.begin(); e_it != explosions.end(); ) {
            e_it->radius += 2.0f;
            e_it->alpha -= 0.02f;
            if (e_it->alpha <= 0.0f) e_it = explosions.erase(e_it);
            else ++e_it;
        }

        // --- PANEL 1: TACTICAL RADAR PPI ---
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(800, 720));
        ImGui::Begin("Tactical Radar", nullptr, window_flags);
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        float cx = canvas_pos.x + 400.0f;
        float cy = canvas_pos.y + 340.0f;
        
        draw_list->AddCircle(ImVec2(cx, cy), 300.0f, IM_COL32(0, 255, 100, 255), 64, 2.0f); 
        draw_list->AddCircle(ImVec2(cx, cy), 200.0f, IM_COL32(0, 150, 50, 200), 64, 1.0f);  
        draw_list->AddCircle(ImVec2(cx, cy), 100.0f, IM_COL32(0, 150, 50, 200), 64, 1.0f);  
        draw_list->AddLine(ImVec2(cx - 300, cy), ImVec2(cx + 300, cy), IM_COL32(0, 150, 50, 150), 1.0f);
        draw_list->AddLine(ImVec2(cx, cy - 300), ImVec2(cx, cy + 300), IM_COL32(0, 150, 50, 150), 1.0f);

        // Draw Targets
        for (int i = 0; i < active_targets.size(); ++i) {
            const auto& target = active_targets[i];
            if (target.is_discovered && target.alpha > 0.0f) {
                float tx = cx + target.x;
                float ty = cy + target.y;
                
                ImU32 color;
                if (target.iff == "FRIENDLY") {
                    color = IM_COL32(50, 150, 255, (int)(target.alpha * 255)); // Blue
                } else if (target.iff == "HOSTILE") {
                    color = IM_COL32(255, 50, 50, (int)(target.alpha * 255)); // Red
                } else {
                    color = IM_COL32(255, 200, 50, (int)(target.alpha * 255)); // Yellow
                }

                // Vertex tasarrufu için çemberi sadece 6 poligonla (altıgen) çiziyoruz
                draw_list->AddCircleFilled(ImVec2(tx, ty), 4.0f, color, 6);
                
                // CRASH FIX: ImGui 16-bit vertex limitini (65,535) aşmamak için sadece seçili hedefin metnini çiz.
                // Binlerce hedefin metnini aynı anda çizmek grafik motorunu patlatıyordu!
                if (selected_target_idx == i) {
                    draw_list->AddCircle(ImVec2(tx, ty), 8.0f, IM_COL32(255, 255, 255, 255), 12, 1.5f);
                    draw_list->AddText(ImVec2(tx + 8, ty - 8), IM_COL32(255, 255, 255, 255), target.id.c_str());
                }
            }
        }

        // Draw Missiles
        for (const auto& m : active_missiles) {
            draw_list->AddCircleFilled(ImVec2(cx + m.x, cy + m.y), 3.0f, IM_COL32(255, 255, 255, 255));
        }

        // Draw Explosions
        for (const auto& e : explosions) {
            draw_list->AddCircle(ImVec2(cx + e.x, cy + e.y), e.radius, IM_COL32(255, 100, 50, (int)(e.alpha * 255)), 32, 2.0f);
        }

        // Draw Sweep Line
        float end_x = cx + cos(radar_sweep_angle) * 300.0f;
        float end_y = cy + sin(radar_sweep_angle) * 300.0f;
        draw_list->AddLine(ImVec2(cx, cy), ImVec2(end_x, end_y), IM_COL32(0, 255, 100, 255), 3.0f);

        ImGui::End();

        // --- PANEL 2: ADVANCED TELEMETRY ---
        ImGui::SetNextWindowPos(ImVec2(800, 0));
        ImGui::SetNextWindowSize(ImVec2(480, 280));
        ImGui::Begin("Telemetry", nullptr, window_flags);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "== ACTIVE TARGETS =="); 
        
        std::string preview_value = (selected_target_idx >= 0 && selected_target_idx < active_targets.size()) ? active_targets[selected_target_idx].id : "Awaiting Selection...";
        if (ImGui::BeginCombo("##target_select", preview_value.c_str())) {
            for (int i = 0; i < active_targets.size(); ++i) {
                if (active_targets[i].is_discovered) {
                    bool is_selected = (selected_target_idx == i);
                    if (ImGui::Selectable(active_targets[i].id.c_str(), is_selected)) {
                        selected_target_idx = i;
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        if (selected_target_idx >= 0 && selected_target_idx < active_targets.size() && active_targets[selected_target_idx].is_discovered) {
            Target& t = active_targets[selected_target_idx];
            
            ImVec4 iff_color;
            if (t.iff == "FRIENDLY") iff_color = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
            else if (t.iff == "HOSTILE") iff_color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
            else iff_color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            
            ImGui::Text("TARGET ID  : %s", t.id.c_str());
            ImGui::Text("STATUS     : TRACKING");
            ImGui::Text("RANGE      : %.1f NM", t.distance * 0.539957f); 
            ImGui::Text("BEARING    : %.1f DEG", t.bearing * (180.0f / M_PI));
            ImGui::Text("ALTITUDE   : %.0f FT", t.altitude);
            ImGui::Text("SPEED      : %.2f MACH", t.speed_mach);
            ImGui::Text("RCS        : %.3f m^2", t.rcs);
            ImGui::Text("EST. SNR   : %.2f dB", t.snr);
            ImGui::Text("IFF STATUS : ");
            ImGui::SameLine();
            ImGui::TextColored(iff_color, "%s", t.iff.c_str());
        }
        ImGui::End();

        // --- PANEL 3: COMMAND & CONTROL (ROE) ---
        ImGui::SetNextWindowPos(ImVec2(800, 280));
        ImGui::SetNextWindowSize(ImVec2(480, 100));
        ImGui::Begin("C2 Operations", nullptr, window_flags);
        
        if (ImGui::Button("IGNORE (SAL)", ImVec2(200, 40))) {
            if (selected_target_idx >= 0 && selected_target_idx < active_targets.size()) {
                system_logs.push_back("> [SYSTEM] " + active_targets[selected_target_idx].id + " dropped from tracking (IGNORED).");
                active_targets[selected_target_idx].is_destroyed = true; // Anında siler
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("ENGAGE (FOX-3)", ImVec2(200, 40))) {
            if (selected_target_idx >= 0 && selected_target_idx < active_targets.size()) {
                Target& t = active_targets[selected_target_idx];
                if (t.iff == "FRIENDLY") {
                    system_logs.push_back("> [LOCK] " + t.id + " FRIENDLY CONTACT! Blue-on-blue engagement prevented.");
                } else {
                    system_logs.push_back("> [SYSTEM] FOX-3 FIRED! Target: " + t.id);
                    for(int i=0; i<3; i++) {
                        // DEĞİŞTİ: Füze yaratılırken pointer (&t) yerine uçağın ID'si (t.id) veriliyor
                        Missile m = { (float)(rand()%10 - 5), (float)(rand()%10 - 5), t.id, true };
                        active_missiles.push_back(m);
                    }
                }
            }
        }
        ImGui::End();

        // --- PANEL 3.5: BENCHMARK & STRESS TEST ---
        ImGui::SetNextWindowPos(ImVec2(800, 380)); 
        ImGui::SetNextWindowSize(ImVec2(480, 130));
        ImGui::Begin("Benchmark Suite", nullptr, window_flags);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "== EXTREME LOAD TESTING ==");
        ImGui::Separator();
        
        // Askeri Sistem Güvenlik Limiti (Hard Cap)
        const int MAX_TRACKS = 2000; 
        
        ImGui::Text("Active Targets: %d / %d MAX", (int)active_targets.size(), MAX_TRACKS);
        ImGui::Text("Frame Time    : %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("FPS           : %.1f", ImGui::GetIO().Framerate);
        
        ImGui::Separator();
        if (ImGui::Button("+50 UNITS", ImVec2(100, 30))) {
            int to_spawn = std::min(50, MAX_TRACKS - (int)active_targets.size());
            for(int i=0; i<to_spawn; i++) {
                Target t;
                t.id = generateID();
                t.x = dist_pos(gen); t.y = dist_pos(gen);
                t.vx = dist_vel(gen); t.vy = dist_vel(gen);
                t.rcs = dist_rcs(gen); t.altitude = dist_alt(gen); t.speed_mach = dist_mach(gen);
                t.iff = (dist_iff(gen) == 0) ? "FRIENDLY" : "HOSTILE";
                t.is_discovered = false; t.alpha = 0.0f; t.is_destroyed = false;
                active_targets.push_back(t);
            }
            if (to_spawn > 0) system_logs.push_back("> [BENCHMARK] " + std::to_string(to_spawn) + " units spawned.");
            else system_logs.push_back("> [WARNING] SYSTEM TRACK LIMIT REACHED (2000)!");
        }
        ImGui::SameLine();
        if (ImGui::Button("+500 UNITS", ImVec2(100, 30))) {
            int to_spawn = std::min(500, MAX_TRACKS - (int)active_targets.size());
            for(int i=0; i<to_spawn; i++) {
                Target t;
                t.id = generateID();
                t.x = dist_pos(gen); t.y = dist_pos(gen);
                t.vx = dist_vel(gen); t.vy = dist_vel(gen);
                t.rcs = dist_rcs(gen); t.altitude = dist_alt(gen); t.speed_mach = dist_mach(gen);
                t.iff = "UNKNOWN";
                t.is_discovered = false; t.alpha = 0.0f; t.is_destroyed = false;
                active_targets.push_back(t);
            }
            if (to_spawn > 0) system_logs.push_back("> [BENCHMARK] HEAVY LOAD: " + std::to_string(to_spawn) + " units spawned.");
            else system_logs.push_back("> [WARNING] SYSTEM TRACK LIMIT REACHED (2000)!");
        }
        ImGui::SameLine();
        if (ImGui::Button("+1000 UNITS", ImVec2(120, 30))) {
            int to_spawn = std::min(1000, MAX_TRACKS - (int)active_targets.size());
            active_targets.reserve(active_targets.size() + to_spawn); 
            for(int i=0; i<to_spawn; i++) {
                Target t;
                t.id = generateID();
                t.x = dist_pos(gen); t.y = dist_pos(gen);
                t.vx = dist_vel(gen); t.vy = dist_vel(gen);
                t.rcs = dist_rcs(gen); t.altitude = dist_alt(gen); t.speed_mach = dist_mach(gen);
                t.iff = "HOSTILE";
                t.is_discovered = false; t.alpha = 0.0f; t.is_destroyed = false;
                active_targets.push_back(t);
            }
            if (to_spawn > 0) system_logs.push_back("> [BENCHMARK] EXTREME LOAD: " + std::to_string(to_spawn) + " units spawned.");
            else system_logs.push_back("> [WARNING] SYSTEM TRACK LIMIT REACHED (2000)!");
        }
        ImGui::End();

        // --- PANEL 4: SYSTEM LOGS ---
        ImGui::SetNextWindowPos(ImVec2(800, 510));
        ImGui::SetNextWindowSize(ImVec2(480, 210));
        ImGui::Begin("System Logs", nullptr, window_flags);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "SYSTEM LOGS:"); 
        ImGui::Separator();
        
        int start_idx = std::max(0, (int)system_logs.size() - 10);
        for (int i = start_idx; i < system_logs.size(); ++i) {
            if (system_logs[i].find("LOCK") != std::string::npos || system_logs[i].find("FIRED") != std::string::npos || system_logs[i].find("SPLASH") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", system_logs[i].c_str()); 
            } else {
                ImGui::Text("%s", system_logs[i].c_str()); 
            }
        }
        ImGui::End();

        // 4. Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.05f, 0.07f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}