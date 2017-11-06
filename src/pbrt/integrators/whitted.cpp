
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


// integrators/whitted.cpp*
#include <pbrt/integrators/whitted.h>

#include <pbrt/core/camera.h>
#include <pbrt/core/error.h>
#include <pbrt/core/film.h>
#include <pbrt/core/interaction.h>
#include <pbrt/core/paramset.h>
#include <pbrt/core/reflection.h>
#include <pbrt/core/sampler.h>

namespace pbrt {

// WhittedIntegrator Method Definitions
Spectrum WhittedIntegrator::Li(const RayDifferential &ray, Sampler &sampler,
                               MemoryArena &arena, int depth) const {
    Spectrum L(0.);
    // Find closest ray intersection or return background radiance
    SurfaceInteraction isect;
    if (!scene.Intersect(ray, &isect)) {
        for (const auto &light : scene.lights) L += light->Le(ray);
        return L;
    }

    // Compute emitted and reflected light at ray intersection point

    // Initialize common variables for Whitted integrator
    const Normal3f &n = isect.shading.n;
    Vector3f wo = isect.wo;

    // Compute scattering functions for surface interaction
    isect.ComputeScatteringFunctions(ray, arena);
    if (!isect.bsdf)
        return Li(isect.SpawnRay(ray.d), sampler, arena, depth);

    // Compute emitted light if ray hit an area light source
    L += isect.Le(wo);

    // Add contribution of each light source
    for (const auto &light : scene.lights) {
        Vector3f wi;
        Float pdf;
        VisibilityTester visibility;
        Spectrum Li =
            light->Sample_Li(isect, sampler.Get2D(), &wi, &pdf, &visibility);
        if (!Li || pdf == 0) continue;
        Spectrum f = isect.bsdf->f(wo, wi);
        if (f && visibility.Unoccluded(scene))
            L += f * Li * AbsDot(wi, n) / pdf;
    }
    if (depth + 1 < maxDepth) {
        // Trace rays for specular reflection and refraction
        L += SpecularReflect(ray, isect, sampler, arena, depth);
        L += SpecularTransmit(ray, isect, sampler, arena, depth);
    }
    return L;
}

std::unique_ptr<WhittedIntegrator> CreateWhittedIntegrator(
    const ParamSet &params, const Scene &scene,
    std::shared_ptr<const Camera> camera, std::unique_ptr<Sampler> sampler) {
    int maxDepth = params.GetOneInt("maxdepth", 5);
    absl::Span<const int> pb = params.GetIntArray("pixelbounds");
    Bounds2i pixelBounds = camera->film->GetSampleBounds();
    if (!pb.empty()) {
        if (pb.size() != 4)
            Error("Expected four values for \"pixelbounds\" parameter. Got %d.",
                  (int)pb.size());
        else {
            pixelBounds = Intersect(pixelBounds,
                                    Bounds2i{{pb[0], pb[2]}, {pb[1], pb[3]}});
            if (pixelBounds.Empty())
                Error("Degenerate \"pixelbounds\" specified.");
        }
    }
    return std::make_unique<WhittedIntegrator>(maxDepth, scene, camera,
                                               std::move(sampler), pixelBounds);
}

}  // namespace pbrt