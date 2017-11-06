
/*
    pbrt source code is Copyright(c) 1998-2016
                        Matt Pharr, Greg Humphreys, and Wenzel Jakob.

    This file is part of pbrt.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */


// materials/translucent.cpp*
#include <pbrt/materials/translucent.h>

#include <pbrt/core/spectrum.h>
#include <pbrt/core/microfacet.h>
#include <pbrt/util/memory.h>
#include <pbrt/core/reflection.h>
#include <pbrt/core/paramset.h>
#include <pbrt/core/texture.h>
#include <pbrt/core/interaction.h>

namespace pbrt {

// TranslucentMaterial Method Definitions
void TranslucentMaterial::ComputeScatteringFunctions(
    SurfaceInteraction *si, MemoryArena &arena, TransportMode mode) const {
    // Perform bump mapping with _bumpMap_, if present
    if (bumpMap) Bump(*bumpMap, si);
    Float eta = 1.5f;
    si->bsdf = arena.Alloc<BSDF>(*si, eta);

    Spectrum r = reflect->Evaluate(*si).Clamp();
    Spectrum t = transmit->Evaluate(*si).Clamp();
    if (!r && !t) return;

    Spectrum kd = Kd->Evaluate(*si).Clamp();
    if (kd) {
        if (r)
            si->bsdf->Add(arena.Alloc<LambertianReflection>(r * kd));
        if (t)
            si->bsdf->Add(arena.Alloc<LambertianTransmission>(t * kd));
    }
    Spectrum ks = Ks->Evaluate(*si).Clamp();
    if (ks) {
        Float rough = roughness->Evaluate(*si);
        if (remapRoughness)
            rough = TrowbridgeReitzDistribution::RoughnessToAlpha(rough);
        MicrofacetDistribution *distrib =
            arena.Alloc<TrowbridgeReitzDistribution>(rough, rough);
        if (r) {
            Fresnel *fresnel = arena.Alloc<FresnelDielectric>(1.f, eta);
            si->bsdf->Add(arena.Alloc<MicrofacetReflection>(
                r * ks, distrib, fresnel));
        }
        if (t)
            si->bsdf->Add(arena.Alloc<MicrofacetTransmission>(
                t * ks, distrib, 1.f, eta, mode));
    }
}

std::shared_ptr<TranslucentMaterial> CreateTranslucentMaterial(
    const TextureParams &mp, const std::shared_ptr<const ParamSet> &attributes) {
    std::shared_ptr<Texture<Spectrum>> Kd =
        mp.GetSpectrumTexture("Kd", Spectrum(0.25f));
    std::shared_ptr<Texture<Spectrum>> Ks =
        mp.GetSpectrumTexture("Ks", Spectrum(0.25f));
    std::shared_ptr<Texture<Spectrum>> reflect =
        mp.GetSpectrumTexture("reflect", Spectrum(0.5f));
    std::shared_ptr<Texture<Spectrum>> transmit =
        mp.GetSpectrumTexture("transmit", Spectrum(0.5f));
    std::shared_ptr<Texture<Float>> roughness =
        mp.GetFloatTexture("roughness", .1f);
    std::shared_ptr<Texture<Float>> bumpMap =
        mp.GetFloatTextureOrNull("bumpmap");
    bool remapRoughness = mp.GetOneBool("remaproughness", true);
    return std::make_shared<TranslucentMaterial>(
        Kd, Ks, roughness, reflect, transmit, bumpMap, remapRoughness, attributes);
}

}  // namespace pbrt