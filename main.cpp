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
    float2 mu;     // center (x,y)
    float  sigma;  // spread
    float3 color;  // RGB
    float  alpha;  // opacity weight

    gaussian(float2 mu, float sigma, float3 color, float alpha)
        : mu(mu), sigma(sigma), color(color), alpha(alpha) {};
};

float squaredDistance(float2 a) {
    return (a.x*a.x) + (a.y*a.y);
}

// Sampling a Gaussian — returns straight color (no alpha baked in)
void splat(const gaussian& gauss, int x, int y, float3& pColor, float& pAlpha) {
    float2 pixelCenter = float2(x+0.5f, y+0.5f);
    float2 offset      = pixelCenter - gauss.mu;
    float  r2          = squaredDistance(offset);
    float  fallOff     = expf(-0.5f * r2 / (gauss.sigma * gauss.sigma));

    pAlpha = gauss.alpha * fallOff;
    pColor = gauss.color;  // straight color — compositing happens at write time
}

// ── Single Gaussian — over-compositing model ──────────────────────────────────
// Returns straight colour and alpha buffers separately for explicit compositing.
pair<vector<float3>, vector<float>> renderGaussian(const gaussian& gauss, int W, int H) {
    vector<float3> accumRGB(W*H, float3(0));
    vector<float>  accumAlpha(W*H, 0.0f);
    float3 pixelColor;
    float  pixelAlpha;
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            splat(gauss, i, j, pixelColor, pixelAlpha);
            accumRGB[j*W + i]   = pixelColor;
            accumAlpha[j*W + i] = pixelAlpha;
        }
    return {accumRGB, accumAlpha};
}

// ── Multiple Gaussians — additive compositing model ───────────────────────────
// Each Gaussian contributes color*alpha to the buffer additively.
// Overlapping Gaussians sum their contributions; HDR values are resolved by
// clamping to [0,1] at write time. This produces natural secondary-colour
// mixing: R+G→yellow, R+B→magenta, G+B→cyan, R+G+B→white.
vector<float3> renderGaussians(const vector<gaussian>& gaussians, int W, int H) {
    vector<float3> accumRGB(W*H, float3(0));
    float3 pColor;
    float  pAlpha;
    for (const auto& g : gaussians)
        for (int j = 0; j < H; j++)
            for (int i = 0; i < W; i++) {
                splat(g, i, j, pColor, pAlpha);
                accumRGB[j*W + i] += pColor * float3(pAlpha);  // additive contribution
            }
    return accumRGB;
}

// ── writePPM overloads ────────────────────────────────────────────────────────

// Over-compositing path: composites straight colour against black at write time
void writePPM(const vector<float3>& colorBuf, const vector<float>& alphaBuf,
              int W, int H, const string& filename)
{
    ofstream f(filename, ios::binary);
    if (!f) { cerr << "writePPM: cannot open " << filename << "\n"; return; }
    f << "P6\n" << W << " " << H << "\n255\n";
    for (int i = 0; i < W*H; i++) {
        float a  = alphaBuf[i];               // HDR alpha — no clamp before multiply
        float3 c = colorBuf[i] * float3(a);   // over black: color*alpha + black*(1-alpha)
        unsigned char rgb[3] = {
            (unsigned char)(255 * std::clamp(c.x, 0.0f, 1.0f)),
            (unsigned char)(255 * std::clamp(c.y, 0.0f, 1.0f)),
            (unsigned char)(255 * std::clamp(c.z, 0.0f, 1.0f))
        };
        f.write((char*)rgb, 3);
    }
}

// Additive path: HDR accumulated buffer is clamped directly to display range
void writePPM(const vector<float3>& imgBuf, int W, int H, const string& filename)
{
    ofstream f(filename, ios::binary);
    if (!f) { cerr << "writePPM: cannot open " << filename << "\n"; return; }
    f << "P6\n" << W << " " << H << "\n255\n";
    for (const auto& c : imgBuf) {
        unsigned char rgb[3] = {
            (unsigned char)(255 * std::clamp(c.x, 0.0f, 1.0f)),
            (unsigned char)(255 * std::clamp(c.y, 0.0f, 1.0f)),
            (unsigned char)(255 * std::clamp(c.z, 0.0f, 1.0f))
        };
        f.write((char*)rgb, 3);
    }
}

int main(int argc, const char * argv[]) {
    // ── Single Gaussian test (over-compositing model) ─────────────────────────
    {
        int W = 900, H = 506;
        gaussian g(float2(W/2.f, H/2.f), 120, float3(1.0f, 0.05f, 0.05f), 25.0f);
        auto [colorBuf, alphaBuf] = renderGaussian(g, W, H);
        writePPM(colorBuf, alphaBuf, W, H, "gaussian.ppm");
    }

    // ── Three Gaussians — RGB additive scene ─────────────────────────────────
    // Equilateral triangle (d=180, σ=50, α=18) on 900×506.
    // All centres ≥ 3.4σ from every edge — no boundary clipping.
    // Centres show primary colours; midpoints show secondary colours; centroid white.
    {
        int W = 900, H = 506;
        vector<gaussian> scene = {
            { float2(360, 180), 50, float3(1.00f, 0.02f, 0.02f), 18.0f },  // red
            { float2(540, 180), 50, float3(0.02f, 1.00f, 0.02f), 18.0f },  // green
            { float2(450, 336), 50, float3(0.02f, 0.02f, 1.00f), 18.0f },  // blue
        };
        auto img = renderGaussians(scene, W, H);
        writePPM(img, W, H, "gaussians_rgb.ppm");
    }

    std::cout << "Current working directory: " << std::filesystem::current_path() << "\n";
    cout << "images written\n";
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
