#pragma once

#include <JuceHeader.h>
#include <cmath>

//==============================================================================
//  SKYGRIN - COURBES DE REPONSE DU POTARD
//
//  Le potard Intensity (0..1) ne pilote jamais un effet directement. Chaque
//  sous-effet lit Intensity a travers une Curve, definie par preset :
//
//      valeur = amount * forme( (t - start) / (1 - start) )
//
//   amount : valeur atteinte a 100 % (0 = module coupe)
//   start  : seuil d'entree (0.4 = le module dort jusqu'a 40 % du potard)
//   shape  : Power  -> x^k        (k > 1 arrive tard, k < 1 arrive tot)
//            SCurve -> x^k / (x^k + (1-x)^k)   (lent, bascule, plafonne)
//            Expo   -> (e^(kx) - 1) / (e^k - 1) (explose a la fin)
//
//  La sortie est une valeur normalisee 0..1 ; la conversion en unites
//  physiques (Hz, dB, ms) est faite a un seul endroit :
//  SkygrinAudioProcessor::mapToSettings().
//==============================================================================
namespace skygrin
{
    enum class Shape { Power, SCurve, Expo };

    struct Curve
    {
        float amount = 0.0f;
        float k      = 1.0f;
        float start  = 0.0f;
        Shape shape  = Shape::Power;
    };

    // Raccourcis d'ecriture pour les presets (constexpr : zero cout a l'execution)
    constexpr Curve pw (float amount, float k = 1.0f, float start = 0.0f) { return { amount, k, start, Shape::Power  }; }
    constexpr Curve sc (float amount, float k = 2.0f, float start = 0.0f) { return { amount, k, start, Shape::SCurve }; }
    constexpr Curve ex (float amount, float k = 3.0f, float start = 0.0f) { return { amount, k, start, Shape::Expo   }; }

    //--------------------------------------------------------------------------
    inline float shapeUnit (float x, float k, Shape shape) noexcept
    {
        x = juce::jlimit (0.0f, 1.0f, x);

        switch (shape)
        {
            case Shape::SCurve:
            {
                const float a = std::pow (x, k);
                const float b = std::pow (1.0f - x, k);
                return (a + b) > 0.0f ? a / (a + b) : 0.0f;
            }

            case Shape::Expo:
                return (k < 1.0e-3f) ? x : (std::exp (k * x) - 1.0f) / (std::exp (k) - 1.0f);

            case Shape::Power:
            default:
                return std::pow (x, k);
        }
    }

    inline float evaluate (const Curve& c, float t) noexcept
    {
        if (c.amount <= 0.0f)
            return 0.0f;

        const float x = (t - c.start) / juce::jmax (1.0e-3f, 1.0f - c.start);
        return (x <= 0.0f) ? 0.0f : c.amount * shapeUnit (x, c.k, c.shape);
    }

    //--------------------------------------------------------------------------
    //  Conversions vers les unites physiques
    //--------------------------------------------------------------------------

    // jmap en domaine logarithmique : l'oreille entend les frequences en octaves.
    // logMap (0.5, 20, 3200) = 253 Hz, et non 1610 Hz comme le ferait jmap lineaire.
    inline float logMap (float v01, float lo, float hi) noexcept
    {
        return lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, v01));
    }

    inline float dbToGain (float db) noexcept   { return juce::Decibels::decibelsToGain (db); }

    // Fondu sans clic : 0 tant que v < 0, 1 au-dela de width.
    inline float fadeIn (float v, float width) noexcept
    {
        const float x = juce::jlimit (0.0f, 1.0f, v / width);
        return x * x * (3.0f - 2.0f * x);
    }
}
