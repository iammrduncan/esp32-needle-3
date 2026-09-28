# Entry points follow the inference-engines native engine format: each target is
# non-interactive, safe to rerun, and takes its inputs from IE_* variables with
# the defaults below. scripts/ie-step.sh runs a target on the host or inside the
# ESP-IDF container, whichever this machine supports.
PYTHON ?= python3
VENV_PY := .venv/bin/python
FLASH_PORT ?= $(or $(IE_PORT_FLASH),/dev/ttyACM0)
SERIAL_PORT ?= $(or $(IE_PORT_SERIAL),/dev/ttyACM1)
HTTP_HOST ?= 127.0.0.1
HTTP_PORT ?= $(or $(IE_HTTP_PORT),8081)
LAYERS ?= $(or $(IE_PARAM_LAYERS),8)
BAUD ?= 921600
IDF_IMAGE ?= docker.io/espressif/idf:v5.5.2
CONTAINER ?= $(firstword $(shell command -v podman docker 2>/dev/null))
ESPTOOL ?= $(if $(shell command -v esptool.py 2>/dev/null),esptool.py,$(PYTHON) -m esptool)

.PHONY: check setup model verify-model build flash flash-app serve health host test install-service capture demo

check:
	@PYTHON="$(PYTHON)" CONTAINER="$(CONTAINER)" IDF_IMAGE="$(IDF_IMAGE)" \
		FLASH_PORT="$(FLASH_PORT)" SERIAL_PORT="$(SERIAL_PORT)" sh scripts/check.sh
setup:
	$(PYTHON) -m venv .venv || $(PYTHON) -m virtualenv .venv
	$(VENV_PY) -m pip install -q -r requirements.txt
model:
	$(PYTHON) tools/download_model.py --layers $(LAYERS)
verify-model:
	$(PYTHON) tools/download_model.py --layers $(LAYERS) --verify-only
# Builds with idf.py when ESP-IDF is active; otherwise inside $(IDF_IMAGE), with
# the checkout mounted at the same path so `git describe` resolves.
build:
ifneq ($(shell command -v idf.py 2>/dev/null),)
	idf.py -C esp32 build
else
	@test -n "$(CONTAINER)" || { echo "idf.py not found and no podman/docker for $(IDF_IMAGE)"; exit 1; }
	$(CONTAINER) run --rm -v "$(CURDIR):$(CURDIR)" -w "$(CURDIR)" $(IDF_IMAGE) idf.py -C esp32 build
endif
# Erases and writes the bootloader, partition table, app and the model at 0x210000.
flash: build verify-model
	cd esp32/build && $(ESPTOOL) --chip esp32s3 --port $(FLASH_PORT) --baud $(BAUD) \
		--before default_reset --after hard_reset write_flash @flash_args 0x210000 ../../model/needle3.cact
flash-app: build
	cd esp32/build && $(ESPTOOL) --chip esp32s3 --port $(FLASH_PORT) --baud $(BAUD) \
		--before default_reset --after hard_reset write_flash @flash_args
# Foreground bridge; stop it with SIGTERM or Ctrl-C.
serve:
	PYTHONUNBUFFERED=1 $(VENV_PY) tools/serial_api.py --serial $(SERIAL_PORT) --host $(HTTP_HOST) --port $(HTTP_PORT)
health:
	curl -fsS http://$(HTTP_HOST):$(HTTP_PORT)/health
host:
	cmake -S host -B host/build
	cmake --build host/build
test: host
	$(VENV_PY) -m unittest discover -s tests -v
	ctest --test-dir host/build --output-on-failure
# Optional: run the bridge as a systemd user service. Not part of the launch path.
install-service:
	$(VENV_PY) tools/install_service.py --serial $(SERIAL_PORT)
capture:
	$(VENV_PY) demo/capture.py
demo:
	$(PYTHON) demo/render.py
