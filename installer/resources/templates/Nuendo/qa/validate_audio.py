#!/usr/bin/env python3
"""
Nuendo Audio Validation Script
Validates audio files rendered from Nuendo sessions using Mach1 Spatial plugins.

This script checks:
- File format and metadata
- Channel count (8 channels for Mach1 Spatial)
- Sample rate consistency
- Peak levels and DC offset
- Basic spectral analysis
"""

import argparse
import sys
import os
from pathlib import Path

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

try:
    import wave
    HAS_WAVE = True
except ImportError:
    HAS_WAVE = False

try:
    import soundfile as sf
    HAS_SOUNDFILE = True
except ImportError:
    HAS_SOUNDFILE = False


class AudioValidator:
    """Validates audio files for Mach1 Spatial compatibility."""
    
    EXPECTED_CHANNELS = 8  # Mach1 Spatial uses 8 channels
    EXPECTED_SAMPLE_RATES = [44100, 48000, 88200, 96000]
    MAX_DC_OFFSET = 0.01  # Maximum acceptable DC offset
    
    def __init__(self, filepath: str, verbose: bool = False):
        self.filepath = Path(filepath)
        self.verbose = verbose
        self.errors = []
        self.warnings = []
        self.info = {}
        
    def log(self, msg: str):
        """Print message if verbose mode is enabled."""
        if self.verbose:
            print(f"  {msg}")
    
    def add_error(self, msg: str):
        """Add an error message."""
        self.errors.append(msg)
        print(f"[ERROR] {msg}")
    
    def add_warning(self, msg: str):
        """Add a warning message."""
        self.warnings.append(msg)
        print(f"[WARN]  {msg}")
    
    def add_info(self, key: str, value):
        """Store info about the file."""
        self.info[key] = value
        self.log(f"{key}: {value}")
    
    def check_file_exists(self) -> bool:
        """Check if file exists and is readable."""
        if not self.filepath.exists():
            self.add_error(f"File not found: {self.filepath}")
            return False
        if not self.filepath.is_file():
            self.add_error(f"Not a file: {self.filepath}")
            return False
        if self.filepath.stat().st_size == 0:
            self.add_error(f"File is empty: {self.filepath}")
            return False
        self.add_info("file_size", f"{self.filepath.stat().st_size:,} bytes")
        return True
    
    def load_audio(self):
        """Load audio file and extract metadata."""
        if HAS_SOUNDFILE:
            return self._load_with_soundfile()
        elif HAS_WAVE and self.filepath.suffix.lower() == '.wav':
            return self._load_with_wave()
        else:
            self.add_error("No audio library available. Install soundfile: pip install soundfile")
            return None, None
    
    def _load_with_soundfile(self):
        """Load audio using soundfile library."""
        try:
            data, sample_rate = sf.read(str(self.filepath))
            info = sf.info(str(self.filepath))
            self.add_info("format", info.format)
            self.add_info("subtype", info.subtype)
            self.add_info("sample_rate", sample_rate)
            self.add_info("channels", info.channels)
            self.add_info("duration", f"{info.duration:.2f} seconds")
            self.add_info("frames", info.frames)
            return data, sample_rate
        except Exception as e:
            self.add_error(f"Failed to load audio: {e}")
            return None, None
    
    def _load_with_wave(self):
        """Load WAV file using built-in wave module."""
        try:
            with wave.open(str(self.filepath), 'rb') as wf:
                channels = wf.getnchannels()
                sample_rate = wf.getframerate()
                frames = wf.getnframes()
                sample_width = wf.getsampwidth()
                
                self.add_info("format", "WAV")
                self.add_info("sample_rate", sample_rate)
                self.add_info("channels", channels)
                self.add_info("frames", frames)
                self.add_info("duration", f"{frames / sample_rate:.2f} seconds")
                self.add_info("bit_depth", sample_width * 8)
                
                if HAS_NUMPY:
                    raw_data = wf.readframes(frames)
                    if sample_width == 2:
                        data = np.frombuffer(raw_data, dtype=np.int16)
                        data = data.astype(np.float32) / 32768.0
                    elif sample_width == 3:
                        # 24-bit audio
                        data = np.zeros(frames * channels, dtype=np.float32)
                        for i in range(frames * channels):
                            sample = int.from_bytes(raw_data[i*3:(i+1)*3], 'little', signed=True)
                            data[i] = sample / 8388608.0
                    elif sample_width == 4:
                        data = np.frombuffer(raw_data, dtype=np.int32)
                        data = data.astype(np.float32) / 2147483648.0
                    else:
                        self.add_warning(f"Unsupported sample width: {sample_width}")
                        return None, sample_rate
                    
                    data = data.reshape(-1, channels)
                    return data, sample_rate
                else:
                    return None, sample_rate
                    
        except Exception as e:
            self.add_error(f"Failed to load WAV: {e}")
            return None, None
    
    def validate_channels(self) -> bool:
        """Check channel count matches Mach1 Spatial requirements."""
        channels = self.info.get('channels')
        if channels is None:
            return True  # Can't validate without info
        
        if channels != self.EXPECTED_CHANNELS:
            self.add_error(
                f"Channel count mismatch: got {channels}, "
                f"expected {self.EXPECTED_CHANNELS} for Mach1 Spatial"
            )
            return False
        
        self.log(f"Channel count OK: {channels}")
        return True
    
    def validate_sample_rate(self) -> bool:
        """Check sample rate is standard."""
        sample_rate = self.info.get('sample_rate')
        if sample_rate is None:
            return True
        
        if sample_rate not in self.EXPECTED_SAMPLE_RATES:
            self.add_warning(
                f"Non-standard sample rate: {sample_rate}. "
                f"Expected one of: {self.EXPECTED_SAMPLE_RATES}"
            )
            return True  # Warning only
        
        self.log(f"Sample rate OK: {sample_rate}")
        return True
    
    def validate_audio_content(self, data) -> bool:
        """Validate audio content for issues."""
        if data is None or not HAS_NUMPY:
            return True  # Can't validate without data/numpy
        
        # Check for silence
        peak = np.max(np.abs(data))
        self.add_info("peak_level", f"{peak:.6f} ({20 * np.log10(peak + 1e-10):.1f} dB)")
        
        if peak < 1e-6:
            self.add_warning("Audio appears to be silent")
        
        # Check for clipping
        clip_threshold = 0.99
        clipped_samples = np.sum(np.abs(data) > clip_threshold)
        if clipped_samples > 0:
            clip_percentage = (clipped_samples / data.size) * 100
            self.add_warning(
                f"Potential clipping detected: {clipped_samples} samples "
                f"({clip_percentage:.4f}%)"
            )
        
        # Check DC offset per channel
        for ch in range(data.shape[1]):
            dc_offset = np.mean(data[:, ch])
            if abs(dc_offset) > self.MAX_DC_OFFSET:
                self.add_warning(
                    f"DC offset on channel {ch + 1}: {dc_offset:.6f}"
                )
        
        # Check RMS levels per channel
        for ch in range(data.shape[1]):
            rms = np.sqrt(np.mean(data[:, ch] ** 2))
            rms_db = 20 * np.log10(rms + 1e-10)
            self.log(f"Channel {ch + 1} RMS: {rms_db:.1f} dB")
        
        return True
    
    def validate(self) -> bool:
        """Run all validation checks."""
        print(f"\nValidating: {self.filepath}")
        print("-" * 60)
        
        # Check file exists
        if not self.check_file_exists():
            return False
        
        # Load audio
        data, sample_rate = self.load_audio()
        
        # Run validations
        valid = True
        valid &= self.validate_channels()
        valid &= self.validate_sample_rate()
        valid &= self.validate_audio_content(data)
        
        # Summary
        print("-" * 60)
        if self.errors:
            print(f"FAILED: {len(self.errors)} error(s)")
            valid = False
        elif self.warnings:
            print(f"PASSED with {len(self.warnings)} warning(s)")
        else:
            print("PASSED: All checks OK")
        
        return valid


def main():
    parser = argparse.ArgumentParser(
        description="Validate audio files for Mach1 Spatial compatibility",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    %(prog)s render.wav
    %(prog)s --verbose *.wav
    %(prog)s --channels 8 --sample-rate 48000 output.wav

Requirements:
    - Python 3.6+
    - soundfile (recommended): pip install soundfile
    - numpy (optional, for detailed analysis): pip install numpy
"""
    )
    
    parser.add_argument(
        "files",
        nargs="+",
        help="Audio file(s) to validate"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose output"
    )
    parser.add_argument(
        "--channels",
        type=int,
        default=8,
        help="Expected channel count (default: 8 for Mach1 Spatial)"
    )
    parser.add_argument(
        "--sample-rate",
        type=int,
        help="Expected sample rate (validates if specified)"
    )
    
    args = parser.parse_args()
    
    # Check dependencies
    if not HAS_SOUNDFILE and not HAS_WAVE:
        print("Warning: No audio library available.")
        print("Install soundfile for best results: pip install soundfile")
    
    if not HAS_NUMPY:
        print("Warning: numpy not available. Some checks will be skipped.")
        print("Install numpy for full analysis: pip install numpy")
    
    # Validate files
    all_passed = True
    for filepath in args.files:
        validator = AudioValidator(filepath, verbose=args.verbose)
        
        # Override defaults if specified
        if args.channels:
            validator.EXPECTED_CHANNELS = args.channels
        if args.sample_rate:
            validator.EXPECTED_SAMPLE_RATES = [args.sample_rate]
        
        if not validator.validate():
            all_passed = False
    
    # Exit code
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
