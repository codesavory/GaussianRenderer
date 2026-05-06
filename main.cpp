//
//  main.cpp
//  CppTarget
//
//  Created by Suriya Dakshina Murthy on 10/2/25.
//

#include <iostream>
#include <vector>
#include <fstream>
#include <filesystem>
//#include "RenderGaussian.h"

using namespace std;

struct float2 {
    float x, y;

    float2(float x, float y) : x(x), y(y) {};
    float2 operator-(const float2& other) const {
        return float2(x-other.x, y-other.y);
    }
};

struct float3 {
    float x, y, z;

    float3() : x(0), y(0), z(0) {};
    float3(float x, float y, float z) : x(x), y(y), z(z) {};
    float3(float i) : x(i), y(i), z(i) {};

    float3 operator*(const float3& other) const {
        return float3(x*other.x, y*other.y, z*other.z);
    }

    float3& operator+=(const float3& other) {
        x+=other.x;
        y+=other.y;
        z+=other.z;
        return *this;
    }
};

struct gaussian {
    float2 mu; // center (x,y)
    float sigma; // radius/spread
    float3 color; // RGB
    float alpha; // opacity weight

    gaussian(float2 mu, float sigma, float3 color, float alpha): mu(mu), sigma(sigma), color(color), alpha(alpha) {};
};

float squaredDistance(float2 a) {
    return (a.x*a.x) + (a.y*a.y);
}

// Sampling a Gaussian — returns straight color (no alpha baked in)
void splat(const gaussian& gauss, int x, int y, float3& pColor, float& pAlpha) {
    float2 pixelCenter = float2(x+0.5f, y+0.5f);
    float2 offset = pixelCenter - gauss.mu;
    float r2 = squaredDistance(offset);
    float gaussianFallOff = expf(-0.5f * r2 / (gauss.sigma*gauss.sigma));

    pAlpha = gauss.alpha * gaussianFallOff;
    pColor = gauss.color;  // straight color — compositing happens at write time
}

// Returns straight color buffer and alpha buffer separately
pair<vector<float3>, vector<float>> renderGaussian(gaussian gauss, int width, int height) {
    vector<float3> accumRGB(width*height, float3(0));
    vector<float> accumAlpha(width*height, 0.0f);
    float3 pixelColor;
    float pixelAlpha;
    for(int j=0; j<height; j++) {
        for(int i=0; i<width; i++) {
            splat(gauss, i, j, pixelColor, pixelAlpha);
            accumRGB[j*width + i]   = pixelColor;
            accumAlpha[j*width + i] = pixelAlpha;  // fix: was i*width + i
        }
    }
    return {accumRGB, accumAlpha};
}

// Composites color * alpha over black background at write time
void writePPM(const vector<float3>& colorBuf, const vector<float>& alphaBuf,
              int W, int H, const string& filename)
{
    ofstream f(filename, ios::binary);
    if (!f) { cerr << "writePPM: cannot open " << filename << "\n"; return; }
    f << "P6\n" << W << " " << H << "\n255\n";
    for(int i = 0; i < W*H; i++) {
        float a = alphaBuf[i];               // no clamp — allow HDR alpha through
        float3 c = colorBuf[i] * float3(a);  // over black: color*alpha + black*(1-alpha)
        unsigned char rgb[3] = {
            (unsigned char)(255 * std::clamp(c.x, 0.0f, 1.0f)),
            (unsigned char)(255 * std::clamp(c.y, 0.0f, 1.0f)),
            (unsigned char)(255 * std::clamp(c.z, 0.0f, 1.0f))
        };
        f.write((char*)rgb, 3);
    }
}

int main(int argc, const char * argv[]) {
    int WIDTH = 500, HEIGHT = 500;
    gaussian firstGaussian(float2(250, 250), 100, float3(1.0, 0.05, 0.05), 0.5);
    auto [colorBuf, alphaBuf] = renderGaussian(firstGaussian, WIDTH, HEIGHT);
    writePPM(colorBuf, alphaBuf, WIDTH, HEIGHT, "gaussian.ppm");
    std::cout << "Current working directory: "
              << std::filesystem::current_path() << "\n";
    cout << "image written";
    return 0;
}


/* Reference: Triangle - Raterization:
 for (int y = 0; y < height; ++y) {
     for (int x = 0; x < width; ++x) {
         // 1. Compute barycentrics for pixel (x + 0.5, y + 0.5)
         float alpha, beta, gamma;
         computeBarycentrics(A, B, C, x + 0.5f, y + 0.5f, alpha, beta, gamma);

         // 2. Check inside triangle
         if (alpha >= 0 && beta >= 0 && gamma >= 0) {
             // 3. Interpolate attributes
             vec3 color = alpha * colorA + beta * colorB + gamma * colorC;
             float depth = alpha * zA + beta * zB + gamma * zC;

             // 4. Draw
             framebuffer[y][x] = color;
             zbuffer[y][x] = depth;
         }
     }
 }
*/
