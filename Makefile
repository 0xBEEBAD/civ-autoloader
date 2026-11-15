.PHONY: build sd clean release

build:
	cd packages/firmware/esp32 && python3 tools/build_and_flash.py --keys prod.keys

sd:
	python3 packages/sdcard/stage.py

clean:
	rm -rf packages/firmware/esp32/build
	rm -rf packages/firmware/esp32/blobs
	rm -rf packages/sdcard/contents

release: build sd
	cd packages/firmware/esp32 && shasum -a 256 blobs/*.bin build/esp32-ldn-civ7.bin > SHA256SUMS
