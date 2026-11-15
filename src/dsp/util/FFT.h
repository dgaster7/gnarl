#pragma once
// Real FFT wrapper over Accelerate vDSP. Create in prepare(), use anywhere (no allocation in perform).
#include <Accelerate/Accelerate.h>
#include <cmath>
#include <vector>

namespace gnarl {

class RealFFT {
public:
    RealFFT() = default;
    explicit RealFFT(int order) { init(order); }
    ~RealFFT() { if (setup) vDSP_destroy_fftsetup(setup); }
    RealFFT(const RealFFT&) = delete;
    RealFFT& operator=(const RealFFT&) = delete;
    void init(int order) {
        if (setup) vDSP_destroy_fftsetup(setup);
        log2n = order; n = 1 << order;
        setup = vDSP_create_fftsetup((vDSP_Length) order, kFFTRadix2);
        re.assign((size_t) n / 2, 0); im.assign((size_t) n / 2, 0);
    }
    int size() const { return n; }
    // Forward: time[n] -> re[0..n/2], im[0..n/2] (bin 0..n/2 inclusive; im[0]=im[n/2]=0). Unscaled (sum).
    void forward(const float* time, float* outRe, float* outIm) {
        DSPSplitComplex sc{re.data(), im.data()};
        vDSP_ctoz(reinterpret_cast<const DSPComplex*>(time), 2, &sc, 1, (vDSP_Length) n / 2);
        vDSP_fft_zrip(setup, &sc, 1, (vDSP_Length) log2n, kFFTDirection_Forward);
        // vDSP packs DC in re[0] and Nyquist in im[0], both scaled by 2.
        outRe[0] = re[0] * 0.5f; outIm[0] = 0;
        outRe[n / 2] = im[0] * 0.5f; outIm[n / 2] = 0;
        for (int k = 1; k < n / 2; ++k) { outRe[k] = re[(size_t) k] * 0.5f; outIm[k] = im[(size_t) k] * 0.5f; }
    }
    // Inverse of forward(): scaled so inverse(forward(x)) == x.
    void inverse(const float* inRe, const float* inIm, float* time) {
        re[0] = inRe[0] * 2; im[0] = inRe[n / 2] * 2;
        for (int k = 1; k < n / 2; ++k) { re[(size_t) k] = inRe[k] * 2; im[(size_t) k] = inIm[k] * 2; }
        DSPSplitComplex sc{re.data(), im.data()};
        vDSP_fft_zrip(setup, &sc, 1, (vDSP_Length) log2n, kFFTDirection_Inverse);
        vDSP_ztoc(&sc, 1, reinterpret_cast<DSPComplex*>(time), 2, (vDSP_Length) n / 2);
        float scale = 1.0f / (float) n;
        vDSP_vsmul(time, 1, &scale, time, 1, (vDSP_Length) n);
        // vDSP inverse gives 2*n*x for the doubled input: we doubled, so result is n*x*... normalised above.
        float half = 0.5f;
        vDSP_vsmul(time, 1, &half, time, 1, (vDSP_Length) n);
    }
private:
    FFTSetup setup = nullptr;
    int log2n = 0, n = 0;
    std::vector<float> re, im;
};

} // namespace gnarl
