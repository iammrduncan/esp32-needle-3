PYTHON ?= python3
VENV_PY := .venv/bin/python
FLASH_PORT ?= /dev/ttyACM0
SERIAL_PORT ?= /dev/ttyACM1

.PHONY: setup model verify-model build flash flash-app serve install-service host test capture demo
setup:
	$(PYTHON) -m venv .venv || $(PYTHON) -m virtualenv .venv
	$(VENV_PY) -m pip install -r requirements.txt
model:
	$(PYTHON) tools/download_model.py
verify-model:
	$(PYTHON) tools/download_model.py --verify-only
build:
	idf.py -C esp32 build
flash: build verify-model
	idf.py -C esp32 -p $(FLASH_PORT) flash
	$(PYTHON) -m esptool --chip esp32s3 --port $(FLASH_PORT) --baud 921600 write_flash 0x210000 model/needle3.cact
flash-app: build
	idf.py -C esp32 -p $(FLASH_PORT) flash
serve:
	$(VENV_PY) tools/serial_api.py --serial $(SERIAL_PORT)
install-service:
	$(VENV_PY) tools/install_service.py --serial $(SERIAL_PORT)
host:
	cmake -S host -B host/build
	cmake --build host/build
test: host
	$(VENV_PY) -m unittest discover -s tests -v
	ctest --test-dir host/build --output-on-failure
capture:
	$(VENV_PY) demo/capture.py
demo:
	$(PYTHON) demo/render.py
