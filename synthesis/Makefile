out.wav: out.raw
	sox -r 48000 -c 1 -b 8 -e signed-integer out.raw out.wav

out.ogg: out.wav
	oggenc out.wav

out.raw: synth.py
	python synth.py

clean:
	rm -f out.raw out.wav out.ogg
