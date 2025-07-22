/*
 * renderz - My old random physics engine
 * Copyright (C) 2025  Connor Thomson
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _USE_MATH_DEFINES 
#include <windows.h> 
#include <GL/gl.h>   
#include <GL/glu.h>  
#include <GL/wglext.h> 

#include <iostream>  
#include <vector>    
#include <cmath>     
#include <random>    
#include <chrono>    
#include <iomanip>   
#include <cstdio>    
#include <fcntl.h>   
#include <io.h>      

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const float GRAVITY = 9.81f;        
const float GROUND_Y = -2.0f;       
const float CUBE_SIZE = 0.5f;       
const int NUM_CUBES = 100;          
const float BOUNCE_FACTOR = 1.0f;   
const float FRICTION_FACTOR = 0.9f; 
const float REST_THRESHOLD = 0.05f; 
const float RESET_INTERVAL_SECONDS = 13.0f; 
const float AUTO_ROTATE_SPEED_Y = 100.0f; 
const float CAMERA_HEIGHT_OFFSET = 8.0f; 

const bool DEBUG_MODE = false;

HDC   g_hDC = NULL;
HGLRC g_hRC = NULL;
HWND  g_hWnd = NULL;
HINSTANCE g_hInstance;

float rotateX = 0.0f; 
float rotateY = 0.0f; 

std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
std::uniform_real_distribution<float> dist_xz(-4.0f, 4.0f); 
std::uniform_real_distribution<float> dist_height(5.0f, 15.0f); 
std::uniform_real_distribution<float> dist_bounce_angle(-0.5f, 0.5f); 
std::uniform_real_distribution<float> dist_angular_vel(-180.0f, 180.0f); 
std::uniform_real_distribution<float> dist_color(0.0f, 1.0f); 

std::chrono::high_resolution_clock::time_point lastFrameTime;

float secondTimer = 0.0f;
int secondsCount = 0;

float fpsTimer = 0.0f;
int frameCount = 0;

float resetTimer = 0.0f;

typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;

struct Vec3 {
    float x, y, z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.y); }

    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }

    Vec3 operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }

    float dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }

    Vec3 cross(const Vec3& other) const {
        return Vec3(y * other.z - z * other.y,
                    z * other.x - x * other.y,
                    x * other.y - y * other.x);
    }

    float length() const { return std::sqrt(x * x + y * y + z * z); }

    Vec3 normalize() const {
        float len = length();
        if (len > 0) return Vec3(x / len, y / len, z / len);
        return Vec3(0.0f, 0.0f, 0.0f);
    }
};

struct Cube {
    Vec3 position;
    Vec3 velocity;
    Vec3 angularVelocity; 
    Vec3 rotation;        
    float size;
    bool resting;         
};

std::vector<Cube> cubes; 

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void EnableOpenGL(HWND hWnd, HDC* hDC, HGLRC* hRC);
void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC);
void initOpenGL();
void display();
void reshape(int width, int height);
void resetCubes();
void updatePhysics(float deltaTime);
void drawCube(const Vec3& position, const Vec3& rotation, float size);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc;
    MSG msg;
    BOOL bQuit = FALSE;

    g_hInstance = hInstance; 

    if (DEBUG_MODE) {
        AllocConsole();
        HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
        int hCrt = _open_osfhandle((intptr_t)handle_out, _O_TEXT);
        FILE* hf_out = _fdopen(hCrt, "w");
        setvbuf(hf_out, NULL, _IONBF, 1);
        *stdout = *hf_out;
        std::cout.clear();
    }

    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "OpenGLWinClass";
    RegisterClass(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    g_hWnd = CreateWindowEx(
        0,
        "OpenGLWinClass",
        "",
        WS_POPUP | WS_VISIBLE, 
        0, 
        0, 
        screenWidth, 
        screenHeight, 
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!g_hWnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    EnableOpenGL(g_hWnd, &g_hDC, &g_hRC);

    initOpenGL();

    UpdateWindow(g_hWnd);

    ShowCursor(FALSE);

    lastFrameTime = std::chrono::high_resolution_clock::now();

    while (!bQuit) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                bQuit = TRUE;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {

            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
            lastFrameTime = currentTime;

            updatePhysics(deltaTime);

            frameCount++;
            fpsTimer += deltaTime;
            if (fpsTimer >= 0.5f) { 
                float currentFps = (float)frameCount / fpsTimer;
                if (DEBUG_MODE) { 
                    std::cout << std::fixed << std::setprecision(2) << "FPS: " << currentFps << std::endl;
                }
                frameCount = 0;
                fpsTimer = 0.0f;
            }

            display();
            SwapBuffers(g_hDC); 
        }
    }

    ShowCursor(TRUE);

    if (DEBUG_MODE) {
        FreeConsole();
    }

    DisableOpenGL(g_hWnd, g_hDC, g_hRC);

    DestroyWindow(g_hWnd);

    UnregisterClass("OpenGLWinClass", hInstance);

    return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            break;

        case WM_SIZE:

            reshape(LOWORD(lParam), HIWORD(lParam));
            break;

        case WM_PAINT:
            ValidateRect(hWnd, NULL);
            break;

        case WM_KEYDOWN:
            switch (wParam) {
                case VK_ESCAPE:

                    PostQuitMessage(0);
                    break;

            }
            break;

        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void EnableOpenGL(HWND hWnd, HDC* hDC, HGLRC* hRC) {
    PIXELFORMATDESCRIPTOR pfd;
    int iFormat;

    *hDC = GetDC(hWnd);

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    iFormat = ChoosePixelFormat(*hDC, &pfd);
    SetPixelFormat(*hDC, iFormat, &pfd);

    *hRC = wglCreateContext(*hDC);
    wglMakeCurrent(*hDC, *hRC);

    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

    if (wglSwapIntervalEXT) {

        if (DEBUG_MODE) {
            wglSwapIntervalEXT(0); 
            std::cout << "V-Sync disabled (DEBUG_MODE is true)." << std::endl;
        } else {
            wglSwapIntervalEXT(1); 
            std::cout << "V-Sync enabled (DEBUG_MODE is false)." << std::endl;
        }
    } else {
        if (DEBUG_MODE) { 
            std::cout << "Could not control V-Sync (wglSwapIntervalEXT not found or not supported)." << std::endl;
        }
    }
}

void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC) {
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hWnd, hDC);
}

void initOpenGL() {

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    if (DEBUG_MODE) { 
        std::cout << "Sky color set to black at initialization." << std::endl;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat light_position[] = {1.0f, 1.0f, 1.0f, 0.0f};
    GLfloat light_ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat light_diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat light_specular[] = {1.0f, 1.0f, 1.0f, 1.0f};

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);

    RECT rect;
    GetClientRect(g_hWnd, &rect);
    reshape(rect.right - rect.left, rect.bottom - rect.top);

    resetCubes(); 
}

void resetCubes() {
    cubes.clear();

    for (int i = 0; i < NUM_CUBES; ++i) {
        Cube newCube;
        newCube.size = CUBE_SIZE;
        newCube.velocity = Vec3(0.0f, 0.0f, 0.0f);
        newCube.angularVelocity = Vec3(0.0f, 0.0f, 0.0f);
        newCube.rotation = Vec3(0.0f, 0.0f, 0.0f);
        newCube.resting = false;

        float x_offset = (i % 10 - 5.0f) * (CUBE_SIZE * 2.0f);
        float z_offset = ((i / 10) % 10 - 5.0f) * (CUBE_SIZE * 2.0f);
        float y_offset = (i / 100) * (CUBE_SIZE * 2.0f) + dist_height(rng);

        newCube.position = Vec3(x_offset, y_offset, z_offset);

        cubes.push_back(newCube);
    }
    secondTimer = 0.0f;
    secondsCount = 0;
    fpsTimer = 0.0f;
    frameCount = 0;
    resetTimer = 0.0f;
}

void updatePhysics(float deltaTime) {
    secondTimer += deltaTime;
    if (secondTimer >= 1.0f) {
        secondsCount++;
        if (DEBUG_MODE) { 
            std::cout << "Seconds: " << secondsCount << std::endl;
        }
        secondTimer = 0.0f;
    }

    resetTimer += deltaTime;
    if (resetTimer >= RESET_INTERVAL_SECONDS) {
        if (DEBUG_MODE) { 
            std::cout << "Resetting cubes due to timer." << std::endl;
        }
        resetCubes();
    }

    rotateY += AUTO_ROTATE_SPEED_Y * deltaTime;
    rotateY = fmod(rotateY, 360.0f); 

    for (int i = 0; i < NUM_CUBES; ++i) {
        Cube& cube = cubes[i];

        cube.velocity.y -= GRAVITY * deltaTime;

        cube.position.x += cube.velocity.x * deltaTime;
        cube.position.y += cube.velocity.y * deltaTime;
        cube.position.z += cube.velocity.z * deltaTime;

        cube.rotation.x += cube.angularVelocity.x * deltaTime;
        cube.rotation.y += cube.angularVelocity.y * deltaTime;
        cube.rotation.z += cube.angularVelocity.z * deltaTime;

        cube.rotation.x = fmod(cube.rotation.x, 360.0f);
        cube.rotation.y = fmod(cube.rotation.y, 360.0f);
        cube.rotation.z = fmod(cube.rotation.z, 360.0f);

        float halfSize = cube.size / 2.0f;
        float cube_bottom = cube.position.y - halfSize;

        if (cube_bottom < GROUND_Y) {
            cube.position.y = GROUND_Y + halfSize;

            Vec3 normal = Vec3(0.0f, 1.0f, 0.0f);
            Vec3 random_perturb = Vec3(dist_bounce_angle(rng), 0.0f, dist_bounce_angle(rng));
            Vec3 bounce_direction = (normal + random_perturb).normalize();

            float normal_speed = cube.velocity.dot(normal);
            Vec3 new_normal_velocity = bounce_direction * (-normal_speed * BOUNCE_FACTOR);

            Vec3 tangential_velocity = cube.velocity - (normal * normal_speed);
            tangential_velocity = tangential_velocity * FRICTION_FACTOR;

            cube.velocity = new_normal_velocity + tangential_velocity;

            if (std::abs(normal_speed) > REST_THRESHOLD) {
                cube.angularVelocity = Vec3(dist_angular_vel(rng), dist_angular_vel(rng), dist_angular_vel(rng));
            }
        }

        float bound = 8.0f;
        if (cube.position.x - halfSize < -bound) {
            cube.position.x = -bound + halfSize;
            Vec3 normal = Vec3(1.0f, 0.0f, 0.0f);
            Vec3 random_perturb = Vec3(0.0f, dist_bounce_angle(rng), dist_bounce_angle(rng));
            Vec3 bounce_direction = (normal + random_perturb).normalize();

            float normal_speed = cube.velocity.dot(normal);
            Vec3 new_normal_velocity = bounce_direction * (-normal_speed * BOUNCE_FACTOR);
            Vec3 tangential_velocity = cube.velocity - (normal * normal_speed);
            tangential_velocity = tangential_velocity * FRICTION_FACTOR;
            cube.velocity = new_normal_velocity + tangential_velocity;

            if (std::abs(normal_speed) > REST_THRESHOLD) {
                cube.angularVelocity = Vec3(dist_angular_vel(rng), dist_angular_vel(rng), dist_angular_vel(rng));
            }
        } else if (cube.position.x + halfSize > bound) {
            cube.position.x = bound - halfSize;
            Vec3 normal = Vec3(-1.0f, 0.0f, 0.0f);
            Vec3 random_perturb = Vec3(0.0f, dist_bounce_angle(rng), dist_bounce_angle(rng));
            Vec3 bounce_direction = (normal + random_perturb).normalize();

            float normal_speed = cube.velocity.dot(normal);
            Vec3 new_normal_velocity = bounce_direction * (-normal_speed * BOUNCE_FACTOR);
            Vec3 tangential_velocity = cube.velocity - (normal * normal_speed);
            tangential_velocity = tangential_velocity * FRICTION_FACTOR;
            cube.velocity = new_normal_velocity + tangential_velocity;

            if (std::abs(normal_speed) > REST_THRESHOLD) {
                cube.angularVelocity = Vec3(dist_angular_vel(rng), dist_angular_vel(rng), dist_angular_vel(rng));
            }
        }

        if (cube.position.z - halfSize < -bound) {
            cube.position.z = -bound + halfSize;
            Vec3 normal = Vec3(0.0f, 0.0f, 1.0f);
            Vec3 random_perturb = Vec3(dist_bounce_angle(rng), dist_bounce_angle(rng), 0.0f);
            Vec3 bounce_direction = (normal + random_perturb).normalize();

            float normal_speed = cube.velocity.dot(normal);
            Vec3 new_normal_velocity = bounce_direction * (-normal_speed * BOUNCE_FACTOR);
            Vec3 tangential_velocity = cube.velocity - (normal * normal_speed);
            tangential_velocity = tangential_velocity * FRICTION_FACTOR;
            cube.velocity = new_normal_velocity + tangential_velocity;

            if (std::abs(normal_speed) > REST_THRESHOLD) {
                cube.angularVelocity = Vec3(dist_angular_vel(rng), dist_angular_vel(rng), dist_angular_vel(rng));
            }
        } else if (cube.position.z + halfSize > bound) {
            cube.position.z = bound - halfSize;
            Vec3 normal = Vec3(0.0f, 0.0f, -1.0f);
            Vec3 random_perturb = Vec3(dist_bounce_angle(rng), dist_bounce_angle(rng), 0.0f);
            Vec3 bounce_direction = (normal + random_perturb).normalize();

            float normal_speed = cube.velocity.dot(normal);
            Vec3 new_normal_velocity = bounce_direction * (-normal_speed * BOUNCE_FACTOR);
            Vec3 tangential_velocity = cube.velocity - (normal * normal_speed);
            tangential_velocity = tangential_velocity * FRICTION_FACTOR;
            cube.velocity = new_normal_velocity + tangential_velocity;

            if (std::abs(normal_speed) > REST_THRESHOLD) {
                cube.angularVelocity = Vec3(dist_angular_vel(rng), dist_angular_vel(rng), dist_angular_vel(rng));
            }
        }

        if (cube.velocity.length() < REST_THRESHOLD && cube.angularVelocity.length() < REST_THRESHOLD * 10 && (cube_bottom <= GROUND_Y + REST_THRESHOLD)) {
            cube.resting = true;
            cube.velocity = Vec3(0.0f, 0.0f, 0.0f);
            cube.angularVelocity = Vec3(0.0f, 0.0f, 0.0f);
        } else {
            cube.resting = false;
        }
    }

    for (int i = 0; i < NUM_CUBES; ++i) {
        for (int j = i + 1; j < NUM_CUBES; ++j) {
            Cube& cube1 = cubes[i];
            Cube& cube2 = cubes[j];

            float c1_minX = cube1.position.x - cube1.size / 2.0f;
            float c1_maxX = cube1.position.x + cube1.size / 2.0f;
            float c1_minY = cube1.position.y - cube1.size / 2.0f;

            float c1_maxY = cube1.position.y + cube1.size / 2.0f;
            float c1_minZ = cube1.position.z - cube1.size / 2.0f;
            float c1_maxZ = cube1.position.z + cube1.size / 2.0f;

            float c2_minX = cube2.position.x - cube2.size / 2.0f;

            float c2_maxX = cube2.position.x + cube2.size / 2.0f;

            float c2_minY = cube2.position.y - cube2.size / 2.0f;

            float c2_maxY = cube2.position.y + cube2.size / 2.0f;

            float c2_minZ = cube2.position.z - cube2.size / 2.0f;

            float c2_maxZ = cube2.position.z + cube2.size / 2.0f;

            bool overlapX = (c1_maxX > c2_minX && c1_minX < c2_maxX);
            bool overlapY = (c1_maxY > c2_minY && c1_minY < c2_maxY);
            bool overlapZ = (c1_maxZ > c2_minZ && c1_minZ < c2_maxZ);

            if (overlapX && overlapY && overlapZ) {
                float overlap_x = std::min(c1_maxX, c2_maxX) - std::max(c1_minX, c2_minX);
                float overlap_y = std::min(c1_maxY, c2_maxY) - std::max(c1_minY, c2_minY);
                float overlap_z = std::min(c1_maxZ, c2_maxZ) - std::max(c1_minZ, c2_minZ);

                Vec3 mtv_direction;
                float mtv_magnitude = 0.0f;

                if (overlap_x < overlap_y && overlap_x < overlap_z) {
                    mtv_magnitude = overlap_x;
                    mtv_direction = Vec3((cube1.position.x > cube2.position.x) ? 1.0f : -1.0f, 0.0f, 0.0f);
                } else if (overlap_y < overlap_x && overlap_y < overlap_z) {
                    mtv_magnitude = overlap_y;
                    mtv_direction = Vec3(0.0f, (cube1.position.y > cube2.position.y) ? 1.0f : -1.0f, 0.0f);
                } else {
                    mtv_magnitude = overlap_z;
                    mtv_direction = Vec3(0.0f, 0.0f, (cube1.position.z > cube2.position.z) ? 1.0f : -1.0f);
                }

                float separation_amount = mtv_magnitude / 2.0f + 0.001f;
                cube1.position = cube1.position + mtv_direction * separation_amount;
                cube2.position = cube2.position - mtv_direction * separation_amount;

                float relative_velocity_along_mtv = (cube1.velocity - cube2.velocity).dot(mtv_direction);

                if (relative_velocity_along_mtv < 0) {
                    float impulse = -(1.0f + BOUNCE_FACTOR) * relative_velocity_along_mtv / 2.0f;
                    Vec3 impulse_vector = mtv_direction * impulse;

                    cube1.velocity = cube1.velocity + impulse_vector;
                    cube2.velocity = cube2.velocity - impulse_vector;

                    Vec3 tangential_velocity1 = cube1.velocity - mtv_direction * cube1.velocity.dot(mtv_direction);
                    Vec3 tangential_velocity2 = cube2.velocity - mtv_direction * cube2.velocity.dot(mtv_direction);
                    cube1.velocity = mtv_direction * cube1.velocity.dot(mtv_direction) + tangential_velocity1 * FRICTION_FACTOR;
                    cube2.velocity = mtv_direction * cube2.velocity.dot(mtv_direction) + tangential_velocity2 * FRICTION_FACTOR;

                    cube1.angularVelocity = Vec3(dist_angular_vel(rng), dist_angular_vel(rng), dist_angular_vel(rng));
                    cube2.angularVelocity = Vec3(dist_angular_vel(rng), dist_angular_vel(rng), dist_angular_vel(rng));
                }
            }
        }
    }
}

void drawCube(const Vec3& position, const Vec3& rotation, float size) {
    glPushMatrix();

    glTranslatef(position.x, position.y, position.z);

    glRotatef(rotation.x, 1.0f, 0.0f, 0.0f);
    glRotatef(rotation.y, 0.0f, 1.0f, 0.0f);
    glRotatef(rotation.z, 0.0f, 0.0f, 1.0f);

    glScalef(size / 2.0f, size / 2.0f, size / 2.0f);

    glColor3f(1.0f, 0.0f, 0.0f); 
    glBegin(GL_QUADS);
        glNormal3f(0.0f, 0.0f, 1.0f);
        glVertex3f(-1.0f, -1.0f, 1.0f);
        glVertex3f( 1.0f, -1.0f, 1.0f);
        glVertex3f( 1.0f,  1.0f, 1.0f);
        glVertex3f(-1.0f,  1.0f, 1.0f);
    glEnd();

    glColor3f(0.0f, 1.0f, 0.0f); 
    glBegin(GL_QUADS);
        glNormal3f(0.0f, 0.0f, -1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(-1.0f,  1.0f, -1.0f);
        glVertex3f( 1.0f,  1.0f, -1.0f);
        glVertex3f( 1.0f, -1.0f, -1.0f);
    glEnd();

    glColor3f(0.0f, 0.0f, 1.0f); 
    glBegin(GL_QUADS);
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-1.0f,  1.0f, -1.0f);
        glVertex3f(-1.0f,  1.0f,  1.0f);
        glVertex3f( 1.0f,  1.0f,  1.0f);
        glVertex3f( 1.0f,  1.0f, -1.0f);
    glEnd();

    glColor3f(1.0f, 1.0f, 0.0f); 
    glBegin(GL_QUADS);
        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f( 1.0f, -1.0f, -1.0f);
        glVertex3f( 1.0f, -1.0f,  1.0f);
        glVertex3f(-1.0f, -1.0f,  1.0f);
    glEnd();

    glColor3f(1.0f, 0.0f, 1.0f); 
    glBegin(GL_QUADS);
        glNormal3f(1.0f, 0.0f, 0.0f);
        glVertex3f( 1.0f, -1.0f, -1.0f);
        glVertex3f( 1.0f,  1.0f, -1.0f);
        glVertex3f( 1.0f,  1.0f,  1.0f);
        glVertex3f( 1.0f, -1.0f,  1.0f);
    glEnd();

    glColor3f(0.0f, 1.0f, 1.0f); 
    glBegin(GL_QUADS);
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(-1.0f, -1.0f,  1.0f);
        glVertex3f(-1.0f,  1.0f,  1.0f);
        glVertex3f(-1.0f,  1.0f, -1.0f);
    glEnd();

    glPopMatrix();
}

void display() {

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(0.0, 0.0 + CAMERA_HEIGHT_OFFSET, 15.0, 
              0.0, 0.0, 0.0,                      
              0.0, 1.0, 0.0);                      

    glRotatef(rotateX, 1.0f, 0.0f, 0.0f);
    glRotatef(rotateY, 0.0f, 1.0f, 0.0f);

    for (const auto& cube : cubes) {
        drawCube(cube.position, cube.rotation, cube.size);
    }
}

void reshape(int width, int height) {
    if (height == 0) height = 1;

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
