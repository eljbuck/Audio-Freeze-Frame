# Audio-Freeze-Frame

## About

What would it mean to freeze frame audio in a meaningful way? Given that our ears are spectral analyzers, a 'meaningful' auditory freeze frame would probably preserve the harmonic content of some audio at a moment in time. This demands some notion of instantaneous frequency, but regretably, frequency is explicitly defined across time (this hang-up is probably why such a popular concept for video has not been extended to audio). Still, I think we all can imagine what an audio freeze frame might do for various sound sources, which suggests that such a tool is possible.

## Motivations

Before getting too deep, why on Earth might we be interested in freeze framing audio? This is not immediately obvious, but here are two reasons that are musically relevant to me.

1. A tool in aiding music transcription
- Solo transcription is a very important aspect of learning jazz vocabulary. Though jazz is not an incredibly old genre, its conception certainly predates modern recording technology and thus older records tend to have lower quality. As such, it can sometimes be difficult to hear a note that a soloist plays, especially when they are playing fast. A tool that can freeze audio while preserving its harmonic content would be very helpful in identifying these notes.

2. A tool for live performance
- DJs have popularized another kind of ‘freezing’ in the form of record scratching. This is applied to pre-existing recordings (typically on vinyls) but happens in a live context as a performance technique. Now, imagine, if you were performing live and you could freeze a moment of that performance. A related example would be [Jacob Collier’s harmonizer synth](https://youtu.be/PHkKMJ6DCZQ?si=lfv_GjRV-hZ36FMl&t=41) which allows live (vocal) input and then freezes that sound. This applied more broadly to any incoming signal could be an interesting tool for live performance.

## Goal

Create a seamless, artifact-free loop that preserves the frequency content of a short window around when the user froze.
