#pragma once
#include <cmath>
#include <complex>
#include <algorithm>
#include "Params.h"

// ============================================================================
//  Velours — réponse des bandes de l'éditeur de sensibilité
//  (prototypes analogiques : pas de déformation près de Nyquist, identiques
//   quelle que soit la fréquence d'échantillonnage)
// ============================================================================
namespace bands
{
struct Band
{
    bool on = false;
    bool bypass = false;      // bande présente mais sans effet (double-clic sur le nœud)
    int type = params::bell;
    float freq = 1000.0f, gain = 0.0f, q = 1.0f;
    int focus = params::focusAll;

    bool operator== (const Band& o) const
    {
        return on == o.on && bypass == o.bypass && type == o.type && juce::exactlyEqual (freq, o.freq) && juce::exactlyEqual (gain, o.gain)
            && juce::exactlyEqual (q, o.q) && focus == o.focus;
    }
    bool operator!= (const Band& o) const { return ! (*this == o); }
};

// Réponse en dB d'une bande à la fréquence f
inline float responseDb (const Band& b, float f)
{
    using C = std::complex<double>;
    const double q = std::max (0.05, (double) b.q);
    const C s (0.0, std::max (1.0e-6, (double) f) / std::max (1.0, (double) b.freq));
    const C s2 = s * s;
    auto mag2db = [] (C h) { return (float) (20.0 * std::log10 (std::max (1.0e-12, std::abs (h)))); };

    switch (b.type)
    {
        case params::bell:
        {
            const double A = std::pow (10.0, b.gain / 40.0);
            return mag2db ((s2 + s * (A / q) + 1.0) / (s2 + s / (A * q) + 1.0));
        }
        case params::lowShelf:
        {
            const double A = std::pow (10.0, b.gain / 40.0), sq = std::sqrt (A) / q;
            return mag2db (A * (s2 + sq * s + A) / (A * s2 + sq * s + 1.0));
        }
        case params::highShelf:
        {
            const double A = std::pow (10.0, b.gain / 40.0), sq = std::sqrt (A) / q;
            return mag2db (A * (A * s2 + sq * s + 1.0) / (s2 + sq * s + A));
        }
        case params::lowCut:   // 24 dB/oct
            return std::max (-120.0f, 2.0f * mag2db (s2 / (s2 + s / q + 1.0)));
        case params::highCut:  // 24 dB/oct
            return std::max (-120.0f, 2.0f * mag2db (1.0 / (s2 + s / q + 1.0)));
        case params::bandPass:
            return std::max (-120.0f, 2.0f * mag2db ((s / q) / (s2 + s / q + 1.0)) + b.gain);
        case params::bandReject:  // zone où le traitement est retiré
            return std::max (-120.0f, 2.0f * mag2db ((s2 + 1.0) / (s2 + s / q + 1.0)));
        case params::tilt:
        {
            const double oct = std::log2 (std::max (1.0e-6, (double) f) / std::max (1.0, (double) b.freq));
            return (float) (b.gain * std::tanh (oct * q * 0.5));
        }
        default: break;
    }
    return 0.0f;
}

// Zone entendue en BAND LISTEN (0..1) : la région de fréquences que la bande « vise »
inline float listenRegion (const Band& b, float f)
{
    using C = std::complex<double>;
    const double q = std::max (0.05, (double) b.q);
    const C s (0.0, std::max (1.0e-6, (double) f) / std::max (1.0, (double) b.freq));
    const C s2 = s * s;
    switch (b.type)
    {
        case params::bell:
        case params::bandPass:
        case params::bandReject:
            return (float) std::norm ((s / q) / (s2 + s / q + 1.0)) ;
        case params::lowShelf:
        case params::highCut:
            return (float) std::norm (1.0 / (s2 + s * 1.41421356 + 1.0));
        case params::highShelf:
        case params::lowCut:
            return (float) std::norm (s2 / (s2 + s * 1.41421356 + 1.0));
        default: break;
    }
    return 1.0f;
}

// La bande agit-elle sur ce canal de traitement ?
//   channel 0/1 = L/R en mode L/R, M/S en mode M/S ; numChannels = 1 -> mono
inline bool appliesTo (const Band& b, int channel, int numChannels, bool midSide)
{
    if (numChannels < 2 || b.focus == params::focusAll) return true;
    if (! midSide)
    {
        if (b.focus == params::focusLeft)  return channel == 0;
        if (b.focus == params::focusRight) return channel == 1;
        return true;   // Mid / Side en mode L/R : agit sur les deux
    }
    if (b.focus == params::focusMid)  return channel == 0;
    if (b.focus == params::focusSide) return channel == 1;
    return true;       // Left / Right en mode M/S : agit sur les deux
}

// Somme des bandes actives (dB) pour un canal
inline float totalDb (const Band* all, int n, float f, int channel, int numChannels, bool midSide)
{
    float db = 0.0f;
    for (int i = 0; i < n; ++i)
        if (all[i].on && ! all[i].bypass && appliesTo (all[i], channel, numChannels, midSide))
            db += responseDb (all[i], f);
    return db;
}

// Multiplicateur de profondeur : +12 dB = traitement x4, -inf = aucun traitement
inline float weightFromDb (float db)
{
    return std::clamp (std::pow (10.0f, db / 20.0f), 0.0f, 4.0f);
}
} // namespace bands
