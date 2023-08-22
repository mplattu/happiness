build:
	pio run --environment mkrzero

upload:
	pio run --environment mkrzero -t upload

monitor:
	pio device monitor

clean:
	rm -fR .pio/
