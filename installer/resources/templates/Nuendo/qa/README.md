# Nuendo Template QA

Scripts for verifying the Mach1 Spatial Template works correctly in Nuendo.

## Quick Start

```bash
# Check plugins and template are installed
./test_nuendo_session.sh --check

# Launch Nuendo with the template for manual testing
./test_nuendo_session.sh --launch
```

## Validate Rendered Audio

After exporting from Nuendo, validate the output:

```bash
python3 validate_audio.py /path/to/render.wav
```

## Manual QA Checklist

After launching with `--launch`:

- [ ] Template loads without errors
- [ ] All tracks visible and properly named
- [ ] M1-Panner plugins load on source tracks
- [ ] M1-Monitor plugin loads on master bus
- [ ] Panning controls respond correctly
- [ ] Export produces valid 8-channel output
