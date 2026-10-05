# Native SPC timer arithmetic

The Original 3DS ARM11 CPU lacks an integer division instruction. The pinned
Shay Green `snes_spc` CPU uses runtime division for timer prescalers, including
the ordinary values 128 and 16. The native adapter specializes those values
as constant divisions, retaining C++ signed truncation, and skips timer-period
division when its quotient is zero. A period of 256 also uses constant division.
Other prescalers/periods keep the original division, including restored tempos.

Only the private native CPU source copy is adapted; upstream source and LGPL
notices are preserved. The pinned DSP, BRR interpolation, echo, voice count,
CPU instructions, timer state, tempo API, sound-bank uploads, 50 ms blocks and
independent music/SFX engines are unchanged. Desktop and VR targets are not
modified. A larger experimental DSP dispatcher was rejected for lack of a
convincing speed improvement; the native player retains the original DSP loop.

The arithmetic test compares 4,330,526 quotients and timer carry/counter states
with the original expressions. The existing independent synthetic state/PCM
trace remains `e7cddb7195a79cc2`. A private, separate baseline library comparison
also checks Original/EX setup, Corneria and asteroids, normal controls/shooting,
the Original 3-5 selection and full source/SPC restore. Both executables produce
the same 582,699,066-byte source/SPC/PCM stream (SHA-256
`1939fd2a04bd04ab7634586074315c8e6da6cb1026c2deeb9ad2ab444cfe563e`).
Private cartridge data and comparison streams are not distributed.

CI cross-builds the native player and inspects the actual ARM timer function
against the unmodified pinned CPU compiled with the same flags. This verifies
normal-prescaler shift paths and the original generic divide fallback. It does
not establish physical-console frame rate or the complete audio speedup.
