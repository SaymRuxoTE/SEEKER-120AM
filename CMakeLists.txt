cmake_minimum_required(VERSION 3.20)
project(Seeker120AM VERSION 4.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Yerel Malzeme (Vendor) Klasörlerimizin Yolları
set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/vendor)
set(IMGUI_DIR ${VENDOR_DIR}/imgui)
set(GLFW_DIR ${VENDOR_DIR}/glfw)

# 2. ImGui Kaynak Dosyaları
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# 3. Kendi Çekirdek Kodlarımız + Arayüz
set(SOURCES
    src/main.cpp
    src/Engine/FlightDynamics.cpp
    src/Radar/RadarSensor.cpp
    ${IMGUI_SOURCES}
)

# Çalıştırılabilir Ana Dosya
add_executable(Seeker120AM_Core ${SOURCES})

# 4. Kütüphane Başlıklarını (Headers) Tanıt
target_include_directories(Seeker120AM_Core PRIVATE 
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLFW_DIR}/include
)

# 5. Hazır Derlenmiş GLFW Kütüphanesini ve Windows Grafik API'lerini Bağla
target_link_directories(Seeker120AM_Core PRIVATE ${GLFW_DIR}/lib-vc2022)
target_link_libraries(Seeker120AM_Core PRIVATE glfw3 opengl32 user32 gdi32 shell32)