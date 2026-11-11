# Native GNU Make entry point (Windows). PowerShell build is the authoritative pipeline.
PROFILE ?= Release
POWERSHELL ?= powershell.exe
.PHONY: all development release package
all:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Profile $(PROFILE)
development:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Profile Development
release:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Profile Release
package:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File tools/package-submission.ps1
