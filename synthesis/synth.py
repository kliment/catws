#/usr/bin/python3

import os
import sys
import random
import scipy.signal
import struct
import math

# Gets all files in a folder and resamples them to have 128 samples
# Files should have about the same length to sound realistic later
def all_file_contents(dir):
    ret = []
    for f in os.listdir(dir):
        contents = open(dir+'/'+f, "rb").read()
        floats = [ x[0] for x in struct.iter_unpack("f", contents) ]
        minimized = scipy.signal.resample(floats, 128)
        ret.append([ int(x * 127) for x in list(minimized) ])
    return ret

inhale = all_file_contents('inhale')
exhale = all_file_contents('exhale')

output_freq = 48000
inhale_snd_freq = 28
exhale_snd_freq = 30

# Phase increment per sample for some frequency
def increment(snd_freq):
    return int(2**24 / output_freq * snd_freq)

# Fade in, using the logistic function
def fade_in(num):
    return [ int(255 * (1 - 1/(1 + math.exp(-6.9 + x/num * 2 * 6.9)))) for x in range(num) ]

def fade_out(num):
    ret = fade_in(num)
    ret.reverse()
    return ret

# Plays random waveforms from an array
# arr: array of waveforms
# freq: Playback frequency
# times: How many waveforms to play
# mul: An array used as multiplier. Length should be `times`. Range [0-255].
def play(arr, freq, times, mul):
    phase = 0
    inc = increment(freq)
    choice = random.randrange(len(arr))

    ret = []

    iter = 0
    while iter < times:
        ret.append((arr[choice][int(phase/2**17)] * mul[iter]) // 256)
        phase += inc
        while phase > 2**24:
            phase -= 2**24
            iter += 1
            choice = random.randrange(len(arr))
    return ret

# Synthesize 10 breaths into out.raw
with open("out.raw", "wb") as out:
    w = lambda x: out.write(struct.pack("b"*len(x), *x))
    for iter in range(10):
        w(play(inhale, inhale_snd_freq, 45, fade_in(10) + [255] * 25 + fade_out(10)))
        w(play([[0]*128], 30, 5, [1]*5))
        w(play(exhale, exhale_snd_freq, 60, fade_in(10) + [255] * 40 + fade_out(10)))
        w(play([[0]*128], 30, 5, [1]*5))
