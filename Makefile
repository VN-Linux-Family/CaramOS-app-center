SHELL := /usr/bin/env bash
.SHELLFLAGS := -euo pipefail -c

PACKAGE := caramos-app-center
PPA ?= ppa:vietnamlinuxfamily/caram-os
GPG_KEY ?= ledungb11@gmail.com
GPG_TTY ?= $(shell tty 2>/dev/null || true)
export GPG_TTY
VERSION ?=

PY := PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=usr/lib/python3/dist-packages python3
PY_SCRIPTS := usr/bin/caramos-app-center usr/lib/caramos-app-center/caramos-app-center-helper \
	usr/share/caramos-app-center/recipes/zalo-full
UI_SOURCES := $(sort $(wildcard usr/lib/python3/dist-packages/caramos_app_center/ui/*.py))
DOMAIN := caramos-app-center
CURRENT_VERSION = $(shell dpkg-parsechangelog -S Version)

.PHONY: help compile test pot build source changelog check-release gpg-unlock upload release run clean

help:
	@echo 'CaramOS App Center'
	@echo '  make test                 Unit tests'
	@echo '  make run                  Run the UI from the source tree with a simulated helper'
	@echo '  make build                Build the .deb into dist/'
	@echo '  make pot                  Refresh the translation template and merge it into vi.po'
	@echo '  make changelog VERSION=x  Open a debian/changelog entry for version x (edit it after)'
	@echo '  make release VERSION=x    Test, build, sign and upload version x to $(PPA)'

# Python byte-compile check and translation catalogs (run by debian/rules too).
compile:
	PYTHONPYCACHEPREFIX="$$(mktemp -d)" python3 -m py_compile $(PY_SCRIPTS) \
		$$(find usr/lib/python3/dist-packages -name '*.py' | sort)
	rm -rf usr/share/locale
	for po in po/$(DOMAIN)/*.po; do \
		lang="$$(basename "$$po" .po)"; \
		mkdir -p "usr/share/locale/$$lang/LC_MESSAGES"; \
		msgfmt --check --output-file="usr/share/locale/$$lang/LC_MESSAGES/$(DOMAIN).mo" "$$po"; \
	done
	@echo '[OK] compiled'

test:
	$(PY) -m unittest discover -s tests -v

pot:
	xgettext --language=Python --from-code=UTF-8 --keyword=_ --keyword=ngettext:1,2 \
		--package-name=$(DOMAIN) --msgid-bugs-address=developer@vietnamlinuxfamily.net \
		-o po/$(DOMAIN)/$(DOMAIN).pot $(UI_SOURCES)
	msgmerge --quiet --update --backup=none --no-fuzzy-matching po/$(DOMAIN)/vi.po po/$(DOMAIN)/$(DOMAIN).pot
	msgattrib --no-obsolete -o po/$(DOMAIN)/vi.po po/$(DOMAIN)/vi.po
	msgattrib --untranslated po/$(DOMAIN)/vi.po | grep -E '^msgid ' || echo '[OK] every message is translated'

build: clean compile
	dpkg-buildpackage -us -uc -b
	mkdir -p dist
	rm -f dist/$(PACKAGE)_*.deb
	cp -f ../$(PACKAGE)_$(CURRENT_VERSION)_all.deb dist/
	@echo "[OK] dist/$(PACKAGE)_$(CURRENT_VERSION)_all.deb"

source: clean
	dpkg-buildpackage -S -sa -us -uc
	@echo "[OK] ../$(PACKAGE)_$(CURRENT_VERSION)_source.changes"

changelog:
	@test -n '$(VERSION)' || { echo 'Usage: make changelog VERSION=0.2.0' >&2; exit 2; }
	./tools/new-changelog-entry.py '$(VERSION)'

check-release:
	@test -n '$(VERSION)' || { echo 'Usage: make release VERSION=0.2.0' >&2; exit 2; }
	@test '$(CURRENT_VERSION)' = '$(VERSION)' || { echo "debian/changelog is at $(CURRENT_VERSION), not $(VERSION). Run: make changelog VERSION=$(VERSION)" >&2; exit 1; }
	@! grep -q 'Describe the changes' debian/changelog || { echo 'Write the changes into debian/changelog first.' >&2; exit 1; }

gpg-unlock:
	@printf 'CaramOS App Center release signing\n' | gpg --pinentry-mode error --clearsign -u '$(GPG_KEY)' >/dev/null 2>&1 \
		|| { echo 'Enter the GPG passphrase for $(GPG_KEY):'; \
		     printf 'CaramOS App Center release signing\n' | gpg --pinentry-mode loopback --clearsign -u '$(GPG_KEY)' >/dev/null; }

upload:
	changes='../$(PACKAGE)_$(CURRENT_VERSION)_source.changes'; \
	gpg --verify "$$changes" >/dev/null 2>&1 || debsign -k'$(GPG_KEY)' "$$changes"; \
	dput '$(PPA)' "$$changes"

release: check-release test build source gpg-unlock upload

run:
	rm -rf .run-locale && mkdir -p .run-locale/vi/LC_MESSAGES
	msgfmt -o .run-locale/vi/LC_MESSAGES/$(DOMAIN).mo po/$(DOMAIN)/vi.po
	CARAMOS_APP_CENTER_DATA="$$PWD/usr/share/caramos-app-center" CARAMOS_APP_CENTER_FAKE_HELPER=1 \
		CARAMOS_APP_CENTER_LOCALEDIR="$$PWD/.run-locale" $(PY) usr/bin/caramos-app-center --wizard

clean:
	rm -rf dist usr/share/locale .run-locale debian/.debhelper debian/$(PACKAGE) debian/debhelper-build-stamp debian/files
	find debian -maxdepth 1 \( -name '*.substvars' -o -name '*.debhelper.log' \) -delete
	find . -name __pycache__ -type d -prune -exec rm -rf {} +
